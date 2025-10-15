// InterfaceModule.js
// Physical interface modeling: Pluck, Hit, Reed, Flute, Brass

export const InterfaceType = {
    PLUCK: 0,
    HIT: 1,
    REED: 2,
    FLUTE: 3,
    BRASS: 4
};

export class InterfaceModule {
    constructor() {
        this.type = InterfaceType.REED;  // Default to reed
        this.intensity = 0.5;

        // State for pluck mode
        this.lastPeak = 0;
        this.peakDecay = 0.999;

        // Constants for fast tanh approximation
        this.TANH_CLIP_THRESHOLD = 3;
        this.TANH_NUMERATOR_CONSTANT = 27;
        this.TANH_DENOMINATOR_SCALE = 9;
    }

    /**
     * Set interface type
     * @param {number} type - InterfaceType enum value
     */
    setType(type) {
        if (type >= InterfaceType.PLUCK && type <= InterfaceType.BRASS) {
            this.type = type;
        }
    }

    /**
     * Set intensity parameter
     * @param {number} value - Normalized 0-1
     */
    setIntensity(value) {
        this.intensity = Math.max(0, Math.min(1, value));
    }

    /**
     * Fast tanh approximation
     * @param {number} x - Input value
     * @returns {number} Approximated tanh(x)
     */
    fastTanh(x) {
        if (x > this.TANH_CLIP_THRESHOLD) return 1;
        if (x < -this.TANH_CLIP_THRESHOLD) return -1;

        const x2 = x * x;
        return x * (this.TANH_NUMERATOR_CONSTANT + x2) /
               (this.TANH_NUMERATOR_CONSTANT + this.TANH_DENOMINATOR_SCALE * x2);
    }

    /**
     * Process pluck interface
     * One-way filter: pass initial impulse, dampen subsequent
     */
    processPluck(input) {
        const threshold = this.intensity * 0.5;

        // Detect peaks
        if (Math.abs(input) > Math.abs(this.lastPeak)) {
            this.lastPeak = input;
            return input;
        }

        // Decay peak tracking
        this.lastPeak *= this.peakDecay;

        // Dampen signal based on intensity
        return input * (1 - this.intensity * 0.7);
    }

    /**
     * Process hit interface
     * Sharp waveshaper with adjustable hardness
     */
    processHit(input) {
        const hardness = 1 + this.intensity * 10;
        const shaped = this.fastTanh(input * hardness);
        return shaped / Math.sqrt(hardness);
    }

    /**
     * Process reed interface
     * Clarinet-style cubic nonlinearity
     */
    processReed(input) {
        const stiffness = 0.8 + this.intensity * 8;
        return this.fastTanh(input * stiffness);
    }

    /**
     * Process flute interface
     * Soft symmetric nonlinearity (jet instability)
     */
    processFlute(input) {
        const softness = 1 + this.intensity * 3;
        // Polynomial approximation of soft saturation
        const normalized = input / softness;
        return normalized - (normalized * normalized * normalized) / 3;
    }

    /**
     * Process brass interface
     * Asymmetric lip model (different + and - slopes)
     */
    processBrass(input) {
        const asymmetry = 0.5 + this.intensity * 2;

        if (input > 0) {
            return this.fastTanh(input * asymmetry);
        } else {
            return this.fastTanh(input / asymmetry);
        }
    }

    /**
     * Process one sample through selected interface
     * @param {number} input - Input sample
     * @returns {number} Processed output
     */
    process(input) {
        switch (this.type) {
            case InterfaceType.PLUCK:
                return this.processPluck(input);
            case InterfaceType.HIT:
                return this.processHit(input);
            case InterfaceType.REED:
                return this.processReed(input);
            case InterfaceType.FLUTE:
                return this.processFlute(input);
            case InterfaceType.BRASS:
                return this.processBrass(input);
            default:
                return input;
        }
    }

    /**
     * Reset state (called on note-on)
     */
    reset() {
        this.lastPeak = 0;
    }
}
