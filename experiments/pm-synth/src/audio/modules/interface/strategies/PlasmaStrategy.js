// PlasmaStrategy.js
// Plasma interface: Electromagnetic waveguide with nonlinear dispersion

import { InterfaceStrategy } from '../InterfaceStrategy.js';
import { AmplitudeTracker } from '../utils/EnergyTracker.js';
import { cubicWaveshaper } from '../utils/NonlinearityLib.js';

export class PlasmaStrategy extends InterfaceStrategy {
    constructor(sampleRate) {
        super(sampleRate);

        // Amplitude tracking for nonlinear dispersion
        this.ampTracker = new AmplitudeTracker(0.001, sampleRate);

        // Phase accumulator for self-focusing effects
        this.phase = 0;

        // Previous samples for allpass-style dispersion
        this.x1 = 0;
        this.y1 = 0;
    }

    /**
     * Process one sample through plasma interface
     * Amplitude-dependent phase modulation (self-focusing)
     * @param {number} input - Input sample
     * @returns {number} Processed output
     */
    process(input) {
        // Track amplitude for nonlinear effects
        const amplitude = this.ampTracker.process(input);

        // Self-focusing: high amplitude -> faster propagation
        const beta = this.intensity * 0.3;
        const phaseMod = 1 + beta * amplitude;

        this.phase += 0.1 * phaseMod;
        if (this.phase > Math.PI * 2) {
            this.phase -= Math.PI * 2;
        }

        // Amplitude-to-frequency conversion
        const freqMod = Math.sin(this.phase) * amplitude * this.intensity * 0.5;

        // Dispersive allpass filter with amplitude-dependent coefficient
        const allpassCoeff = 0.3 + amplitude * this.intensity * 0.4;
        const dispersed = allpassCoeff * input + this.x1 - allpassCoeff * this.y1;

        this.x1 = input;
        this.y1 = dispersed;

        // Add frequency modulation component
        const output = dispersed + freqMod;

        // Nonlinear harmonic generation at high intensities
        if (this.intensity > 0.5) {
            const nonlinear = cubicWaveshaper(output, (this.intensity - 0.5) * 0.4);
            return Math.max(-1, Math.min(1, nonlinear));
        }

        return Math.max(-1, Math.min(1, output));
    }

    /**
     * Reset state
     */
    reset() {
        this.ampTracker.reset();
        this.phase = 0;
        this.x1 = 0;
        this.y1 = 0;
    }

    /**
     * Called on note-on trigger
     */
    onNoteOn() {
        this.reset();
    }
}
