// FormantBankModule.js
// Cascade of four formant filters (F1, F2, F3, F4)

import { FormantModule } from './FormantModule.js';

export class FormantBankModule {
  constructor(sampleRate) {
    this.sampleRate = sampleRate;

    // Create four formant filters
    this.formants = [
      new FormantModule(sampleRate), // F1
      new FormantModule(sampleRate), // F2
      new FormantModule(sampleRate), // F3
      new FormantModule(sampleRate), // F4
    ];

    // Set default formant frequencies and bandwidths
    // These approximate a neutral vowel (schwa)
    this.setFormant(0, 500, 80);   // F1 - wider bandwidth
    this.setFormant(1, 1500, 120); // F2 - wider bandwidth
    this.setFormant(2, 2500, 150); // F3 - wider bandwidth
    this.setFormant(3, 3500, 200); // F4 - wider bandwidth

    // Gain compensation for cascade
    // With Q-based filters, we need moderate gain compensation
    this.makeupGain = 8.0;
  }

  /**
   * Set a specific formant's frequency and bandwidth
   * @param {number} index - Formant index (0-3 for F1-F4)
   * @param {number} frequency - Center frequency in Hz
   * @param {number} bandwidth - Bandwidth in Hz
   */
  setFormant(index, frequency, bandwidth) {
    if (index >= 0 && index < this.formants.length) {
      this.formants[index].setFrequency(frequency);
      this.formants[index].setBandwidth(bandwidth);
    }
  }

  /**
   * Set formant frequency only
   * @param {number} index - Formant index (0-3)
   * @param {number} frequency - Center frequency in Hz
   */
  setFormantFrequency(index, frequency) {
    if (index >= 0 && index < this.formants.length) {
      this.formants[index].setFrequency(frequency);
    }
  }

  /**
   * Set formant bandwidth only
   * @param {number} index - Formant index (0-3)
   * @param {number} bandwidth - Bandwidth in Hz
   */
  setFormantBandwidth(index, bandwidth) {
    if (index >= 0 && index < this.formants.length) {
      this.formants[index].setBandwidth(bandwidth);
    }
  }

  /**
   * Set formant using Q factor
   * @param {number} index - Formant index (0-3)
   * @param {number} q - Quality factor
   */
  setFormantQ(index, q) {
    if (index >= 0 && index < this.formants.length) {
      this.formants[index].setQ(q);
    }
  }

  /**
   * Load a vowel preset
   * @param {string} vowel - Vowel name ('a', 'e', 'i', 'o', 'u')
   */
  setVowel(vowel) {
    // Typical formant frequencies for vowels (average male voice)
    const presets = {
      'a': { f1: 730, f2: 1090, f3: 2440, f4: 3200 }, // "ah" as in "father"
      'e': { f1: 530, f2: 1840, f3: 2480, f4: 3500 }, // "eh" as in "bed"
      'i': { f1: 270, f2: 2290, f3: 3010, f4: 3500 }, // "ee" as in "see"
      'o': { f1: 570, f2: 840, f3: 2410, f4: 3200 },  // "oh" as in "home"
      'u': { f1: 300, f2: 870, f3: 2240, f4: 3200 },  // "oo" as in "boot"
    };

    const preset = presets[vowel];
    if (preset) {
      this.setFormant(0, preset.f1, 80);
      this.setFormant(1, preset.f2, 120);
      this.setFormant(2, preset.f3, 150);
      this.setFormant(3, preset.f4, 200);
    }
  }

  /**
   * Process one sample through the formant cascade
   * @param {number} input - Input sample (excitation signal)
   * @returns {number} Filtered output
   */
  process(input) {
    // Pass signal through all four formants in series
    let signal = input;

    for (let i = 0; i < this.formants.length; i++) {
      signal = this.formants[i].process(signal);
    }

    // Apply makeup gain to compensate for cascade attenuation
    return signal * this.makeupGain;
  }

  /**
   * Reset all formant filter states
   */
  reset() {
    for (let i = 0; i < this.formants.length; i++) {
      this.formants[i].reset();
    }
  }
}
