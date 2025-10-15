// signalAnalysis.js
// Shared signal analysis utilities for both runtime diagnostics and tests

/**
 * Calculate RMS (Root Mean Square) level
 * @param {Float32Array} buffer - Signal to analyse
 * @param {number} startIndex - Start index (inclusive)
 * @param {number} endIndex - End index (exclusive)
 * @returns {number} RMS level
 */
export function rmsLevel(buffer, startIndex = 0, endIndex = buffer.length) {
    const length = Math.max(1, endIndex - startIndex);
    let sum = 0;
    for (let i = startIndex; i < endIndex; i++) {
        const sample = buffer[i] || 0;
        sum += sample * sample;
    }
    return Math.sqrt(sum / length);
}

/**
 * Calculate peak (maximum absolute) value
 * @param {Float32Array} buffer - Signal to analyse
 * @returns {number} Peak level
 */
export function peakLevel(buffer) {
    let peak = 0;
    for (let i = 0; i < buffer.length; i++) {
        const sample = Math.abs(buffer[i]);
        if (sample > peak) peak = sample;
    }
    return peak;
}

/**
 * Calculate crest factor (peak/RMS)
 * @param {Float32Array} buffer - Signal to analyse
 * @returns {number} Crest factor
 */
export function crestFactor(buffer) {
    const peak = peakLevel(buffer);
    const rms = rmsLevel(buffer);
    return rms > 0 ? peak / rms : 0;
}

/**
 * Calculate mean value (DC offset)
 * @param {Float32Array} buffer - Signal to analyse
 * @returns {number} Mean value
 */
export function mean(buffer) {
    let sum = 0;
    for (let i = 0; i < buffer.length; i++) {
        sum += buffer[i] || 0;
    }
    return sum / buffer.length;
}

/**
 * Detect peak-to-peak amplitude
 * @param {Float32Array} buffer - Signal to analyse
 * @returns {number} Peak-to-peak level
 */
export function peakToPeak(buffer) {
    let min = Infinity;
    let max = -Infinity;
    for (let i = 0; i < buffer.length; i++) {
        const sample = buffer[i];
        if (sample < min) min = sample;
        if (sample > max) max = sample;
    }
    return max - min;
}

/**
 * Compute a simple magnitude spectrum using a windowed FFT
 * Applies a Hann window and zero padding to nearest power of two
 * @param {Float32Array} timeDomain - Time-domain samples
 * @returns {{ frequencies: Float32Array, magnitudes: Float32Array }}
 */
export function computeSpectrum(timeDomain, sampleRate = 44100) {
    const length = timeDomain.length;
    if (length === 0) {
        return { frequencies: new Float32Array(0), magnitudes: new Float32Array(0) };
    }

    const size = Math.pow(2, Math.ceil(Math.log2(length)));
    const real = new Float32Array(size);
    const imag = new Float32Array(size);

    // Apply Hann window and copy into FFT buffer
    for (let i = 0; i < size; i++) {
        const sample = i < length ? timeDomain[i] : 0;
        const window = 0.5 * (1 - Math.cos((2 * Math.PI * i) / (size - 1)));
        real[i] = sample * window;
        imag[i] = 0;
    }

    fft(real, imag);

    const half = size / 2;
    const magnitudes = new Float32Array(half);
    const frequencies = new Float32Array(half);

    for (let i = 0; i < half; i++) {
        const re = real[i];
        const im = imag[i];
        magnitudes[i] = Math.sqrt(re * re + im * im);
        frequencies[i] = (i * sampleRate) / size;
    }

    return { frequencies, magnitudes };
}

/**
 * In-place Cooley-Tukey FFT (radix-2)
 * Adapted for small FFT sizes used in the analyser path
 */
function fft(real, imag) {
    const n = real.length;
    const levels = Math.log2(n);

    if (!Number.isInteger(levels)) {
        throw new Error('FFT size must be a power of two');
    }

    // Bit-reversed addressing permutation
    for (let i = 0; i < n; i++) {
        const j = reverseBits(i, levels);
        if (j > i) {
            [real[i], real[j]] = [real[j], real[i]];
            [imag[i], imag[j]] = [imag[j], imag[i]];
        }
    }

    for (let size = 2; size <= n; size <<= 1) {
        const halfSize = size >> 1;
        const tableStep = (2 * Math.PI) / size;

        for (let i = 0; i < n; i += size) {
            for (let j = 0; j < halfSize; j++) {
                const angle = tableStep * j;
                const cos = Math.cos(angle);
                const sin = -Math.sin(angle);

                const index1 = i + j;
                const index2 = index1 + halfSize;

                const tre = real[index2] * cos - imag[index2] * sin;
                const tim = real[index2] * sin + imag[index2] * cos;

                real[index2] = real[index1] - tre;
                imag[index2] = imag[index1] - tim;
                real[index1] += tre;
                imag[index1] += tim;
            }
        }
    }
}

function reverseBits(value, bits) {
    let reversed = 0;
    for (let i = 0; i < bits; i++) {
        reversed = (reversed << 1) | (value & 1);
        value >>= 1;
    }
    return reversed;
}

/**
 * Convenience helper that aggregates common metrics for display
 * @param {Float32Array} buffer - Time-domain buffer (values in [-1, 1])
 * @param {number} sampleRate - Sample rate for spectral analysis
 * @returns {{
 *   rms: number,
 *   peak: number,
 *   peakToPeak: number,
 *   crestFactor: number,
 *   dcOffset: number,
 *   spectrum: { frequencies: Float32Array, magnitudes: Float32Array }
 * }}
 */
export function analyseBuffer(buffer, sampleRate = 44100, options = {}) {
    const { includeSpectrum = false } = options;
    if (!buffer || buffer.length === 0) {
        return {
            rms: 0,
            peak: 0,
            peakToPeak: 0,
            crestFactor: 0,
            dcOffset: 0,
            spectrum: { frequencies: new Float32Array(0), magnitudes: new Float32Array(0) }
        };
    }

    const result = {
        rms: rmsLevel(buffer),
        peak: peakLevel(buffer),
        peakToPeak: peakToPeak(buffer),
        crestFactor: crestFactor(buffer),
        dcOffset: mean(buffer),
        spectrum: { frequencies: new Float32Array(0), magnitudes: new Float32Array(0) }
    };

    if (includeSpectrum) {
        result.spectrum = computeSpectrum(buffer, sampleRate);
    }

    return result;
}

export const SignalAnalysis = {
    rmsLevel,
    peakLevel,
    crestFactor,
    mean,
    peakToPeak,
    computeSpectrum,
    analyseBuffer
};
