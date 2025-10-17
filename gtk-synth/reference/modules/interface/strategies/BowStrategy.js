// BowStrategy.js
// Bow interface: Stick-slip friction with controllable bite and noise

import { InterfaceStrategy } from '../InterfaceStrategy.js';
import { fastTanh } from '../utils/NonlinearityLib.js';
import { whiteNoise } from '../utils/ExcitationGen.js';

export class BowStrategy extends InterfaceStrategy {
    constructor(sampleRate) {
        super(sampleRate);

        // Bow state
        this.bowState = 0;
    }

    /**
     * Process one sample through bow interface
     * Stick-slip friction model
     * @param {number} input - Input sample
     * @returns {number} Processed output
     */
    process(input) {
        const bowVelocity = this.intensity * 0.9 + 0.2;
        const slip = input - this.bowState;
        const friction = fastTanh(slip * (6 + this.intensity * 12));
        const grit = whiteNoise(this.intensity * 0.012);
        const output = friction * (0.55 + this.intensity * 0.35) + slip * 0.25 + grit;
        const stick = 0.8 - this.intensity * 0.25;
        this.bowState = this.bowState * stick + (input + friction * bowVelocity * 0.05) * (1 - stick);
        return Math.max(-1, Math.min(1, output));
    }

    /**
     * Reset state
     */
    reset() {
        this.bowState = 0;
    }

    /**
     * Called on note-on trigger
     */
    onNoteOn() {
        this.reset();
    }
}
