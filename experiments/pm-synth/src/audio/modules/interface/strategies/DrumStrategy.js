// DrumStrategy.js
// Drum interface: Energy accumulator with noisy drive

import { InterfaceStrategy } from '../InterfaceStrategy.js';
import { whiteNoise } from '../utils/ExcitationGen.js';

export class DrumStrategy extends InterfaceStrategy {
    constructor(sampleRate) {
        super(sampleRate);

        // Energy state
        this.drumEnergy = 0;
    }

    /**
     * Process one sample through drum interface
     * Energy accumulation with percussive drive
     * @param {number} input - Input sample
     * @returns {number} Processed output
     */
    process(input) {
        const drive = 1.2 + this.intensity * 2.2;
        const noise = whiteNoise(0.02 + this.intensity * 0.06);

        // Accumulate energy with decay
        this.drumEnergy = this.drumEnergy * (0.7 - this.intensity * 0.2) +
                         Math.abs(input) * (0.6 + this.intensity * 0.7);

        const hit = Math.tanh(input * drive) + noise;
        const output = hit * (0.4 + this.intensity * 0.4) +
                      Math.sign(hit) * Math.min(0.8, this.drumEnergy * 0.6);

        return Math.max(-1, Math.min(1, output));
    }

    /**
     * Reset state
     */
    reset() {
        this.drumEnergy = 0;
    }

    /**
     * Called on note-on trigger
     */
    onNoteOn() {
        this.reset();
    }
}
