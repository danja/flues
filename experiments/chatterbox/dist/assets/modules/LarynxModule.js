// LarynxModule.js
// Modified sawtooth oscillator for vocal excitation

const TWO_PI = Math.PI * 2;

export class LarynxModule {
  constructor(sampleRate) {
    this.sampleRate = sampleRate;
    this.phase = 0;
    this.frequency = 120; // Default pitch (Hz)
    this.enabled = true;

    // Vocal fry mode (subharmonics)
    this.fryEnabled = false;
    this.fryPhase = 0;

    // Vibrato (sing mode)
    this.vibratoEnabled = false;
    this.vibratoPhase = 0;
    this.vibratoRate = 5.5; // Hz
    this.vibratoDepth = 0.015; // ±1.5%
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
   * Enable or disable vocal fry mode
   * @param {boolean} enabled - True to add subharmonics
   */
  setFry(enabled) {
    this.fryEnabled = enabled;
  }

  /**
   * Enable or disable vibrato (sing mode)
   * @param {boolean} enabled - True to add vibrato
   */
  setVibrato(enabled) {
    this.vibratoEnabled = enabled;
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

    // Apply vibrato if enabled
    let currentFreq = this.frequency;
    if (this.vibratoEnabled) {
      this.vibratoPhase += this.vibratoRate / this.sampleRate;
      if (this.vibratoPhase >= 1.0) {
        this.vibratoPhase -= 1.0;
      }
      const vibrato = Math.sin(TWO_PI * this.vibratoPhase);
      currentFreq *= 1.0 + vibrato * this.vibratoDepth;
    }

    this.stepPhase(currentFreq);

    // Generate modified sawtooth wave
    // Add some waveshaping for richer harmonic content
    const naive = this.phase * 2 - 1; // Basic sawtooth (-1 to +1)

    // Apply mild cubic waveshaping to emphasize odd harmonics
    // This makes the waveform more "vocal-like"
    let shaped = naive + 0.15 * (naive * naive * naive);

    // Add vocal fry (subharmonics) if enabled
    if (this.fryEnabled) {
      this.fryPhase += (currentFreq * 0.5) / this.sampleRate; // Octave below
      if (this.fryPhase >= 1.0) {
        this.fryPhase -= 1.0;
      }
      const fry = (this.fryPhase * 2 - 1) * 0.3; // Attenuated subharmonic
      shaped += fry;
    }

    return shaped * 0.5; // Scale down to prevent clipping
  }

  /**
   * Reset phase (useful for hard sync or note-on events)
   */
  reset() {
    this.phase = 0;
    this.fryPhase = 0;
    this.vibratoPhase = 0;
  }
}
