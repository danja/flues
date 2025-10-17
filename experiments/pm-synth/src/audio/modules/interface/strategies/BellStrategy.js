// BellStrategy.js
// Bell interface: Metallic waveshaping with evolving phase

import { InterfaceStrategy } from '../InterfaceStrategy.js';
import { fastTanh } from '../utils/NonlinearityLib.js';

export class BellStrategy extends InterfaceStrategy {
    constructor(sampleRate) {
        super(sampleRate);

        // Phase accumulator for metallic partials
        this.bellPhase = 0;
    }

    /**
     * Process one sample through bell interface
     * Metallic waveshaping with evolving harmonics
     * @param {number} input - Input sample
     * @returns {number} Processed output
     */
    process(input) {
        this.bellPhase += 0.1 + this.intensity * 0.25;
        if (this.bellPhase > Math.PI * 2) {
            this.bellPhase -= Math.PI * 2;
        }

        const harmonicSpread = 6 + this.intensity * 14;
        const even = Math.sin(input * harmonicSpread + this.bellPhase) * (0.4 + this.intensity * 0.4);
        const odd = Math.sin(input * (harmonicSpread * 0.5 + 2)) * (0.2 + this.intensity * 0.3);
        const bright = fastTanh((even + odd) * (1.1 + this.intensity * 0.6));
        return Math.max(-1, Math.min(1, bright));
    }

    /**
     * Reset state
     */
    reset() {
        this.bellPhase = 0;
    }

    /**
     * Called on note-on trigger
     */
    onNoteOn() {
        this.reset();
    }
}
