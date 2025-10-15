// FilterModule.js
// State-variable filter with morphable response (LP/BP/HP)

export class FilterModule {
    constructor(sampleRate = 44100) {
        this.sampleRate = sampleRate;

        // Filter parameters
        this.frequency = 1000;  // Hz
        this.q = 1.0;           // Quality factor
        this.shape = 0.0;       // 0=LP, 0.5=BP, 1=HP

        // State variables
        this.low = 0;
        this.band = 0;
        this.high = 0;

        // Previous values for stability
        this.prevLow = 0;
        this.prevBand = 0;
    }

    /**
     * Set filter frequency
     * @param {number} value - Normalized 0-1, maps to 20-20000 Hz (exponential)
     */
    setFrequency(value) {
        // Exponential mapping for perceptually linear frequency control
        this.frequency = 20 * Math.pow(1000, value);
    }

    /**
     * Set filter Q (resonance)
     * @param {number} value - Normalized 0-1, maps to 0.5-20 (exponential)
     */
    setQ(value) {
        // Exponential mapping for Q
        this.q = 0.5 * Math.pow(40, value);
    }

    /**
     * Set filter shape
     * @param {number} value - Normalized 0-1
     *                         0.0 = Lowpass
     *                         0.5 = Bandpass
     *                         1.0 = Highpass
     */
    setShape(value) {
        this.shape = Math.max(0, Math.min(1, value));
    }

    /**
     * Process one sample through the state-variable filter
     * @param {number} input - Input sample
     * @returns {number} Filtered output
     */
    process(input) {
        // Calculate filter coefficients
        const f = 2 * Math.sin(Math.PI * this.frequency / this.sampleRate);
        const qInv = 1 / Math.max(0.5, this.q);

        // State-variable filter equations
        // These equations implement a resonant filter in three outputs
        this.low = this.low + f * this.band;
        this.high = input - this.low - qInv * this.band;
        this.band = f * this.high + this.band;

        // Stability check: prevent NaN or infinity
        if (!isFinite(this.low)) this.low = 0;
        if (!isFinite(this.band)) this.band = 0;
        if (!isFinite(this.high)) this.high = 0;

        // Morph between filter responses based on shape parameter
        let output;

        if (this.shape < 0.5) {
            // Blend lowpass → bandpass (0.0 to 0.5)
            const mix = this.shape * 2;
            output = this.low * (1 - mix) + this.band * mix;
        } else {
            // Blend bandpass → highpass (0.5 to 1.0)
            const mix = (this.shape - 0.5) * 2;
            output = this.band * (1 - mix) + this.high * mix;
        }

        return output;
    }

    /**
     * Reset state (called on note-on)
     */
    reset() {
        this.low = 0;
        this.band = 0;
        this.high = 0;
        this.prevLow = 0;
        this.prevBand = 0;
    }
}
