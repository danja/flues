// CrystalStrategy.js
// Crystal interface: Idealized inharmonic resonator with cross-coupling

import { InterfaceStrategy } from '../InterfaceStrategy.js';
import { cubicWaveshaper } from '../utils/NonlinearityLib.js';

export class CrystalStrategy extends InterfaceStrategy {
    constructor(sampleRate) {
        super(sampleRate);

        // Three phase accumulators for inharmonic partials
        this.phase1 = 0;
        this.phase2 = 0;
        this.phase3 = 0;

        // Golden ratio and its powers for inharmonic spacing
        this.PHI = 1.618033988749895;
        this.PHI2 = this.PHI * this.PHI;
        this.PHI3 = this.PHI2 * this.PHI;
    }

    /**
     * Process one sample through crystal interface
     * Creates inharmonic spectrum via cross-coupling of delay taps
     * @param {number} input - Input sample
     * @returns {number} Processed output
     */
    process(input) {
        // Store input in phase accumulators (acting as delay taps)
        // Each "phase" is actually tracking the signal at different time scales
        this.phase1 = this.phase1 * 0.98 + input;
        this.phase2 = this.phase2 * 0.95 + input * this.PHI;
        this.phase3 = this.phase3 * 0.92 + input * this.PHI2;

        // Create inharmonic partials by modulating input with delayed versions
        const p1 = input * (1 + this.phase1 * 0.3);
        const p2 = input * (1 + this.phase2 * 0.3);
        const p3 = input * (1 + this.phase3 * 0.3);

        // Cross-coupling creates sum/difference tones (ring modulation)
        const crossCoupling = this.intensity * 0.3;
        const coupled = (p1 + p2 + p3) * 0.33 +
                       crossCoupling * (p1 * p2 + p2 * p3 + p1 * p3) * 0.1;

        // Apply nonlinearity for additional harmonics
        const output = cubicWaveshaper(coupled, this.intensity * 0.2);

        return Math.max(-1, Math.min(1, output));
    }

    /**
     * Reset state
     */
    reset() {
        this.phase1 = 0;
        this.phase2 = 0;
        this.phase3 = 0;
    }

    /**
     * Called on note-on trigger
     */
    onNoteOn() {
        this.reset();
    }
}
