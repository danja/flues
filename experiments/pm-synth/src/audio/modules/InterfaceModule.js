// InterfaceModule.js
// Physical interface modeling: Pluck, Hit, Reed, Flute, Brass

export const InterfaceType = {
    PLUCK: 0,
    HIT: 1,
    REED: 2,
    FLUTE: 3,
    BRASS: 4,
    BOW: 5,
    BELL: 6,
    DRUM: 7
};

export class InterfaceModule {
    constructor() {
        this.type = InterfaceType.REED;  // Default to reed
        this.intensity = 0.5;

        // State for pluck mode
        this.lastPeak = 0;
        this.peakDecay = 0.999;
        this.prevInput = 0;

        // State for bow/bell/drum
        this.bowState = 0;
        this.bellPhase = 0;
        this.drumEnergy = 0;

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
        const brightness = 0.2 + this.intensity * 0.45;
        let response;

        if (Math.abs(input) > Math.abs(this.lastPeak)) {
            // Let the first spike through but brighten it slightly
            this.lastPeak = input;
            response = input;
        } else {
            this.lastPeak *= this.peakDecay;
            const transient = (input - this.prevInput) * brightness;
            const damp = 0.35 + (1 - this.intensity) * 0.45;
            response = input * damp + transient;
        }

        this.prevInput = input;
        return Math.max(-1, Math.min(1, response));
    }

    /**
     * Process hit interface
     * Sharp waveshaper with adjustable hardness
     */
    processHit(input) {
        const drive = 2 + this.intensity * 8;
        const folded = Math.sin(input * drive * Math.PI * 0.5);
        const hardness = 0.35 + this.intensity * 0.55;
        const shaped = Math.sign(folded) * Math.pow(Math.abs(folded), hardness);
        return Math.max(-1, Math.min(1, shaped));
    }

    /**
     * Process reed interface
     * Clarinet-style cubic nonlinearity
     */
    processReed(input) {
        const stiffness = 2.5 + this.intensity * 10;
        const bias = (this.intensity - 0.5) * 0.25;
        const excited = (input + bias) * stiffness;
        const core = this.fastTanh(excited);
        const gain = 0.6 + this.intensity * 0.5;
        return Math.max(-1, Math.min(1, core * gain - bias * 0.3));
    }

    /**
     * Process flute interface
     * Soft symmetric nonlinearity (jet instability)
     */
    processFlute(input) {
        const softness = 0.45 + this.intensity * 0.4;
        const breath = (Math.random() * 2 - 1) * this.intensity * 0.04;
        const mixed = (input + breath) * softness;
        const shaped = mixed - (mixed * mixed * mixed) * 0.35;
        return Math.max(-0.49, Math.min(0.49, shaped));
    }

    /**
     * Process brass interface
     * Asymmetric lip model (different + and - slopes)
     */
    processBrass(input) {
        const drive = 1.5 + this.intensity * 5;
        const asymmetry = 0.4 + this.intensity * 0.5;
        let shaped;

        if (input >= 0) {
            const lifted = input * drive + (0.2 + this.intensity * 0.35);
            shaped = this.fastTanh(Math.max(lifted, 0));
        } else {
            const compressed = -input * (drive * (0.4 + this.intensity * 0.4));
            shaped = -Math.pow(Math.min(compressed, 1.5), 1.3) * (0.35 + (1 - this.intensity) * 0.25);
        }

        const buzz = this.fastTanh(shaped * (1.2 + this.intensity * 1.5));
        return Math.max(-1, Math.min(1, buzz + this.intensity * 0.05));
    }

    /**
     * Process bow interface
     * Stick-slip friction with controllable bite and noise
     */
    processBow(input) {
        const bowVelocity = this.intensity * 0.9 + 0.2;
        const slip = input - this.bowState;
        const friction = this.fastTanh(slip * (6 + this.intensity * 12));
        const grit = (Math.random() * 2 - 1) * this.intensity * 0.012;
        const output = friction * (0.55 + this.intensity * 0.35) + slip * 0.25 + grit;
        const stick = 0.8 - this.intensity * 0.25;
        this.bowState = this.bowState * stick + (input + friction * bowVelocity * 0.05) * (1 - stick);
        return Math.max(-1, Math.min(1, output));
    }

    /**
     * Process bell interface
     * Metallic waveshaping with evolving phase
     */
    processBell(input) {
        this.bellPhase += 0.1 + this.intensity * 0.25;
        if (this.bellPhase > Math.PI * 2) this.bellPhase -= Math.PI * 2;
        const harmonicSpread = 6 + this.intensity * 14;
        const even = Math.sin(input * harmonicSpread + this.bellPhase) * (0.4 + this.intensity * 0.4);
        const odd = Math.sin(input * (harmonicSpread * 0.5 + 2)) * (0.2 + this.intensity * 0.3);
        const bright = this.fastTanh((even + odd) * (1.1 + this.intensity * 0.6));
        return Math.max(-1, Math.min(1, bright));
    }

    /**
     * Process drum interface
     * Energy accumulator with noisy drive
     */
    processDrum(input) {
        const drive = 1.2 + this.intensity * 2.2;
        const noise = (Math.random() * 2 - 1) * (0.02 + this.intensity * 0.06);
        this.drumEnergy = this.drumEnergy * (0.7 - this.intensity * 0.2) + Math.abs(input) * (0.6 + this.intensity * 0.7);
        const hit = Math.tanh(input * drive) + noise;
        const output = hit * (0.4 + this.intensity * 0.4) + Math.sign(hit) * Math.min(0.8, this.drumEnergy * 0.6);
        return Math.max(-1, Math.min(1, output));
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
            case InterfaceType.BOW:
                return this.processBow(input);
            case InterfaceType.BELL:
                return this.processBell(input);
            case InterfaceType.DRUM:
                return this.processDrum(input);
            default:
                return input;
        }
    }

    /**
     * Reset state (called on note-on)
     */
    reset() {
        this.lastPeak = 0;
        this.prevInput = 0;
        this.bowState = 0;
        this.bellPhase = 0;
        this.drumEnergy = 0;
    }
}
