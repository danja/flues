// FeedbackModule.js
// Three-way feedback mixer with independent gains

export class FeedbackModule {
    constructor() {
        // Feedback gains (0-0.99 to prevent runaway)
        this.delay1Gain = 0.95;
        this.delay2Gain = 0.95;
        this.filterGain = 0.0;
    }

    /**
     * Set delay line 1 feedback gain
     * @param {number} value - Normalized 0-1, maps to 0-0.99
     */
    setDelay1Gain(value) {
        this.delay1Gain = value * 0.99;
    }

    /**
     * Set delay line 2 feedback gain
     * @param {number} value - Normalized 0-1, maps to 0-0.99
     */
    setDelay2Gain(value) {
        this.delay2Gain = value * 0.99;
    }

    /**
     * Set post-filter feedback gain
     * @param {number} value - Normalized 0-1, maps to 0-0.99
     */
    setFilterGain(value) {
        this.filterGain = value * 0.99;
    }

    /**
     * Mix feedback signals
     * @param {number} delay1Output - Output from delay line 1
     * @param {number} delay2Output - Output from delay line 2
     * @param {number} filterOutput - Output from filter
     * @returns {number} Mixed feedback signal
     */
    process(delay1Output, delay2Output, filterOutput) {
        return delay1Output * this.delay1Gain +
               delay2Output * this.delay2Gain +
               filterOutput * this.filterGain;
    }

    /**
     * Reset state (called on note-on)
     */
    reset() {
        // No internal state to reset
    }
}
