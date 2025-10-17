// DelayUtils.js
// Fractional delay and interpolation utilities

/**
 * Linear interpolation
 * @param {number} a - Start value
 * @param {number} b - End value
 * @param {number} t - Interpolation factor [0, 1]
 * @returns {number} Interpolated value
 */
export function lerp(a, b, t) {
    return a + (b - a) * t;
}

/**
 * Hermite interpolation (4-point, 3rd order)
 * Provides smooth interpolation with good frequency response
 * @param {number} xm1 - Sample at position -1
 * @param {number} x0 - Sample at position 0
 * @param {number} x1 - Sample at position 1
 * @param {number} x2 - Sample at position 2
 * @param {number} frac - Fractional position [0, 1] between x0 and x1
 * @returns {number} Interpolated value
 */
export function hermiteInterpolate(xm1, x0, x1, x2, frac) {
    const c0 = x0;
    const c1 = 0.5 * (x1 - xm1);
    const c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
    const c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

/**
 * Cubic interpolation (4-point)
 * Simpler than Hermite, good compromise
 * @param {number} xm1 - Sample at position -1
 * @param {number} x0 - Sample at position 0
 * @param {number} x1 - Sample at position 1
 * @param {number} x2 - Sample at position 2
 * @param {number} frac - Fractional position [0, 1]
 * @returns {number} Interpolated value
 */
export function cubicInterpolate(xm1, x0, x1, x2, frac) {
    const a0 = x2 - x1 - xm1 + x0;
    const a1 = xm1 - x0 - a0;
    const a2 = x1 - xm1;
    const a3 = x0;

    return a0 * frac * frac * frac + a1 * frac * frac + a2 * frac + a3;
}

/**
 * Allpass interpolator coefficient calculator
 * For fractional delay implementation
 * @param {number} delay - Fractional delay (0-1)
 * @returns {number} Allpass coefficient
 */
export function allpassCoefficient(delay) {
    return (1 - delay) / (1 + delay);
}

/**
 * First-order allpass filter for fractional delay
 */
export class AllpassDelay {
    constructor() {
        this.x1 = 0;
        this.y1 = 0;
    }

    /**
     * Process one sample through allpass filter
     * @param {number} input - Input sample
     * @param {number} coefficient - Allpass coefficient
     * @returns {number} Delayed output
     */
    process(input, coefficient) {
        const output = coefficient * input + this.x1 - coefficient * this.y1;
        this.x1 = input;
        this.y1 = output;
        return output;
    }

    reset() {
        this.x1 = 0;
        this.y1 = 0;
    }
}

/**
 * Variable delay line with fractional delay support
 */
export class FractionalDelayLine {
    constructor(maxLength) {
        this.buffer = new Float32Array(maxLength);
        this.maxLength = maxLength;
        this.writePos = 0;
    }

    /**
     * Write sample to delay line
     * @param {number} sample - Input sample
     */
    write(sample) {
        this.buffer[this.writePos] = sample;
        this.writePos = (this.writePos + 1) % this.maxLength;
    }

    /**
     * Read from delay line with linear interpolation
     * @param {number} delayLength - Delay in samples (can be fractional)
     * @returns {number} Delayed sample
     */
    readLinear(delayLength) {
        const readPosFloat = this.writePos - delayLength;
        const readPosWrapped = (readPosFloat + this.maxLength) % this.maxLength;
        const readPosInt = Math.floor(readPosWrapped);
        const frac = readPosWrapped - readPosInt;
        const nextPos = (readPosInt + 1) % this.maxLength;

        return lerp(this.buffer[readPosInt], this.buffer[nextPos], frac);
    }

    /**
     * Read from delay line with Hermite interpolation
     * @param {number} delayLength - Delay in samples (can be fractional)
     * @returns {number} Delayed sample
     */
    readHermite(delayLength) {
        const readPosFloat = this.writePos - delayLength;
        const readPosWrapped = (readPosFloat + this.maxLength) % this.maxLength;
        const readPosInt = Math.floor(readPosWrapped);
        const frac = readPosWrapped - readPosInt;

        // Get 4 surrounding samples
        const pos0 = readPosInt;
        const posm1 = (pos0 - 1 + this.maxLength) % this.maxLength;
        const pos1 = (pos0 + 1) % this.maxLength;
        const pos2 = (pos0 + 2) % this.maxLength;

        return hermiteInterpolate(
            this.buffer[posm1],
            this.buffer[pos0],
            this.buffer[pos1],
            this.buffer[pos2],
            frac
        );
    }

    /**
     * Clear the delay line
     */
    reset() {
        this.buffer.fill(0);
        this.writePos = 0;
    }
}

/**
 * Calculate stable delay modulation bounds
 * Ensures delay length stays within safe range for stability
 * @param {number} baseDelay - Base delay length
 * @param {number} modAmount - Modulation amount
 * @param {number} minDelay - Minimum safe delay (default 2 samples)
 * @param {number} maxDelay - Maximum safe delay
 * @returns {Object} {min, max} - Safe modulation range
 */
export function calculateSafeDelayRange(baseDelay, modAmount, minDelay = 2, maxDelay = Infinity) {
    const min = Math.max(minDelay, baseDelay - modAmount);
    const max = Math.min(maxDelay, baseDelay + modAmount);
    return { min, max };
}

/**
 * Smooth delay length changes to avoid clicks
 * First-order lowpass filter
 */
export class DelayLengthSmoother {
    constructor(smoothingTime = 0.01, sampleRate = 44100) {
        this.current = 0;
        this.setSmoothing(smoothingTime, sampleRate);
    }

    /**
     * Set smoothing time constant
     * @param {number} time - Time in seconds
     * @param {number} sampleRate - Sample rate
     */
    setSmoothing(time, sampleRate) {
        this.coefficient = Math.exp(-1 / (time * sampleRate));
    }

    /**
     * Process target delay length
     * @param {number} target - Target delay length
     * @returns {number} Smoothed delay length
     */
    process(target) {
        this.current = this.current * this.coefficient + target * (1 - this.coefficient);
        return this.current;
    }

    reset(value = 0) {
        this.current = value;
    }
}
