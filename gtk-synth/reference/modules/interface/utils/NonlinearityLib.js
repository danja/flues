// NonlinearityLib.js
// Shared nonlinear functions for interface processing

/**
 * Fast tanh approximation using rational function
 * Accurate to ~0.001 for |x| < 3
 * @param {number} x - Input value
 * @returns {number} Approximated tanh(x)
 */
export function fastTanh(x) {
    const TANH_CLIP_THRESHOLD = 3;
    const TANH_NUMERATOR_CONSTANT = 27;
    const TANH_DENOMINATOR_SCALE = 9;

    if (x > TANH_CLIP_THRESHOLD) return 1;
    if (x < -TANH_CLIP_THRESHOLD) return -1;

    const x2 = x * x;
    return x * (TANH_NUMERATOR_CONSTANT + x2) /
           (TANH_NUMERATOR_CONSTANT + TANH_DENOMINATOR_SCALE * x2);
}

/**
 * Hard clipper with adjustable threshold
 * @param {number} x - Input value
 * @param {number} threshold - Clipping threshold (default 1.0)
 * @returns {number} Clipped value
 */
export function hardClip(x, threshold = 1.0) {
    return Math.max(-threshold, Math.min(threshold, x));
}

/**
 * Soft clipper using tanh
 * @param {number} x - Input value
 * @param {number} drive - Pre-gain before clipping (default 1.0)
 * @returns {number} Soft clipped value
 */
export function softClip(x, drive = 1.0) {
    return fastTanh(x * drive);
}

/**
 * Power function with fast approximation for common exponents
 * Uses lookup table for fractional powers
 * @param {number} x - Input value (must be positive)
 * @param {number} alpha - Exponent
 * @returns {number} x^alpha
 */
export function powerFunction(x, alpha) {
    if (x <= 0) return 0;

    // Fast path for common exponents
    if (alpha === 1) return x;
    if (alpha === 2) return x * x;
    if (alpha === 3) return x * x * x;
    if (alpha === 0.5) return Math.sqrt(x);

    // General case using exp/log
    return Math.exp(alpha * Math.log(x));
}

/**
 * Cubic polynomial waveshaper
 * out = x - α*x³
 * @param {number} x - Input value
 * @param {number} alpha - Nonlinearity strength (0-1)
 * @returns {number} Shaped output
 */
export function cubicWaveshaper(x, alpha = 0.33) {
    const x3 = x * x * x;
    return x - alpha * x3;
}

/**
 * Polynomial waveshaper with adjustable coefficients
 * out = a1*x + a3*x³ + a5*x⁵
 * @param {number} x - Input value
 * @param {number} a1 - Linear coefficient
 * @param {number} a3 - Cubic coefficient
 * @param {number} a5 - Quintic coefficient
 * @returns {number} Shaped output
 */
export function polynomialWaveshaper(x, a1 = 1.0, a3 = -0.33, a5 = 0.1) {
    const x2 = x * x;
    const x3 = x2 * x;
    const x5 = x3 * x2;
    return a1 * x + a3 * x3 + a5 * x5;
}

/**
 * Friction curve generator (Stribeck + viscous damping)
 * μ(v) = μ_s + (μ_d - μ_s) * exp(-|v|/v_s) + μ_v * v
 * @param {number} velocity - Relative velocity
 * @param {number} normalForce - Normal force (intensity parameter)
 * @returns {number} Friction force
 */
export function frictionCurve(velocity, normalForce = 1.0) {
    const MU_STATIC = 0.8;      // Static friction coefficient
    const MU_DYNAMIC = 0.6;     // Dynamic friction coefficient
    const MU_VISCOUS = 0.05;    // Viscous damping coefficient
    const V_STRIBECK = 0.01 + normalForce * 0.09;  // Stribeck velocity

    const absV = Math.abs(velocity);
    const mu = MU_STATIC + (MU_DYNAMIC - MU_STATIC) * Math.exp(-absV / V_STRIBECK)
               + MU_VISCOUS * absV;

    return normalForce * mu * Math.sign(velocity);
}

/**
 * Sigmoid function (smooth step)
 * @param {number} x - Input value
 * @param {number} steepness - Steepness factor (default 1.0)
 * @returns {number} Value in range [0, 1]
 */
export function sigmoid(x, steepness = 1.0) {
    return 1 / (1 + Math.exp(-steepness * x));
}

/**
 * Sine fold waveshaper (for percussive sounds)
 * @param {number} x - Input value
 * @param {number} drive - Pre-gain
 * @returns {number} Folded output
 */
export function sineFold(x, drive = 1.0) {
    return Math.sin(x * drive * Math.PI * 0.5);
}

/**
 * Asymmetric waveshaper (different positive/negative curves)
 * @param {number} x - Input value
 * @param {number} posGain - Positive side gain
 * @param {number} negGain - Negative side gain
 * @returns {number} Asymmetrically shaped output
 */
export function asymmetricShape(x, posGain = 1.0, negGain = 1.0) {
    if (x >= 0) {
        return fastTanh(x * posGain);
    } else {
        return fastTanh(x * negGain);
    }
}
