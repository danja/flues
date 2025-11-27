// FormantModule.js
// Resonant bandpass filter for vocal formants

const TWO_PI = Math.PI * 2;

export class FormantModule {
  constructor(sampleRate) {
    this.sampleRate = sampleRate;

    // Filter parameters
    this.frequency = 500; // Center frequency in Hz
    this.bandwidth = 100; // Bandwidth in Hz

    // Biquad filter coefficients
    this.a0 = 1;
    this.a1 = 0;
    this.a2 = 0;
    this.b1 = 0;
    this.b2 = 0;

    // Filter state
    this.x1 = 0;
    this.x2 = 0;
    this.y1 = 0;
    this.y2 = 0;

    this.updateCoefficients();
  }

  /**
   * Set formant center frequency
   * @param {number} frequency - Frequency in Hz
   */
  setFrequency(frequency) {
    this.frequency = Math.max(20, Math.min(this.sampleRate / 2, frequency));
    this.updateCoefficients();
  }

  /**
   * Set formant bandwidth
   * @param {number} bandwidth - Bandwidth in Hz
   */
  setBandwidth(bandwidth) {
    this.bandwidth = Math.max(10, Math.min(5000, bandwidth));
    this.updateCoefficients();
  }

  /**
   * Set formant Q (quality factor)
   * @param {number} q - Q value (higher = narrower bandwidth)
   */
  setQ(q) {
    // Convert Q to bandwidth: BW = f0 / Q
    this.bandwidth = this.frequency / Math.max(0.5, q);
    this.updateCoefficients();
  }

  /**
   * Update biquad filter coefficients for bandpass filter
   * Using Q-based parameterization for stability
   */
  updateCoefficients() {
    // Calculate Q from bandwidth: Q = f0 / BW
    const Q = Math.max(0.5, this.frequency / this.bandwidth);

    // Normalize frequency
    const omega = TWO_PI * this.frequency / this.sampleRate;
    const sn = Math.sin(omega);
    const cs = Math.cos(omega);
    const alpha = sn / (2 * Q);

    // Bandpass filter coefficients (constant 0 dB peak gain)
    const a0 = 1 + alpha;
    const a1 = -2 * cs;
    const a2 = 1 - alpha;
    const b0 = Q * alpha;
    const b1 = 0;
    const b2 = -Q * alpha;

    // Normalize
    this.a0 = b0 / a0;
    this.a1 = b1 / a0;
    this.a2 = b2 / a0;
    this.b1 = a1 / a0;
    this.b2 = a2 / a0;
  }

  /**
   * Process one sample through the formant filter
   * @param {number} input - Input sample
   * @returns {number} Filtered output
   */
  process(input) {
    // Biquad difference equation:
    // y[n] = a0*x[n] + a1*x[n-1] + a2*x[n-2] - b1*y[n-1] - b2*y[n-2]
    const output = this.a0 * input + this.a1 * this.x1 + this.a2 * this.x2
                 - this.b1 * this.y1 - this.b2 * this.y2;

    // Update state
    this.x2 = this.x1;
    this.x1 = input;
    this.y2 = this.y1;
    this.y1 = output;

    // Stability check
    if (!isFinite(output)) {
      this.reset();
      return 0;
    }

    return output;
  }

  /**
   * Reset filter state
   */
  reset() {
    this.x1 = 0;
    this.x2 = 0;
    this.y1 = 0;
    this.y2 = 0;
  }
}
