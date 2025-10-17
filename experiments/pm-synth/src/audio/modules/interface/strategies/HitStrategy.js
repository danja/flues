// HitStrategy.js
// Hit interface: Sharp waveshaper with adjustable hardness

import { InterfaceStrategy } from '../InterfaceStrategy.js';
import { sineFold } from '../utils/NonlinearityLib.js';

export class HitStrategy extends InterfaceStrategy {
    constructor(sampleRate) {
        super(sampleRate);
    }

    /**
     * Process one sample through hit interface
     * Sine-fold waveshaping for percussive strikes
     * @param {number} input - Input sample
     * @returns {number} Processed output
     */
    process(input) {
        const drive = 2 + this.intensity * 8;
        const folded = sineFold(input, drive);
        const hardness = 0.35 + this.intensity * 0.55;
        const shaped = Math.sign(folded) * Math.pow(Math.abs(folded), hardness);
        return Math.max(-1, Math.min(1, shaped));
    }

    /**
     * Reset state (stateless, but keep for interface consistency)
     */
    reset() {
        // No state to reset
    }
}
