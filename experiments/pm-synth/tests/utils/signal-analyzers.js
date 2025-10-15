// signal-analyzers.js
// Signal analysis utilities for validating audio module outputs

import {
    rmsLevel as sharedRmsLevel,
    peakLevel as sharedPeakLevel,
    crestFactor as sharedCrestFactor,
    mean as sharedMean,
    peakToPeak as sharedPeakToPeak,
    computeSpectrum as sharedComputeSpectrum,
    analyseBuffer as sharedAnalyseBuffer
} from '../../src/utils/signalAnalysis.js';

/**
 * Signal analysis functions for systematic output validation
 */
export const SignalAnalysis = {
    /**
     * Detect if signal prematurely drops to silence
     * Useful for catching feedback/envelope issues
     * @param {Float32Array} buffer - Signal to analyze
     * @param {number} threshold - Silence threshold (default 0.001)
     * @param {number} minDuration - Minimum silent samples before flagging (default 100)
     * @param {number} startIndex - Where to start checking (skip attack phase)
     * @returns {{premature: boolean, index: number|null, duration: number}}
     */
    detectPrematureSilence(buffer, threshold = 0.001, minDuration = 100, startIndex = 0) {
        let silentCount = 0;
        let silenceStart = -1;

        for (let i = startIndex; i < buffer.length; i++) {
            if (Math.abs(buffer[i]) < threshold) {
                if (silenceStart === -1) silenceStart = i;
                silentCount++;
                if (silentCount >= minDuration) {
                    return {
                        premature: true,
                        index: silenceStart,
                        duration: silentCount
                    };
                }
            } else {
                silentCount = 0;
                silenceStart = -1;
            }
        }

        return { premature: false, index: null, duration: 0 };
    },

    /**
     * Calculate RMS (Root Mean Square) level
     * Useful for measuring average signal energy
     * @param {Float32Array} buffer - Signal to analyze
     * @param {number} startIndex - Start analysis here
     * @param {number} endIndex - End analysis here (default: buffer.length)
     * @returns {number} RMS level
     */
    rmsLevel(buffer, startIndex = 0, endIndex = buffer.length) {
        return sharedRmsLevel(buffer, startIndex, endIndex);
    },

    /**
     * Calculate peak level (maximum absolute value)
     * @param {Float32Array} buffer - Signal to analyze
     * @returns {number} Peak level
     */
    peakLevel(buffer) {
        return sharedPeakLevel(buffer);
    },

    /**
     * Calculate crest factor (peak / RMS ratio)
     * Useful for detecting clipping or compression
     * @param {Float32Array} buffer - Signal to analyze
     * @returns {number} Crest factor
     */
    crestFactor(buffer) {
        return sharedCrestFactor(buffer);
    },

    peakToPeak(buffer) {
        return sharedPeakToPeak(buffer);
    },

    /**
     * Check if all samples are finite and within reasonable bounds
     * @param {Float32Array} buffer - Signal to analyze
     * @param {number} maxAbsValue - Maximum acceptable absolute value (default 100)
     * @returns {boolean} True if all samples valid
     */
    hasValidOutput(buffer, maxAbsValue = 100) {
        for (let i = 0; i < buffer.length; i++) {
            if (!isFinite(buffer[i]) || Math.abs(buffer[i]) > maxAbsValue) {
                return false;
            }
        }
        return true;
    },

    /**
     * Find all samples that exceed threshold
     * @param {Float32Array} buffer - Signal to analyze
     * @param {number} threshold - Threshold value
     * @returns {{count: number, indices: number[], maxValue: number}}
     */
    findClipping(buffer, threshold) {
        const indices = [];
        let maxValue = 0;

        for (let i = 0; i < buffer.length; i++) {
            const abs = Math.abs(buffer[i]);
            if (abs > threshold) {
                indices.push(i);
                if (abs > maxValue) maxValue = abs;
            }
        }

        return {
            count: indices.length,
            indices: indices,
            maxValue: maxValue
        };
    },

    /**
     * Calculate mean (DC offset)
     * @param {Float32Array} buffer - Signal to analyze
     * @returns {number} Mean value
     */
    mean(buffer) {
        return sharedMean(buffer);
    },

    /**
     * Calculate variance
     * @param {Float32Array} buffer - Signal to analyze
     * @returns {number} Variance
     */
    variance(buffer) {
        const meanVal = this.mean(buffer);
        let sum = 0;
        for (let i = 0; i < buffer.length; i++) {
            const diff = buffer[i] - meanVal;
            sum += diff * diff;
        }
        return sum / buffer.length;
    },

    /**
     * Calculate standard deviation
     * @param {Float32Array} buffer - Signal to analyze
     * @returns {number} Standard deviation
     */
    standardDeviation(buffer) {
        return Math.sqrt(this.variance(buffer));
    },

    /**
     * Count zero crossings (approximate frequency measure)
     * @param {Float32Array} buffer - Signal to analyze
     * @returns {number} Number of zero crossings
     */
    zeroCrossingRate(buffer) {
        let crossings = 0;
        for (let i = 1; i < buffer.length; i++) {
            if ((buffer[i - 1] >= 0 && buffer[i] < 0) ||
                (buffer[i - 1] < 0 && buffer[i] >= 0)) {
                crossings++;
            }
        }
        return crossings;
    },

    /**
     * Estimate fundamental frequency from zero crossings
     * @param {Float32Array} buffer - Signal to analyze
     * @param {number} sampleRate - Sample rate
     * @returns {number} Estimated frequency in Hz
     */
    estimateFrequency(buffer, sampleRate) {
        const crossings = this.zeroCrossingRate(buffer);
        const duration = buffer.length / sampleRate;
        return crossings / (2 * duration); // Divide by 2 (two crossings per cycle)
    },

    /**
     * Measure decay time (time to drop below threshold)
     * Useful for envelope and reverb testing
     * @param {Float32Array} buffer - Signal to analyze
     * @param {number} threshold - Level threshold (default 0.01 = -40dB)
     * @param {number} sampleRate - Sample rate
     * @returns {{samples: number, seconds: number, found: boolean}}
     */
    measureDecayTime(buffer, threshold = 0.01, sampleRate = 44100) {
        // Find peak first
        let peakIndex = 0;
        let peakValue = 0;
        for (let i = 0; i < buffer.length; i++) {
            const abs = Math.abs(buffer[i]);
            if (abs > peakValue) {
                peakValue = abs;
                peakIndex = i;
            }
        }

        // Find where it drops below threshold after peak
        for (let i = peakIndex; i < buffer.length; i++) {
            if (Math.abs(buffer[i]) < threshold) {
                const samples = i - peakIndex;
                return {
                    samples: samples,
                    seconds: samples / sampleRate,
                    found: true
                };
            }
        }

        return {
            samples: buffer.length - peakIndex,
            seconds: (buffer.length - peakIndex) / sampleRate,
            found: false
        };
    },

    /**
     * Check if signal maintains minimum energy level
     * Useful for detecting premature decay in sustaining sounds
     * @param {Float32Array} buffer - Signal to analyze
     * @param {number} minRMS - Minimum acceptable RMS level
     * @param {number} windowSize - Analysis window size (default 100)
     * @returns {{passes: boolean, lowestRMS: number, failIndex: number|null}}
     */
    checkSustainedEnergy(buffer, minRMS, windowSize = 100) {
        let lowestRMS = Infinity;
        let failIndex = null;

        for (let i = 0; i <= buffer.length - windowSize; i += windowSize) {
            const windowRMS = this.rmsLevel(buffer, i, i + windowSize);
            if (windowRMS < lowestRMS) lowestRMS = windowRMS;
            if (windowRMS < minRMS && failIndex === null) {
                failIndex = i;
            }
        }

        return {
            passes: failIndex === null,
            lowestRMS: lowestRMS,
            failIndex: failIndex
        };
    },

    /**
     * Detect discontinuities (clicks/pops)
     * @param {Float32Array} buffer - Signal to analyze
     * @param {number} threshold - Difference threshold (default 0.5)
     * @returns {{count: number, indices: number[], maxDiff: number}}
     */
    detectDiscontinuities(buffer, threshold = 0.5) {
        const indices = [];
        let maxDiff = 0;

        for (let i = 1; i < buffer.length; i++) {
            const diff = Math.abs(buffer[i] - buffer[i - 1]);
            if (diff > threshold) {
                indices.push(i);
                if (diff > maxDiff) maxDiff = diff;
            }
        }

        return {
            count: indices.length,
            indices: indices,
            maxDiff: maxDiff
        };
    },

    /**
     * Check if signal is monotonically increasing/decreasing
     * Useful for envelope attack/release validation
     * @param {Float32Array} buffer - Signal to analyze
     * @param {string} direction - 'increasing' or 'decreasing'
     * @param {number} tolerance - Allow small violations (default 0.0001)
     * @returns {{monotonic: boolean, violations: number}}
     */
    checkMonotonic(buffer, direction = 'increasing', tolerance = 0.0001) {
        let violations = 0;

        for (let i = 1; i < buffer.length; i++) {
            const diff = buffer[i] - buffer[i - 1];
            if (direction === 'increasing' && diff < -tolerance) {
                violations++;
            } else if (direction === 'decreasing' && diff > tolerance) {
                violations++;
            }
        }

        return {
            monotonic: violations === 0,
            violations: violations
        };
    },

    /**
     * Compare two signals and measure similarity
     * @param {Float32Array} signal1 - First signal
     * @param {Float32Array} signal2 - Second signal
     * @returns {{correlation: number, meanSquareError: number}}
     */
    compareSignals(signal1, signal2) {
        const length = Math.min(signal1.length, signal2.length);

        let sum1 = 0, sum2 = 0, sum1Sq = 0, sum2Sq = 0, pSum = 0;
        for (let i = 0; i < length; i++) {
            sum1 += signal1[i];
            sum2 += signal2[i];
            sum1Sq += signal1[i] * signal1[i];
            sum2Sq += signal2[i] * signal2[i];
            pSum += signal1[i] * signal2[i];
        }

        const num = pSum - (sum1 * sum2 / length);
        const den = Math.sqrt((sum1Sq - sum1 * sum1 / length) * (sum2Sq - sum2 * sum2 / length));
        const correlation = den === 0 ? 0 : num / den;

        let mse = 0;
        for (let i = 0; i < length; i++) {
            const diff = signal1[i] - signal2[i];
            mse += diff * diff;
        }
        mse /= length;

        return {
            correlation,
            meanSquareError: mse
        };
    },

    analyseBuffer(buffer, sampleRate, options) {
        return sharedAnalyseBuffer(buffer, sampleRate, options);
    },

    computeSpectrum(buffer, sampleRate) {
        return sharedComputeSpectrum(buffer, sampleRate);
    }
};
