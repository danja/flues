// clarinet-worklet.js
// AudioWorklet processor for the clarinet engine

import {
    DEFAULT_BREATH_PRESSURE,
    DEFAULT_REED_STIFFNESS,
    DEFAULT_NOISE_LEVEL,
    DEFAULT_LPF_CUTOFF,
    DEFAULT_HPF_CUTOFF,
    DEFAULT_ATTACK_TIME,
    DEFAULT_RELEASE_TIME,
    DEFAULT_VIBRATO_AMOUNT,
    DEFAULT_VIBRATO_RATE,
    DEFAULT_FREQUENCY,
    DEFAULT_DELAY_LENGTH,
    REED_STIFFNESS_SCALE,
    REED_STIFFNESS_OFFSET,
    TANH_CLIP_THRESHOLD,
    TANH_NUMERATOR_CONSTANT,
    TANH_DENOMINATOR_SCALE,
    SATURATION_SCALE,
    INITIAL_EXCITATION_AMPLITUDE,
    BREATH_SCALE_WORKLET,
    BREATH_OFFSET_WORKLET,
    NOISE_SCALE,
    ATTACK_SCALE,
    ATTACK_OFFSET,
    RELEASE_SCALE,
    RELEASE_OFFSET,
    DAMPING_SCALE,
    DAMPING_OFFSET,
    BRIGHTNESS_SCALE_WORKLET,
    BRIGHTNESS_OFFSET_WORKLET,
    VIBRATO_SCALE,
    FLOW_MIX_WORKLET,
    WORKLET_OUTPUT_SCALE
} from '../constants.js';

const sampleRate = globalThis.sampleRate;

class ClarinetWorkletProcessor extends AudioWorkletProcessor {
    constructor() {
        super();

        this.sampleRate = sampleRate;
        this.delayLine = null;
        this.delayLength = DEFAULT_DELAY_LENGTH;
        this.writePos = 0;

        // Reed parameters
        this.breathPressure = DEFAULT_BREATH_PRESSURE;
        this.reedStiffness = DEFAULT_REED_STIFFNESS;
        this.noiseLevel = DEFAULT_NOISE_LEVEL;

        // Filter state
        this.lpf = { x1: 0, y1: 0, cutoff: DEFAULT_LPF_CUTOFF };
        this.hpf = { x1: 0, y1: 0, cutoff: DEFAULT_HPF_CUTOFF };

        // Envelope
        this.envelope = 0;
        this.attackTime = DEFAULT_ATTACK_TIME;
        this.releaseTime = DEFAULT_RELEASE_TIME;
        this.gate = false;

        // Vibrato
        this.vibratoAmount = DEFAULT_VIBRATO_AMOUNT;
        this.vibratoRate = DEFAULT_VIBRATO_RATE;
        this.vibratoPhase = 0;

        // Frequency
        this.frequency = DEFAULT_FREQUENCY;
        this.targetFrequency = DEFAULT_FREQUENCY;
        this.isPlaying = false;

        // Handle messages from main thread
        this.port.onmessage = (e) => {
            const { type, data } = e.data;

            switch(type) {
                case 'noteOn':
                    this.noteOn(data.frequency);
                    break;
                case 'noteOff':
                    this.noteOff();
                    break;
                case 'setParameter':
                    this.setParameter(data.param, data.value);
                    break;
            }
        };
    }

    setFrequency(freq) {
        this.targetFrequency = freq;
        const newLength = Math.floor(this.sampleRate / freq);

        if (!this.delayLine || newLength !== this.delayLength) {
            this.delayLength = newLength;
            this.delayLine = new Float32Array(this.delayLength);
            this.delayLine.fill(0);
            this.writePos = 0;
        }
    }

    reedReflection(pressureDiff) {
        const stiffness = this.reedStiffness * REED_STIFFNESS_SCALE + REED_STIFFNESS_OFFSET;
        const scaled = pressureDiff * stiffness;

        if (scaled > TANH_CLIP_THRESHOLD) return 1;
        if (scaled < -TANH_CLIP_THRESHOLD) return -1;

        const x2 = scaled * scaled;
        return scaled * (TANH_NUMERATOR_CONSTANT + x2) / (TANH_NUMERATOR_CONSTANT + TANH_DENOMINATOR_SCALE * x2);
    }

    lowpass(input, cutoff) {
        const a = cutoff;
        this.lpf.y1 = a * input + (1 - a) * this.lpf.y1;
        return this.lpf.y1;
    }

    highpass(input, cutoff) {
        // Simple DC blocker - only removes very low frequencies
        const a = 1.0 - cutoff;  // Inverted so higher cutoff = more highs
        const output = a * (this.hpf.y1 + input - this.hpf.x1);
        this.hpf.x1 = input;
        this.hpf.y1 = output;
        return output;
    }

    saturate(input) {
        if (input > TANH_CLIP_THRESHOLD) return 1;
        if (input < -TANH_CLIP_THRESHOLD) return -1;
        const x2 = input * input;
        return input * (TANH_NUMERATOR_CONSTANT + x2) / (TANH_NUMERATOR_CONSTANT + TANH_DENOMINATOR_SCALE * x2);
    }

