// ClarinetEngine.js
// Digital Waveguide Clarinet Synthesizer Engine

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
    OUTPUT_SCALE,
    INITIAL_EXCITATION_AMPLITUDE
} from '../constants.js';

export class ClarinetEngine {
    constructor(sampleRate = 44100) {
        this.sampleRate = sampleRate;

        // Waveguide parameters
        this.delayLine = null;
        this.delayLength = DEFAULT_DELAY_LENGTH;
        this.readPos = 0;
        this.writePos = 0;

        // Reed parameters
        this.breathPressure = DEFAULT_BREATH_PRESSURE;
        this.reedStiffness = DEFAULT_REED_STIFFNESS;
        this.noiseLevel = DEFAULT_NOISE_LEVEL;

        // Filter state variables
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

        // Current frequency
        this.frequency = DEFAULT_FREQUENCY;
        this.targetFrequency = DEFAULT_FREQUENCY;

        // State
        this.isPlaying = false;
    }

    // Initialize the delay line for a given frequency
    setFrequency(freq) {
        this.targetFrequency = freq;
        const newLength = Math.floor(this.sampleRate / freq);

        if (!this.delayLine || newLength !== this.delayLength) {
            this.delayLength = newLength;
            this.delayLine = new Float32Array(this.delayLength);
            this.delayLine.fill(0);
            this.readPos = 0;
            this.writePos = 0;
        }
    }

    // Nonlinear reed reflection function (sigmoid/tanh approximation)
    reedReflection(pressureDiff) {
        // Cubic approximation: u = a*Δp + b*Δp^2 - c*Δp^3
        // Simplified to tanh-like function for efficiency
        const stiffness = this.reedStiffness * REED_STIFFNESS_SCALE + REED_STIFFNESS_OFFSET;
        const scaled = pressureDiff * stiffness;

        // Fast tanh approximation
        if (scaled > TANH_CLIP_THRESHOLD) return 1;
        if (scaled < -TANH_CLIP_THRESHOLD) return -1;

        const x2 = scaled * scaled;
        return scaled * (TANH_NUMERATOR_CONSTANT + x2) / (TANH_NUMERATOR_CONSTANT + TANH_DENOMINATOR_SCALE * x2);
    }

    // One-pole lowpass filter
    lowpass(input, cutoff) {
        const a = cutoff;
        this.lpf.y1 = a * input + (1 - a) * this.lpf.y1;
        return this.lpf.y1;
    }

    // One-pole highpass filter
    highpass(input, cutoff) {
        const a = cutoff;
        const output = a * (this.hpf.y1 + input - this.hpf.x1);
        this.hpf.x1 = input;
        this.hpf.y1 = output;
        return output;
    }

    // Soft saturation (tanh approximation)
    saturate(input) {
        if (input > TANH_CLIP_THRESHOLD) return 1;
        if (input < -TANH_CLIP_THRESHOLD) return -1;
        const x2 = input * input;
        return input * (TANH_NUMERATOR_CONSTANT + x2) / (TANH_NUMERATOR_CONSTANT + TANH_DENOMINATOR_SCALE * x2);
    }

    // Generate noise for breath turbulence
    generateNoise() {
        return (Math.random() * 2 - 1) * this.noiseLevel;
    }

    // Update envelope
    updateEnvelope(deltaTime) {
        if (this.gate) {
            // Attack
            const attackRate = 1.0 / (this.attackTime * this.sampleRate);
            this.envelope += attackRate * deltaTime;
            if (this.envelope > 1) this.envelope = 1;
        } else {
            // Release
            const releaseRate = 1.0 / (this.releaseTime * this.sampleRate);
            this.envelope -= releaseRate * deltaTime;
            if (this.envelope < 0) {
                this.envelope = 0;
                this.isPlaying = false;
            }
        }
        return this.envelope;
    }

    // Process one sample
    process() {
        if (!this.delayLine || this.delayLength === 0) return 0;

        // Update envelope
        const env = this.updateEnvelope(1);

        if (env <= 0.001) return 0;

        // Vibrato modulation
        this.vibratoPhase += (this.vibratoRate * 2 * Math.PI) / this.sampleRate;
        if (this.vibratoPhase > 2 * Math.PI) this.vibratoPhase -= 2 * Math.PI;
        const vibratoMod = Math.sin(this.vibratoPhase) * this.vibratoAmount;

        // Calculate fractional delay for vibrato
        const baseDelay = this.sampleRate / this.targetFrequency;
        const modulatedDelay = baseDelay * (1 + vibratoMod);
        const delayInSamples = Math.min(modulatedDelay, this.delayLength - 1);

        // Read from delay line with linear interpolation
        const readPosFloat = this.writePos - delayInSamples;
        const readPosWrapped = (readPosFloat + this.delayLength) % this.delayLength;
        const readPosInt = Math.floor(readPosWrapped);
        const readPosFrac = readPosWrapped - readPosInt;
        const nextPos = (readPosInt + 1) % this.delayLength;

        const borePressure = this.delayLine[readPosInt] * (1 - readPosFrac) +
            this.delayLine[nextPos] * readPosFrac;

        // Generate excitation (breath + noise)
        const noise = this.generateNoise();
        const breath = this.breathPressure * env;
        const mouthPressure = breath + noise * env;

        // Reed nonlinearity
        const pressureDiff = mouthPressure - borePressure;
        const flow = this.reedReflection(pressureDiff);

        // Inject flow into bore
        let newSample = borePressure + flow * 0.5;

        // Apply loop filters
        newSample = this.lowpass(newSample, this.lpf.cutoff);
        newSample = this.highpass(newSample, this.hpf.cutoff);

        // Soft saturation to prevent runaway
        newSample = this.saturate(newSample * SATURATION_SCALE);

        // Write to delay line
        this.delayLine[this.writePos] = newSample;
        this.writePos = (this.writePos + 1) % this.delayLength;

        // Output with envelope
        return newSample * env * OUTPUT_SCALE;
    }

    // Note on
    noteOn(frequency) {
        this.setFrequency(frequency);
        this.gate = true;
        this.isPlaying = true;
        this.vibratoPhase = 0;

        // Small initial excitation to start oscillation
        if (this.delayLine) {
            for (let i = 0; i < this.delayLength; i++) {
                this.delayLine[i] = (Math.random() * 2 - 1) * INITIAL_EXCITATION_AMPLITUDE;
            }
        }
    }

    // Note off
    noteOff() {
        this.gate = false;
    }

    // Set parameters
    setBreath(value) { // 0-1
        this.breathPressure = value * 0.8 + 0.2;
    }

    setReedStiffness(value) { // 0-1
        this.reedStiffness = value;
    }

    setNoise(value) { // 0-1
        this.noiseLevel = value * 0.3;
    }

    setAttack(value) { // 0-1
        this.attackTime = value * 0.1 + 0.001;
    }

    setRelease(value) { // 0-1
        this.releaseTime = value * 0.3 + 0.01;
    }

    setDamping(value) { // 0-1
        // Lower values = more damping
        this.lpf.cutoff = 0.3 + value * 0.69;
    }

    setBrightness(value) { // 0-1
        // Higher values = brighter
        this.hpf.cutoff = value * 0.05;
    }

    setVibrato(value) { // 0-1
        this.vibratoAmount = value * 0.01;
    }
}
