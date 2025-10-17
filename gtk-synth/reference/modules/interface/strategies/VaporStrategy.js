// VaporStrategy.js
// Vapor interface: Chaotic aeroacoustic turbulence

import { InterfaceStrategy } from '../InterfaceStrategy.js';
import { ChaoticOscillator } from '../utils/ExcitationGen.js';
import { softClip } from '../utils/NonlinearityLib.js';

export class VaporStrategy extends InterfaceStrategy {
    constructor(sampleRate) {
        super(sampleRate);

        // Multiple chaotic oscillators with different seeds
        this.chaos1 = new ChaoticOscillator(3.7);
        this.chaos2 = new ChaoticOscillator(3.8);
        this.chaos3 = new ChaoticOscillator(3.9);

        // Previous outputs for feedback
        this.prev1 = 0;
        this.prev2 = 0;
    }

    /**
     * Process one sample through vapor interface
     * Chaotic turbulence with feedback coupling
     * @param {number} input - Input sample
     * @returns {number} Processed output
     */
    process(input) {
        // Map intensity to chaos parameter (3.57+ = chaotic)
        const r = 2.5 + this.intensity * 1.5;

        this.chaos1.setR(r);
        this.chaos2.setR(r + 0.1);
        this.chaos3.setR(r + 0.2);

        // Generate chaotic signals
        const c1 = this.chaos1.process(0.3);
        const c2 = this.chaos2.process(0.3);
        const c3 = this.chaos3.process(0.3);

        // Mix input with chaotic forcing
        const chaosAmount = this.intensity * 0.6;
        const inputAmount = 1 - chaosAmount * 0.5;

        const mixed = input * inputAmount + (c1 + c2 + c3) * chaosAmount;

        // Couple with feedback from previous samples
        const feedback = (this.prev1 * 0.3 + this.prev2 * 0.2) * chaosAmount;

        const turbulent = mixed + feedback;

        // Soft clip to prevent runaway
        const output = softClip(turbulent, 1.2);

        // Store for feedback
        this.prev2 = this.prev1;
        this.prev1 = output;

        return Math.max(-1, Math.min(1, output));
    }

    /**
     * Reset state
     */
    reset() {
        this.chaos1.reset();
        this.chaos2.reset();
        this.chaos3.reset();
        this.prev1 = 0;
        this.prev2 = 0;
    }

    /**
     * Called on note-on trigger
     */
    onNoteOn() {
        this.reset();
    }
}
