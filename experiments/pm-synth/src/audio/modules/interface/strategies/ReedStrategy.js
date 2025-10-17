// ReedStrategy.js
// Reed interface: Clarinet-style cubic nonlinearity with bias

import { InterfaceStrategy } from '../InterfaceStrategy.js';
import { fastTanh } from '../utils/NonlinearityLib.js';

export class ReedStrategy extends InterfaceStrategy {
    constructor(sampleRate) {
        super(sampleRate);
    }

    /**
     * Process one sample through reed interface
     * Biased saturation for clarinet-like behavior
     * @param {number} input - Input sample
     * @returns {number} Processed output
     */
    process(input) {
        const stiffness = 2.5 + this.intensity * 10;
        const bias = (this.intensity - 0.5) * 0.25;
        const excited = (input + bias) * stiffness;
        const core = fastTanh(excited);
        const gain = 0.6 + this.intensity * 0.5;
        return Math.max(-1, Math.min(1, core * gain - bias * 0.3));
    }

    /**
     * Reset state (stateless)
     */
    reset() {
        // No state to reset
    }
}
