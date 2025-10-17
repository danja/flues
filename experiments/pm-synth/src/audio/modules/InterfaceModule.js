// InterfaceModule.js
// Physical interface modeling using Strategy pattern
// Delegates processing to concrete strategy implementations

import { InterfaceType } from './interface/InterfaceStrategy.js';
import { InterfaceFactory } from './interface/InterfaceFactory.js';

export { InterfaceType };

/**
 * Interface module - Context class for Strategy pattern
 * Manages interface type switching and delegates processing to strategies
 */
export class InterfaceModule {
    constructor(sampleRate = 44100) {
        this.sampleRate = sampleRate;
        this.currentType = InterfaceType.REED;

        // Create initial strategy
        this.strategy = InterfaceFactory.createStrategy(this.currentType, sampleRate);
    }

    /**
     * Set interface type
     * @param {number} type - InterfaceType enum value
     */
    setType(type) {
        if (!InterfaceFactory.isValidType(type)) {
            console.warn(`Invalid interface type: ${type}`);
            return;
        }

        // Only recreate strategy if type actually changed
        if (type !== this.currentType) {
            this.currentType = type;
            const oldIntensity = this.strategy.intensity;

            // Create new strategy
            this.strategy = InterfaceFactory.createStrategy(type, this.sampleRate);

            // Preserve intensity parameter
            this.strategy.setIntensity(oldIntensity);
        }
    }

    /**
     * Set intensity parameter
     * @param {number} value - Normalized 0-1
     */
    setIntensity(value) {
        this.strategy.setIntensity(value);
    }

    /**
     * Process one sample through the current interface strategy
     * @param {number} input - Input sample
     * @returns {number} Processed output
     */
    process(input) {
        return this.strategy.process(input);
    }

    /**
     * Reset internal state (called on note-on)
     */
    reset() {
        this.strategy.reset();
    }

    /**
     * Get current interface type
     * @returns {number} Current InterfaceType
     */
    getType() {
        return this.currentType;
    }

    /**
     * Get current intensity value
     * @returns {number} Current intensity (0-1)
     */
    getIntensity() {
        return this.strategy.intensity;
    }

    /**
     * Get current strategy name (for debugging)
     * @returns {string} Strategy name
     */
    getStrategyName() {
        return this.strategy.getName();
    }
}
