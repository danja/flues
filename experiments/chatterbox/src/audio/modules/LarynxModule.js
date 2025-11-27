// LarynxModule.js
// Modified sawtooth oscillator for vocal excitation

const TWO_PI = Math.PI * 2;

export class LarynxModule {
  constructor(sampleRate) {
    this.sampleRate = sampleRate;
    this.phase = 0;
    this.frequency = 120; // Default pitch (Hz)
    this.enabled = true;
  }

  /**
   * Set the pitch frequency
   * @param {number} frequency - Frequency in Hz (typical range: 80-400 Hz)
   */
  setPitch(frequency) {
    this.frequency = Math.max(20, Math.min(2000, frequency));
  }

  /**
   * Enable or disable voiced excitation
   * @param {boolean} enabled - True to enable larynx output
   */
  setVoiced(enabled) {
    this.enabled = enabled;
  }

  /**
   * Step the phase accumulator forward
   * @param {number} frequency - Frequency in Hz
   * @returns {number} New phase value (0-1)
   */
  stepPhase(frequency) {
    this.phase += frequency / this.sampleRate;
    if (this.phase >= 1.0) {
      this.phase -= Math.floor(this.phase);
    }
    return this.phase;
  }

  /**
   * Process one sample
   * @returns {number} Output sample
   */
  process() {
    if (!this.enabled) {
      return 0;
    }

    this.stepPhase(this.frequency);

    // Generate modified sawtooth wave
    // Add some waveshaping for richer harmonic content
    const naive = this.phase * 2 - 1; // Basic sawtooth (-1 to +1)

    // Apply mild cubic waveshaping to emphasize odd harmonics
    // This makes the waveform more "vocal-like"
    const shaped = naive + 0.15 * (naive * naive * naive);

    return shaped * 0.5; // Scale down to prevent clipping
  }

  /**
   * Reset phase (useful for hard sync or note-on events)
   */
  reset() {
    this.phase = 0;
  }
}
