// signal-generators.js
// Standardized test signal generators for audio module testing

/**
 * Test signal generators for systematic module validation
 */
export const TestSignals = {
    /**
     * Generate an impulse (single spike, rest zeros)
     * Useful for testing transient response and impulse response
     * @param {number} length - Buffer length in samples
     * @param {number} amplitude - Impulse amplitude (default 1.0)
     * @returns {Float32Array}
     */
    impulse(length = 1000, amplitude = 1.0) {
        const buffer = new Float32Array(length);
        buffer[0] = amplitude;
        return buffer;
    },

    /**
     * Generate a step function (DC offset)
     * Useful for testing stability and bias handling
     * @param {number} level - DC level
     * @param {number} length - Buffer length in samples
     * @returns {Float32Array}
     */
    step(level, length = 1000) {
        return new Float32Array(length).fill(level);
    },

    /**
     * Generate a pure sine wave
     * Useful for testing frequency response
     * @param {number} frequency - Frequency in Hz
     * @param {number} sampleRate - Sample rate
     * @param {number} length - Buffer length in samples
     * @param {number} amplitude - Amplitude (default 1.0)
     * @param {number} phase - Initial phase in radians (default 0)
     * @returns {Float32Array}
     */
    sine(frequency, sampleRate, length, amplitude = 1.0, phase = 0) {
        const buffer = new Float32Array(length);
        for (let i = 0; i < length; i++) {
            buffer[i] = amplitude * Math.sin(2 * Math.PI * frequency * i / sampleRate + phase);
        }
        return buffer;
    },

    /**
     * Generate white noise
     * Useful for testing noise handling and stability
     * @param {number} length - Buffer length in samples
     * @param {number} amplitude - Amplitude (default 1.0)
     * @returns {Float32Array}
     */
    whiteNoise(length = 1000, amplitude = 1.0) {
        const buffer = new Float32Array(length);
        for (let i = 0; i < length; i++) {
            buffer[i] = (Math.random() * 2 - 1) * amplitude;
        }
        return buffer;
    },

    /**
     * Generate a square wave
     * Useful for testing nonlinear behavior and discontinuities
     * @param {number} frequency - Frequency in Hz
     * @param {number} sampleRate - Sample rate
     * @param {number} length - Buffer length in samples
     * @param {number} amplitude - Amplitude (default 1.0)
     * @returns {Float32Array}
     */
    square(frequency, sampleRate, length, amplitude = 1.0) {
        const buffer = new Float32Array(length);
        const period = sampleRate / frequency;
        for (let i = 0; i < length; i++) {
            buffer[i] = ((i % period) < (period / 2)) ? amplitude : -amplitude;
        }
        return buffer;
    },

    /**
     * Generate a sawtooth wave
     * Useful for testing aliasing and harmonic content
     * @param {number} frequency - Frequency in Hz
     * @param {number} sampleRate - Sample rate
     * @param {number} length - Buffer length in samples
     * @param {number} amplitude - Amplitude (default 1.0)
     * @returns {Float32Array}
     */
    sawtooth(frequency, sampleRate, length, amplitude = 1.0) {
        const buffer = new Float32Array(length);
        let phase = 0;
        const phaseIncrement = frequency / sampleRate;
        for (let i = 0; i < length; i++) {
            buffer[i] = (phase * 2 - 1) * amplitude;
            phase += phaseIncrement;
            if (phase >= 1.0) phase -= 1.0;
        }
        return buffer;
    },

    /**
     * Generate a tone burst (windowed sine wave)
     * Useful for testing envelope and decay characteristics
     * @param {number} frequency - Frequency in Hz
     * @param {number} sampleRate - Sample rate
     * @param {number} burstLength - Burst duration in samples
     * @param {number} totalLength - Total buffer length
     * @param {number} amplitude - Amplitude (default 1.0)
     * @returns {Float32Array}
     */
    burst(frequency, sampleRate, burstLength, totalLength, amplitude = 1.0) {
        const buffer = new Float32Array(totalLength);
        for (let i = 0; i < Math.min(burstLength, totalLength); i++) {
            // Hann window
            const window = 0.5 * (1 - Math.cos(2 * Math.PI * i / burstLength));
            buffer[i] = window * amplitude * Math.sin(2 * Math.PI * frequency * i / sampleRate);
        }
        return buffer;
    },

    /**
     * Generate a frequency sweep (chirp)
     * Useful for testing filter response across spectrum
     * @param {number} startFreq - Starting frequency in Hz
     * @param {number} endFreq - Ending frequency in Hz
     * @param {number} sampleRate - Sample rate
     * @param {number} length - Buffer length in samples
     * @param {number} amplitude - Amplitude (default 1.0)
     * @returns {Float32Array}
     */
    sweep(startFreq, endFreq, sampleRate, length, amplitude = 1.0) {
        const buffer = new Float32Array(length);
        let phase = 0;
        for (let i = 0; i < length; i++) {
            const t = i / length;
            const freq = startFreq + (endFreq - startFreq) * t;
            buffer[i] = amplitude * Math.sin(phase);
            phase += 2 * Math.PI * freq / sampleRate;
        }
        return buffer;
    },

    /**
     * Generate a ramp (linear increase)
     * Useful for testing parameter ramping
     * @param {number} start - Starting value
     * @param {number} end - Ending value
     * @param {number} length - Buffer length in samples
     * @returns {Float32Array}
     */
    ramp(start, end, length) {
        const buffer = new Float32Array(length);
        const increment = (end - start) / (length - 1);
        for (let i = 0; i < length; i++) {
            buffer[i] = start + increment * i;
        }
        return buffer;
    },

    /**
     * Generate silence (all zeros)
     * Useful as a baseline or for testing reset behavior
     * @param {number} length - Buffer length in samples
     * @returns {Float32Array}
     */
    silence(length = 1000) {
        return new Float32Array(length);
    }
};
