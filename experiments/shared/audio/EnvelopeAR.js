// EnvelopeAR.js
// Attack-release envelope generator intended for reuse across synth projects.

export class EnvelopeAR {
    constructor({
        sampleRate = 44100,
        attackSeconds = 0.01,
        releaseSeconds = 0.05,
        minAttack = 0.001,
        maxAttack = 1.0,
        minRelease = 0.01,
        maxRelease = 3.0,
    } = {}) {
        this.sampleRate = sampleRate;
        this.minAttack = minAttack;
        this.maxAttack = maxAttack;
        this.minRelease = minRelease;
        this.maxRelease = maxRelease;

        this.attackSeconds = attackSeconds;
        this.releaseSeconds = releaseSeconds;

        this._value = 0;
        this._gate = false;
        this._active = false;
    }

    /**
     * Map a normalized value (0-1) to seconds using exponential scaling.
     */
    static exponentialMap(value, minSeconds, maxSeconds) {
        const clamped = Math.min(Math.max(value, 0), 1);
        return minSeconds * Math.pow(maxSeconds / minSeconds, clamped);
    }

    /**
     * Set attack in seconds.
     */
    setAttackSeconds(seconds) {
        const clamped = Math.min(Math.max(seconds, this.minAttack), this.maxAttack);
        this.attackSeconds = clamped;
    }

    /**
     * Set release in seconds.
     */
    setReleaseSeconds(seconds) {
        const clamped = Math.min(Math.max(seconds, this.minRelease), this.maxRelease);
        this.releaseSeconds = clamped;
    }

    /**
     * Set attack using a normalized (0-1) control value.
     */
    setAttackNormalized(value) {
        this.attackSeconds = EnvelopeAR.exponentialMap(value, this.minAttack, this.maxAttack);
    }

    /**
     * Set release using a normalized (0-1) control value.
     */
    setReleaseNormalized(value) {
        this.releaseSeconds = EnvelopeAR.exponentialMap(value, this.minRelease, this.maxRelease);
    }

    /**
     * Toggle the gate state. True on note-on, false on note-off.
     */
    setGate(gateState) {
        this._gate = !!gateState;
        if (this._gate) {
            this._active = true;
        }
    }

    /**
     * Reset envelope to zero and mark active; useful on retrigger.
     */
    reset() {
        this._value = 0;
        this._active = true;
    }

    /**
     * Process a single sample and return the envelope value.
     */
    process() {
        if (this._gate) {
            const attackRate = 1.0 / Math.max(this.attackSeconds * this.sampleRate, 1);
            this._value += attackRate;
            if (this._value >= 1.0) {
                this._value = 1.0;
            }
        } else {
            const releaseRate = 1.0 / Math.max(this.releaseSeconds * this.sampleRate, 1);
            this._value -= releaseRate;
            if (this._value <= 0) {
                this._value = 0;
                this._active = false;
            }
        }

        return this._value;
    }

    /**
     * Current envelope value without processing.
     */
    get value() {
        return this._value;
    }

    /**
     * Whether the envelope is currently producing a non-zero output.
     */
    get isActive() {
        return this._active;
    }
}

