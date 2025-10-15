// PMSynthEngine.js
// Physical Modeling Synthesizer Engine - integrates all modules

import { SourcesModule } from './modules/SourcesModule.js';
import { EnvelopeModule } from './modules/EnvelopeModule.js';
import { InterfaceModule, InterfaceType } from './modules/InterfaceModule.js';
import { DelayLinesModule } from './modules/DelayLinesModule.js';
import { FeedbackModule } from './modules/FeedbackModule.js';
import { FilterModule } from './modules/FilterModule.js';
import { ModulationModule } from './modules/ModulationModule.js';
import { ReverbModule } from './modules/ReverbModule.js';

export { InterfaceType };

export class PMSynthEngine {
    constructor(sampleRate = 44100) {
        this.sampleRate = sampleRate;

        // Instantiate all modules
        this.sources = new SourcesModule(sampleRate);
        this.envelope = new EnvelopeModule(sampleRate);
        this.interface = new InterfaceModule();
        this.delayLines = new DelayLinesModule(sampleRate);
        this.feedback = new FeedbackModule();
        this.filter = new FilterModule(sampleRate);
        this.modulation = new ModulationModule(sampleRate);
        this.reverb = new ReverbModule(sampleRate);

        // Current state
        this.frequency = 440;
        this.gate = false;
        this.isPlaying = false;

        // Output scaling
        this.outputGain = 0.5;
    }

    /**
     * Note on - trigger the synth
     * @param {number} frequency - Note frequency in Hz
     */
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

        // Set gate
        this.envelope.setGate(true);
    }

    /**
     * Note off - release the synth
     */
    noteOff() {
        this.gate = false;
        this.envelope.setGate(false);
    }

    /**
     * Process one sample
     * @returns {number} Output sample
     */
    process() {
        if (!this.isPlaying) return 0;

        // Get modulation values
        const mod = this.modulation.process();

        // Apply FM to frequency
        const modulatedFreq = this.frequency * mod.fm;

        // Generate sources (DC, Noise, Tone)
        const sourceSignal = this.sources.process(modulatedFreq);

        // Apply envelope
        const env = this.envelope.process();
        if (env <= 0.001) {
            this.isPlaying = false;
            return 0;
        }
        const envelopedSignal = sourceSignal * env;

        // Apply interface nonlinearity
        const interfaceOutput = this.interface.process(envelopedSignal);

        // Get delay line outputs (they need feedback input)
        const delayOutputs = this.delayLines.process(0, modulatedFreq);

        // Mix feedback from delay lines and filter
        // Note: We need the filter output, so we'll do this in two passes
        // First pass: use previous filter output (slight delay)
        const feedbackSignal = this.feedback.process(
            delayOutputs.delay1,
            delayOutputs.delay2,
            0  // Will add filter feedback in next iteration
        );

        // Combine interface output with feedback
        const delayInput = interfaceOutput + feedbackSignal;

        // Now process through delay lines with the actual input
        const finalDelayOutputs = this.delayLines.process(delayInput, modulatedFreq);

        // Mix delay outputs (simple average)
        const delayMix = (finalDelayOutputs.delay1 + finalDelayOutputs.delay2) * 0.5;

        // Apply filter
        const filterOutput = this.filter.process(delayMix);

        // Apply AM
        const preReverbOutput = filterOutput * mod.am * this.outputGain;

        // Apply reverb (at end of signal path)
        const output = this.reverb.process(preReverbOutput);

        return output;
    }

    // ===== Parameter Setters =====

    // Sources
    setDCLevel(value) { this.sources.setDCLevel(value); }
    setNoiseLevel(value) { this.sources.setNoiseLevel(value); }
    setToneLevel(value) { this.sources.setToneLevel(value); }

    // Envelope
    setAttack(value) { this.envelope.setAttack(value); }
    setRelease(value) { this.envelope.setRelease(value); }

    // Interface
    setInterfaceType(type) { this.interface.setType(type); }
    setInterfaceIntensity(value) { this.interface.setIntensity(value); }

    // Delay Lines
    setTuning(value) { this.delayLines.setTuning(value); }
    setRatio(value) { this.delayLines.setRatio(value); }

    // Feedback
    setDelay1Feedback(value) { this.feedback.setDelay1Gain(value); }
    setDelay2Feedback(value) { this.feedback.setDelay2Gain(value); }
    setFilterFeedback(value) { this.feedback.setFilterGain(value); }

    // Filter
    setFilterFrequency(value) { this.filter.setFrequency(value); }
    setFilterQ(value) { this.filter.setQ(value); }
    setFilterShape(value) { this.filter.setShape(value); }

    // Modulation
    setLFOFrequency(value) { this.modulation.setFrequency(value); }
    setModulationTypeLevel(value) { this.modulation.setTypeLevel(value); }

    // Reverb
    setReverbSize(value) { this.reverb.setSize(value); }
    setReverbLevel(value) { this.reverb.setLevel(value); }

    /**
     * Get current playing status
     * @returns {boolean}
     */
    getIsPlaying() {
        return this.isPlaying;
    }
}
