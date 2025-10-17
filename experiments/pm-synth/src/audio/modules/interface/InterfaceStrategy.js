// InterfaceStrategy.js
// Abstract base class for interface processing strategies

/**
 * Interface type enumeration
 */
export const InterfaceType = {
    PLUCK: 0,
    HIT: 1,
    REED: 2,
    FLUTE: 3,
    BRASS: 4,
    BOW: 5,
    BELL: 6,
    DRUM: 7,
    CRYSTAL: 8,
    VAPOR: 9,
    QUANTUM: 10,
    PLASMA: 11
};

/**
 * Abstract base class for all interface strategies
 * Defines the contract that all concrete strategies must implement
 */
export class InterfaceStrategy {
    constructor(sampleRate = 44100) {
        if (new.target === InterfaceStrategy) {
            throw new TypeError("Cannot instantiate abstract class InterfaceStrategy");
        }

        this.sampleRate = sampleRate;
        this.intensity = 0.5;  // Normalized 0-1 parameter
        this.gate = false;
        this.previousGate = false;
    }

    /**
     * Set intensity parameter
     * @param {number} value - Normalized 0-1
     */
    setIntensity(value) {
        this.intensity = Math.max(0, Math.min(1, value));
    }

    /**
     * Set gate state
     * @param {number} gate - Gate signal (0 or 1)
     */
    setGate(gate) {
        this.previousGate = this.gate;
        this.gate = gate > 0;

        // Trigger note-on event if gate transitions from off to on
        if (this.gate && !this.previousGate) {
            this.onNoteOn();
        }
    }

    /**
     * Process one sample through the interface
     * Must be implemented by concrete strategies
     * @param {number} input - Input sample
     * @returns {number} Processed output
     */
    process(input) {
        throw new Error("process() must be implemented by concrete strategy");
    }

    /**
     * Reset internal state
     * Must be implemented by concrete strategies
     */
    reset() {
        throw new Error("reset() must be implemented by concrete strategy");
    }

    /**
     * Called when note-on is triggered (gate goes high)
     * Can be overridden by strategies that need special note-on behavior
     */
    onNoteOn() {
        // Default: do nothing
        // Concrete strategies can override for specific initialization
    }

    /**
     * Get strategy name (for debugging/logging)
     * Can be overridden by concrete strategies
     * @returns {string} Strategy name
     */
    getName() {
        return this.constructor.name;
    }
}
