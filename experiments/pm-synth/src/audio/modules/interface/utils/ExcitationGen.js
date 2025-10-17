// ExcitationGen.js
// Excitation signal generators for various interface types

/**
 * Generate triangular initial displacement profile for pluck
 * @param {number} length - Buffer length in samples
 * @param {number} pickPosition - Pick position 0-1 (0 = bridge, 1 = nut)
 * @param {number} amplitude - Peak amplitude
 * @returns {Float32Array} Triangular displacement profile
 */
export function generateTriangularProfile(length, pickPosition = 0.5, amplitude = 1.0) {
    const buffer = new Float32Array(length);
    const pickSample = Math.floor(pickPosition * length);

    // Rising edge (0 to pick position)
    for (let i = 0; i < pickSample; i++) {
        buffer[i] = amplitude * (i / pickSample);
    }

    // Falling edge (pick position to end)
    for (let i = pickSample; i < length; i++) {
        buffer[i] = amplitude * (1 - (i - pickSample) / (length - pickSample));
    }

    return buffer;
}

/**
 * Generate noise burst for percussive attacks
 * @param {number} length - Burst duration in samples
 * @param {number} amplitude - Peak amplitude
 * @param {number} decay - Exponential decay rate (0-1)
 * @returns {Float32Array} Noise burst with exponential decay
 */
export function generateNoiseBurst(length, amplitude = 1.0, decay = 0.95) {
    const buffer = new Float32Array(length);
    let envelope = 1.0;

    for (let i = 0; i < length; i++) {
        const noise = (Math.random() * 2 - 1);
        buffer[i] = noise * amplitude * envelope;
        envelope *= decay;
    }

    return buffer;
}

/**
 * Generate impulse (single sample spike)
 * @param {number} length - Buffer length
 * @param {number} position - Impulse position (0 to length-1)
 * @param {number} amplitude - Impulse amplitude
 * @returns {Float32Array} Impulse signal
 */
export function generateImpulse(length, position = 0, amplitude = 1.0) {
    const buffer = new Float32Array(length);
    if (position >= 0 && position < length) {
        buffer[position] = amplitude;
    }
    return buffer;
}

/**
 * Generate velocity burst (first derivative of displacement)
 * @param {number} length - Buffer length in samples
 * @param {number} amplitude - Peak velocity
 * @param {number} width - Burst width in samples
 * @returns {Float32Array} Velocity profile
 */
export function generateVelocityBurst(length, amplitude = 1.0, width = 10) {
    const buffer = new Float32Array(length);
    const halfWidth = width / 2;

    for (let i = 0; i < Math.min(width, length); i++) {
        const t = (i - halfWidth) / halfWidth;
        // Gaussian-like envelope
        buffer[i] = amplitude * Math.exp(-t * t * 4);
    }

    return buffer;
}

/**
 * White noise generator (stateless)
 * @param {number} amplitude - Peak amplitude (default 1.0)
 * @returns {number} Random value in [-amplitude, +amplitude]
 */
export function whiteNoise(amplitude = 1.0) {
    return (Math.random() * 2 - 1) * amplitude;
}

/**
 * Pink noise generator (1/f spectrum approximation)
 * Uses Paul Kellet's refined method
 */
export class PinkNoiseGenerator {
    constructor() {
        this.b0 = 0;
        this.b1 = 0;
        this.b2 = 0;
        this.b3 = 0;
        this.b4 = 0;
        this.b5 = 0;
        this.b6 = 0;
    }

    /**
     * Generate one sample of pink noise
     * @param {number} amplitude - Peak amplitude
     * @returns {number} Pink noise sample
     */
    process(amplitude = 1.0) {
        const white = Math.random() * 2 - 1;
        this.b0 = 0.99886 * this.b0 + white * 0.0555179;
        this.b1 = 0.99332 * this.b1 + white * 0.0750759;
        this.b2 = 0.96900 * this.b2 + white * 0.1538520;
        this.b3 = 0.86650 * this.b3 + white * 0.3104856;
        this.b4 = 0.55000 * this.b4 + white * 0.5329522;
        this.b5 = -0.7616 * this.b5 - white * 0.0168980;
        const pink = this.b0 + this.b1 + this.b2 + this.b3 + this.b4 + this.b5 + this.b6 + white * 0.5362;
        this.b6 = white * 0.115926;
        return pink * 0.11 * amplitude; // Normalize to ~[-1, 1]
    }

    reset() {
        this.b0 = this.b1 = this.b2 = this.b3 = this.b4 = this.b5 = this.b6 = 0;
    }
}

/**
 * Chaotic oscillator (logistic map)
 * For vapor/turbulent interfaces
 */
export class ChaoticOscillator {
    constructor(r = 3.8) {
        this.r = r;  // Chaos parameter (3.57+ = chaotic)
        this.x = 0.5;
    }

    /**
     * Set chaos parameter
     * @param {number} r - Parameter (2.5-4.0, chaos at 3.57+)
     */
    setR(r) {
        this.r = Math.max(2.5, Math.min(4.0, r));
    }

    /**
     * Generate one sample
     * @param {number} amplitude - Output amplitude scaling
     * @returns {number} Chaotic output in [-amplitude, +amplitude]
     */
    process(amplitude = 1.0) {
        this.x = this.r * this.x * (1 - this.x);
        // Map from [0, 1] to [-1, 1]
        return (this.x * 2 - 1) * amplitude;
    }

    reset() {
        this.x = 0.5;
    }
}

/**
 * Generate Gaussian noise sample (Box-Muller transform)
 * @param {number} mean - Mean value
 * @param {number} stdDev - Standard deviation
 * @returns {number} Gaussian random value
 */
export function gaussianNoise(mean = 0, stdDev = 1.0) {
    // Box-Muller transform
    const u1 = Math.random();
    const u2 = Math.random();
    const z0 = Math.sqrt(-2.0 * Math.log(u1)) * Math.cos(2.0 * Math.PI * u2);
    return z0 * stdDev + mean;
}
