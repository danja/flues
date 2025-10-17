// BrassStrategy.js
// Brass interface: Asymmetric lip model with different positive/negative slopes

import { InterfaceStrategy } from '../InterfaceStrategy.js';
import { fastTanh } from '../utils/NonlinearityLib.js';

export class BrassStrategy extends InterfaceStrategy {
    constructor(sampleRate) {
        super(sampleRate);
    }

    /**
     * Process one sample through brass interface
     * Asymmetric lip buzz nonlinearity
     * @param {number} input - Input sample
     * @returns {number} Processed output
     */
    process(input) {
        const drive = 1.5 + this.intensity * 5;
        let shaped;

        if (input >= 0) {
            const lifted = input * drive + (0.2 + this.intensity * 0.35);
            shaped = fastTanh(Math.max(lifted, 0));
        } else {
            const compressed = -input * (drive * (0.4 + this.intensity * 0.4));
            shaped = -Math.pow(Math.min(compressed, 1.5), 1.3) * (0.35 + (1 - this.intensity) * 0.25);
        }

        const buzz = fastTanh(shaped * (1.2 + this.intensity * 1.5));
        return Math.max(-1, Math.min(1, buzz + this.intensity * 0.05));
    }

    /**
     * Reset state (stateless)
     */
    reset() {
        // No state to reset
    }
}
