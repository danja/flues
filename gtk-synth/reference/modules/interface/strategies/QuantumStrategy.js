// QuantumStrategy.js
// Quantum interface: Amplitude-quantized resonator with zipper artifacts

import { InterfaceStrategy } from '../InterfaceStrategy.js';

export class QuantumStrategy extends InterfaceStrategy {
    constructor(sampleRate) {
        super(sampleRate);
    }

    /**
     * Process one sample through quantum interface
     * Reduces bit depth to create quantization artifacts
     * @param {number} input - Input sample
     * @returns {number} Processed output
     */
    process(input) {
        // Map intensity to bit depth (0.0 = 8-bit, 1.0 = 3-bit)
        const bitDepth = 8 - Math.floor(this.intensity * 5);
        const levels = Math.pow(2, bitDepth);

        // Quantize amplitude
        const quantized = Math.round(input * levels) / levels;

        // Add slight nonlinearity at quantization boundaries
        // Creates interesting harmonic distortion
        const nearBoundary = Math.abs(input * levels - Math.round(input * levels));
        const boundaryNoise = (nearBoundary > 0.45) ? (Math.random() * 2 - 1) * 0.01 * this.intensity : 0;

        const output = quantized + boundaryNoise;

        return Math.max(-1, Math.min(1, output));
    }

    /**
     * Reset state (stateless, but keep for interface consistency)
     */
    reset() {
        // No state to reset
    }
}
