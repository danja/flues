// ModulationModule.js
// LFO with bipolar AM/FM control

export class ModulationModule {
    constructor(sampleRate = 44100) {
        this.sampleRate = sampleRate;

        // LFO parameters
        this.lfoFrequency = 5;     // Hz
        this.lfoPhase = 0;

        // Modulation control (bipolar)
        // < 0.5: AM (amplitude modulation)
        // = 0.5: No modulation
        // > 0.5: FM (frequency modulation)
        this.typeLevel = 0.5;

        // Derived values
        this.amDepth = 0;    // 0-1
        this.fmDepth = 0;    // 0-1
    }

    /**
     * Set LFO frequency
     * @param {number} value - Normalized 0-1, maps to 0.1-20 Hz (exponential)
     */
    setFrequency(value) {
        // Exponential mapping
        this.lfoFrequency = 0.1 * Math.pow(200, value);
    }

    /**
     * Set modulation type and level (bipolar control)
     * @param {number} value - Normalized 0-1
     *                         0.0 = Max AM
     *                         0.5 = No modulation
     *                         1.0 = Max FM
     */
    setTypeLevel(value) {
        this.typeLevel = Math.max(0, Math.min(1, value));

        if (value < 0.5) {
            // AM mode: 0.0 -> 0.5 maps to 100% -> 0% AM
            this.amDepth = (0.5 - value) * 2;
            this.fmDepth = 0;
        } else {
            // FM mode: 0.5 -> 1.0 maps to 0% -> 100% FM
            this.amDepth = 0;
            this.fmDepth = (value - 0.5) * 2;
        }
    }

    /**
     * Process one sample and generate LFO
     * @returns {Object} {lfo: number, am: number, fm: number}
     *                   lfo: raw LFO value (-1 to +1)
     *                   am: amplitude multiplier (0-1)
     *                   fm: frequency multiplier (0.9-1.1)
     */
    process() {
        // Generate sine wave LFO
        const phaseIncrement = (this.lfoFrequency * 2 * Math.PI) / this.sampleRate;
        this.lfoPhase += phaseIncrement;

        if (this.lfoPhase > 2 * Math.PI) {
            this.lfoPhase -= 2 * Math.PI;
        }

        const lfo = Math.sin(this.lfoPhase);

        // Calculate AM multiplier (0-1 range, modulates amplitude)
        // When amDepth = 1, lfo varies from 0 to 1
        // When amDepth = 0, stays at 1
        const am = 1 - this.amDepth * 0.5 + (lfo * this.amDepth * 0.5);

        // Calculate FM multiplier (varies pitch)
        // When fmDepth = 1, lfo varies frequency by ±10%
        // When fmDepth = 0, stays at 1
        const fm = 1 + (lfo * this.fmDepth * 0.1);

        return { lfo, am, fm };
    }

    /**
     * Reset state (called on note-on)
     */
    reset() {
        this.lfoPhase = 0;
    }
}
