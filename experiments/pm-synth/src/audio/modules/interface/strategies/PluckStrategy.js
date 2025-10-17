// PluckStrategy.js
// Pluck interface: One-way damping with transient brightening

import { InterfaceStrategy } from '../InterfaceStrategy.js';

export class PluckStrategy extends InterfaceStrategy {
    constructor(sampleRate) {
        super(sampleRate);

        // State variables
        this.lastPeak = 0;
        this.peakDecay = 0.999;
        this.prevInput = 0;
    }

    /**
     * Process one sample through pluck interface
     * One-way filter: pass initial impulse, dampen subsequent
     * @param {number} input - Input sample
     * @returns {number} Processed output
     */
    process(input) {
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
     * Reset state
     */
    reset() {
        this.lastPeak = 0;
        this.prevInput = 0;
    }

    /**
     * Called on note-on trigger
     */
    onNoteOn() {
        this.reset();
    }
}
