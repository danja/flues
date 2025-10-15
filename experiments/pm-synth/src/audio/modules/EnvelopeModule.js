// EnvelopeModule.js
// Attack-Release envelope generator

export class EnvelopeModule {
    constructor(sampleRate = 44100) {
        this.sampleRate = sampleRate;

        // Envelope parameters (in seconds)
        this.attackTime = 0.01;   // 10ms default
        this.releaseTime = 0.05;  // 50ms default

        // State
        this.envelope = 0;
        this.gate = false;
        this.isActive = false;
    }

    /**
     * Set attack time
     * @param {number} value - Normalized 0-1, maps to 0.001-0.5s
     */
    setAttack(value) {
        // Exponential mapping for better control
        this.attackTime = 0.001 * Math.pow(500, value);
    }

    /**
     * Set release time
     * @param {number} value - Normalized 0-1, maps to 0.01-2.0s
     */
    setRelease(value) {
        // Exponential mapping for better control
        this.releaseTime = 0.01 * Math.pow(200, value);
    }

    /**
     * Trigger the envelope
     * @param {boolean} gateState - True for note-on, false for note-off
     */
    setGate(gateState) {
        this.gate = gateState;
        if (gateState) {
            this.isActive = true;
        }
    }

    /**
     * Process one sample
     * @returns {number} Current envelope value (0-1)
     */
    process() {
        if (this.gate) {
            // Attack phase
            const attackRate = 1.0 / (this.attackTime * this.sampleRate);
            this.envelope += attackRate;
            if (this.envelope > 1.0) {
                this.envelope = 1.0;
            }
        } else {
            // Release phase
            const releaseRate = 1.0 / (this.releaseTime * this.sampleRate);
            this.envelope -= releaseRate;
            if (this.envelope < 0) {
                this.envelope = 0;
                this.isActive = false;
            }
        }

        return this.envelope;
    }

    /**
     * Check if envelope is active
     * @returns {boolean} True if envelope is generating signal
     */
    isPlaying() {
        return this.isActive;
    }

    /**
     * Reset state (called on note-on)
     */
    reset() {
        this.envelope = 0;
        this.isActive = true;
    }
}
