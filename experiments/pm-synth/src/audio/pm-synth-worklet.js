// pm-synth-worklet.js
// AudioWorklet processor for the PM Synth engine

// Import modules (worklet context)
import { SourcesModule } from './modules/SourcesModule.js';
import { EnvelopeModule } from './modules/EnvelopeModule.js';
import { InterfaceModule, InterfaceType } from './modules/InterfaceModule.js';
import { DelayLinesModule } from './modules/DelayLinesModule.js';
import { FeedbackModule } from './modules/FeedbackModule.js';
import { FilterModule } from './modules/FilterModule.js';
import { ModulationModule } from './modules/ModulationModule.js';
import { ReverbModule } from './modules/ReverbModule.js';

const sampleRate = globalThis.sampleRate;

class PMSynthWorkletProcessor extends AudioWorkletProcessor {
    constructor() {
        super();

        // Instantiate all modules
        this.sources = new SourcesModule(sampleRate);
        this.envelope = new EnvelopeModule(sampleRate);
        this.interface = new InterfaceModule();
        this.delayLines = new DelayLinesModule(sampleRate);
        this.feedback = new FeedbackModule();
        this.filter = new FilterModule(sampleRate);
        this.modulation = new ModulationModule(sampleRate);
        this.reverb = new ReverbModule(sampleRate);

        // State
        this.frequency = 440;
        this.gate = false;
        this.isPlaying = false;
        this.outputGain = 0.5;
        this.dcBlockerX1 = 0;
        this.dcBlockerY1 = 0;
        this.prevDelayOutputs = { delay1: 0, delay2: 0 };
        this.prevFilterOutput = 0;

        // Handle messages from main thread
        this.port.onmessage = (e) => {
            const { type, data } = e.data;

            switch (type) {
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

    noteOn(frequency) {
        this.frequency = frequency;
        this.gate = true;
        this.isPlaying = true;

        // Reset all modules
        this.sources.reset();
        this.envelope.reset();
        this.interface.reset();
        this.delayLines.reset();
        this.feedback.reset();
        this.filter.reset();
        this.modulation.reset();
        this.reverb.reset();
        this.dcBlockerX1 = 0;
        this.dcBlockerY1 = 0;
        this.prevDelayOutputs.delay1 = 0;
        this.prevDelayOutputs.delay2 = 0;
        this.prevFilterOutput = 0;

        // Set gate
        this.envelope.setGate(true);
    }

    noteOff() {
        this.gate = false;
        this.envelope.setGate(false);
    }

    setParameter(param, value) {
        switch (param) {
            // Sources
            case 'dcLevel':
                this.sources.setDCLevel(value);
                break;
            case 'noiseLevel':
                this.sources.setNoiseLevel(value);
                break;
            case 'toneLevel':
                this.sources.setToneLevel(value);
                break;

            // Envelope
            case 'attack':
                this.envelope.setAttack(value);
                break;
            case 'release':
                this.envelope.setRelease(value);
                break;

            // Interface
            case 'interfaceType':
                this.interface.setType(value);
                break;
            case 'interfaceIntensity':
                this.interface.setIntensity(value);
                break;

            // Delay Lines
            case 'tuning':
                this.delayLines.setTuning(value);
                break;
            case 'ratio':
                this.delayLines.setRatio(value);
                break;

            // Feedback
            case 'delay1Feedback':
                this.feedback.setDelay1Gain(value);
                break;
            case 'delay2Feedback':
                this.feedback.setDelay2Gain(value);
                break;
            case 'filterFeedback':
                this.feedback.setFilterGain(value);
                break;

            // Filter
            case 'filterFrequency':
                this.filter.setFrequency(value);
                break;
            case 'filterQ':
                this.filter.setQ(value);
                break;
            case 'filterShape':
                this.filter.setShape(value);
                break;

            // Modulation
            case 'lfoFrequency':
                this.modulation.setFrequency(value);
                break;
            case 'modulationTypeLevel':
                this.modulation.setTypeLevel(value);
                break;

            // Reverb
            case 'reverbSize':
                this.reverb.setSize(value);
                break;
            case 'reverbLevel':
                this.reverb.setLevel(value);
                break;
        }
    }

    processSample() {
        if (!this.isPlaying) return 0;

        // Get modulation values
        const mod = this.modulation.process();

        // Apply FM to frequency
        const modulatedFreq = this.frequency * mod.fm;

        // Generate sources (DC, Noise, Tone)
        const sourceSignal = this.sources.process(modulatedFreq);

        // Apply envelope
        const env = this.envelope.process();
        const envelopedSignal = sourceSignal * env;

        // Apply interface nonlinearity
        const interfaceOutput = this.interface.process(envelopedSignal);

        // Mix feedback from previous delay outputs and filter
        const feedbackSignal = this.feedback.process(
            this.prevDelayOutputs.delay1,
            this.prevDelayOutputs.delay2,
            this.prevFilterOutput
        );

        // Combine interface output with feedback and remove DC offset
        const rawDelayInput = interfaceOutput + feedbackSignal;
        const delayInput = this.dcBlock(rawDelayInput);
        const clampedDelayInput = Math.max(-1, Math.min(1, delayInput));

        // Process through delay lines with actual input
        const finalDelayOutputs = this.delayLines.process(clampedDelayInput, this.frequency);

        // Mix delay outputs
        const delayMix = (finalDelayOutputs.delay1 + finalDelayOutputs.delay2) * 0.5;

        // Apply filter
        const filterOutput = this.filter.process(delayMix);

        // Apply AM and output gain
        const preReverbOutput = filterOutput * mod.am * this.outputGain;

        // Apply reverb (at end of signal path)
        const output = this.reverb.process(preReverbOutput);

        // Store outputs for next iteration feedback
        this.prevDelayOutputs.delay1 = finalDelayOutputs.delay1;
        this.prevDelayOutputs.delay2 = finalDelayOutputs.delay2;
        this.prevFilterOutput = filterOutput;

        if (!this.envelope.isPlaying() && Math.abs(output) < 1e-5 &&
            Math.abs(this.prevDelayOutputs.delay1) < 1e-5 && Math.abs(this.prevDelayOutputs.delay2) < 1e-5) {
            this.isPlaying = false;
        }

        return output;
    }

    dcBlock(sample) {
        const y = sample - this.dcBlockerX1 + 0.995 * this.dcBlockerY1;
        this.dcBlockerX1 = sample;
        this.dcBlockerY1 = y;
        return y;
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

registerProcessor('pm-synth-worklet', PMSynthWorkletProcessor);
