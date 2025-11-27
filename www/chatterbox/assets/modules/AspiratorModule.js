// AspiratorModule.js
// White noise generator for unvoiced/aspirated sounds

export class AspiratorModule {
  constructor(sampleRate) {
    this.sampleRate = sampleRate;
    this.level = 0.0;
    this.enabled = false;
  }

  /**
   * Set the noise level
   * @param {number} level - Amplitude 0-1
   */
  setLevel(level) {
    this.level = Math.max(0, Math.min(1, level));
  }

  /**
   * Enable or disable aspirated noise
   * @param {boolean} enabled - True to enable noise output
   */
  setAspirated(enabled) {
    this.enabled = enabled;
  }

  /**
   * Process one sample
   * @returns {number} Output sample
   */
  process() {
    if (!this.enabled || this.level <= 0) {
      return 0;
    }

    // Generate white noise
    const noise = Math.random() * 2 - 1;

    return noise * this.level;
  }
}
