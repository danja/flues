// FluteStrategy.js
// Flute interface: Soft symmetric nonlinearity with breath noise

import { InterfaceStrategy } from '../InterfaceStrategy.js';
import { whiteNoise } from '../utils/ExcitationGen.js';

export class FluteStrategy extends InterfaceStrategy {
    constructor(sampleRate) {
        super(sampleRate);
    }

    /**
     * Process one sample through flute interface
     * Soft jet instability with breath turbulence
     * @param {number} input - Input sample
     * @returns {number} Processed output
     */
    process(input) {
        const softness = 0.45 + this.intensity * 0.4;
        const breath = whiteNoise(this.intensity * 0.04);
        const mixed = (input + breath) * softness;
        const shaped = mixed - (mixed * mixed * mixed) * 0.35;
        return Math.max(-0.49, Math.min(0.49, shaped));
    }

    /**
     * Reset state (stateless)
     */
    reset() {
        // No state to reset
    }
}
