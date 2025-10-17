// EnergyTracker.js
// Energy and amplitude tracking utilities

/**
 * RMS (Root Mean Square) energy estimator
 * Provides smoothed energy measurement
 */
export class RMSTracker {
    constructor(windowSize = 1024) {
        this.windowSize = windowSize;
        this.buffer = new Float32Array(windowSize);
        this.writePos = 0;
        this.sumOfSquares = 0;
    }

    /**
     * Process one sample and return RMS energy
     * @param {number} sample - Input sample
     * @returns {number} RMS energy
     */
    process(sample) {
        // Remove old sample contribution
        const oldSample = this.buffer[this.writePos];
        this.sumOfSquares -= oldSample * oldSample;

        // Add new sample
        this.buffer[this.writePos] = sample;
        this.sumOfSquares += sample * sample;

        this.writePos = (this.writePos + 1) % this.windowSize;

        // Return RMS
        return Math.sqrt(this.sumOfSquares / this.windowSize);
    }

    /**
     * Get current RMS without processing new sample
     * @returns {number} Current RMS
     */
    getRMS() {
        return Math.sqrt(this.sumOfSquares / this.windowSize);
    }

    reset() {
        this.buffer.fill(0);
        this.writePos = 0;
        this.sumOfSquares = 0;
    }
}

/**
 * Peak envelope follower
 * Tracks peak amplitude with attack/release
 */
export class PeakEnvelopeFollower {
    constructor(attackTime = 0.001, releaseTime = 0.1, sampleRate = 44100) {
        this.peak = 0;
        this.setTimes(attackTime, releaseTime, sampleRate);
    }

    /**
     * Set attack and release times
     * @param {number} attackTime - Attack time in seconds
     * @param {number} releaseTime - Release time in seconds
     * @param {number} sampleRate - Sample rate
     */
    setTimes(attackTime, releaseTime, sampleRate) {
        this.attackCoeff = Math.exp(-1 / (attackTime * sampleRate));
        this.releaseCoeff = Math.exp(-1 / (releaseTime * sampleRate));
    }

    /**
     * Process one sample
     * @param {number} sample - Input sample
     * @returns {number} Envelope amplitude
     */
    process(sample) {
        const rectified = Math.abs(sample);

        if (rectified > this.peak) {
            // Attack (fast response to increases)
            this.peak = this.peak * this.attackCoeff + rectified * (1 - this.attackCoeff);
        } else {
            // Release (slow decay)
            this.peak = this.peak * this.releaseCoeff + rectified * (1 - this.releaseCoeff);
        }

        return this.peak;
    }

    /**
     * Get current peak without processing
     * @returns {number} Current peak
     */
    getPeak() {
        return this.peak;
    }

    reset() {
        this.peak = 0;
    }
}

/**
 * Leaky integrator (exponential moving average)
 * Useful for slow energy tracking
 */
export class LeakyIntegrator {
    constructor(timeConstant = 0.1, sampleRate = 44100) {
        this.value = 0;
        this.setTimeConstant(timeConstant, sampleRate);
    }

    /**
     * Set integration time constant
     * @param {number} timeConstant - Time constant in seconds
     * @param {number} sampleRate - Sample rate
     */
    setTimeConstant(timeConstant, sampleRate) {
        this.coefficient = Math.exp(-1 / (timeConstant * sampleRate));
    }

    /**
     * Process one sample
     * @param {number} sample - Input sample
     * @returns {number} Integrated value
     */
    process(sample) {
        this.value = this.value * this.coefficient + sample * (1 - this.coefficient);
        return this.value;
    }

    /**
     * Get current value without processing
     * @returns {number} Current integrated value
     */
    getValue() {
        return this.value;
    }

    reset(initialValue = 0) {
        this.value = initialValue;
    }
}

/**
 * Energy accumulator with controllable decay
 * Used for drum-like energy buildup
 */
export class EnergyAccumulator {
    constructor(decayRate = 0.9) {
        this.energy = 0;
        this.decayRate = decayRate;
    }

    /**
     * Set decay rate
     * @param {number} rate - Decay multiplier per sample (0-1)
     */
    setDecayRate(rate) {
        this.decayRate = Math.max(0, Math.min(1, rate));
    }

    /**
     * Add energy to accumulator
     * @param {number} input - Energy input (typically |sample|)
     * @returns {number} Current accumulated energy
     */
    process(input) {
        this.energy = this.energy * this.decayRate + Math.abs(input);
        return this.energy;
    }

    /**
     * Get current energy level
     * @returns {number} Energy level
     */
    getEnergy() {
        return this.energy;
    }

    reset() {
        this.energy = 0;
    }
}

/**
 * Instantaneous amplitude tracker
 * Simple absolute value with optional smoothing
 */
export class AmplitudeTracker {
    constructor(smoothing = 0.0, sampleRate = 44100) {
        this.amplitude = 0;
        this.setSmoothing(smoothing, sampleRate);
    }

    /**
     * Set smoothing amount
     * @param {number} time - Smoothing time in seconds (0 = no smoothing)
     * @param {number} sampleRate - Sample rate
     */
    setSmoothing(time, sampleRate) {
        if (time <= 0) {
            this.coefficient = 0;
        } else {
            this.coefficient = Math.exp(-1 / (time * sampleRate));
        }
    }

    /**
     * Process one sample
     * @param {number} sample - Input sample
     * @returns {number} Amplitude
     */
    process(sample) {
        const instant = Math.abs(sample);

        if (this.coefficient === 0) {
            this.amplitude = instant;
        } else {
            this.amplitude = this.amplitude * this.coefficient + instant * (1 - this.coefficient);
        }

        return this.amplitude;
    }

    getAmplitude() {
        return this.amplitude;
    }

    reset() {
        this.amplitude = 0;
    }
}

/**
 * Zero-crossing rate detector
 * Measures signal periodicity/noisiness
 */
export class ZeroCrossingDetector {
    constructor(windowSize = 256) {
        this.windowSize = windowSize;
        this.prevSample = 0;
        this.crossingCount = 0;
        this.sampleCount = 0;
    }

    /**
     * Process one sample
     * @param {number} sample - Input sample
     * @returns {number} Zero crossing rate (0-1)
     */
    process(sample) {
        // Detect zero crossing
        if ((this.prevSample >= 0 && sample < 0) || (this.prevSample < 0 && sample >= 0)) {
            this.crossingCount++;
        }

        this.sampleCount++;

        // Calculate rate over window
        if (this.sampleCount >= this.windowSize) {
            const rate = this.crossingCount / this.windowSize;
            this.crossingCount = 0;
            this.sampleCount = 0;
            this.prevSample = sample;
            return rate;
        }

        this.prevSample = sample;
        return this.crossingCount / Math.max(1, this.sampleCount);
    }

    reset() {
        this.prevSample = 0;
        this.crossingCount = 0;
        this.sampleCount = 0;
    }
}