    generateNoise() {
        return (Math.random() * 2 - 1) * this.noiseLevel;
    }

    updateEnvelope(deltaTime) {
        if (this.gate) {
            // attackTime is in seconds, need to convert to samples
            const attackRate = 1.0 / (this.attackTime * this.sampleRate);
            this.envelope += attackRate * deltaTime;
            if (this.envelope > 1) this.envelope = 1;
        } else {
            // releaseTime is in seconds, need to convert to samples
            const releaseRate = 1.0 / (this.releaseTime * this.sampleRate);
            this.envelope -= releaseRate * deltaTime;
            if (this.envelope < 0) {
                this.envelope = 0;
                this.isPlaying = false;
            }
        }
        return this.envelope;
    }

    processSample() {
        if (!this.delayLine || this.delayLength === 0) return 0;

        const env = this.updateEnvelope(1);
        if (env <= 0.001) return 0;

        // Vibrato
        this.vibratoPhase += (this.vibratoRate * 2 * Math.PI) / this.sampleRate;
        if (this.vibratoPhase > 2 * Math.PI) this.vibratoPhase -= 2 * Math.PI;
        const vibratoMod = Math.sin(this.vibratoPhase) * this.vibratoAmount;

        // Calculate delay with vibrato
        const baseDelay = this.sampleRate / this.targetFrequency;
        const modulatedDelay = baseDelay * (1 + vibratoMod);
        const delayInSamples = Math.min(modulatedDelay, this.delayLength - 1);

        // Read from delay line with interpolation
        const readPosFloat = this.writePos - delayInSamples;
        const readPosWrapped = (readPosFloat + this.delayLength) % this.delayLength;
        const readPosInt = Math.floor(readPosWrapped);
        const readPosFrac = readPosWrapped - readPosInt;
        const nextPos = (readPosInt + 1) % this.delayLength;

        const borePressure = this.delayLine[readPosInt] * (1 - readPosFrac) +
                            this.delayLine[nextPos] * readPosFrac;

        // Generate excitation
        const noise = this.generateNoise();
        const breath = this.breathPressure * env;
        const mouthPressure = breath + noise * env;

        // Reed nonlinearity
        const pressureDiff = mouthPressure - borePressure;
        const flow = this.reedReflection(pressureDiff);

        // Mix into bore - balanced for clarinet-like tone
        let newSample = borePressure + flow * FLOW_MIX_WORKLET;

        // Apply filters
        newSample = this.lowpass(newSample, this.lpf.cutoff);
        newSample = this.highpass(newSample, this.hpf.cutoff);

        // Soft saturation
        newSample = this.saturate(newSample * SATURATION_SCALE);

        // Write to delay line
        this.delayLine[this.writePos] = newSample;
        this.writePos = (this.writePos + 1) % this.delayLength;

        return newSample * env * WORKLET_OUTPUT_SCALE;
    }

    noteOn(frequency) {
        this.setFrequency(frequency);
        this.gate = true;
        this.isPlaying = true;
        this.vibratoPhase = 0;

        // Reset filter state to prevent DC buildup
        this.lpf.x1 = 0;
        this.lpf.y1 = 0;
        this.hpf.x1 = 0;
        this.hpf.y1 = 0;

        if (this.delayLine) {
            for (let i = 0; i < this.delayLength; i++) {
                this.delayLine[i] = (Math.random() * 2 - 1) * INITIAL_EXCITATION_AMPLITUDE;
            }
        }
    }

    noteOff() {
        this.gate = false;
    }

    setParameter(param, value) {
        switch(param) {
            case 'breath':
                // Inverted: lower internal pressure = more stable/louder oscillation
                this.breathPressure = (1 - value) * BREATH_SCALE_WORKLET + BREATH_OFFSET_WORKLET;
                break;
            case 'reed':
                this.reedStiffness = value;
                break;
            case 'noise':
                this.noiseLevel = value * NOISE_SCALE;
                break;
            case 'attack':
                this.attackTime = value * ATTACK_SCALE + ATTACK_OFFSET;
                break;
            case 'release':
                this.releaseTime = value * RELEASE_SCALE + RELEASE_OFFSET;
                break;
            case 'damping':
                this.lpf.cutoff = DAMPING_OFFSET + value * DAMPING_SCALE;
                break;
            case 'brightness':
                this.hpf.cutoff = BRIGHTNESS_OFFSET_WORKLET + value * BRIGHTNESS_SCALE_WORKLET;
                break;
            case 'vibrato':
                this.vibratoAmount = value * VIBRATO_SCALE;
                break;
        }
    }

    process(inputs, outputs, parameters) {
        const output = outputs[0];
        const channel = output[0];

        for (let i = 0; i < channel.length; i++) {
            channel[i] = this.processSample();
        }

        return true; // Keep processor alive
    }
}

registerProcessor('clarinet-worklet', ClarinetWorkletProcessor);
