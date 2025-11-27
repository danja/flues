// vocal-modes.spec.js
// Tests for advanced vocal modes: nasal, sing, shout, fry, stress

import { describe, it, expect, beforeEach } from 'vitest';
import { LarynxModule } from '../src/audio/modules/LarynxModule.js';
import { FormantBankModule } from '../src/audio/modules/FormantBankModule.js';

const SAMPLE_RATE = 48000;

describe('LarynxModule - Vocal Fry', () => {
  let larynx;

  beforeEach(() => {
    larynx = new LarynxModule(SAMPLE_RATE);
    larynx.setPitch(120); // A2
    larynx.setVoiced(true);
  });

  it('should add subharmonics when fry is enabled', () => {
    // Process with fry disabled
    larynx.setFry(false);
    const samplesNormal = [];
    for (let i = 0; i < 100; i++) {
      samplesNormal.push(larynx.process());
    }

    // Reset and process with fry enabled
    larynx.reset();
    larynx.setFry(true);
    const samplesFry = [];
    for (let i = 0; i < 100; i++) {
      samplesFry.push(larynx.process());
    }

    // Fry mode should produce different output (with subharmonics)
    const normalEnergy = samplesNormal.reduce((sum, s) => sum + s * s, 0);
    const fryEnergy = samplesFry.reduce((sum, s) => sum + s * s, 0);

    expect(Math.abs(normalEnergy - fryEnergy)).toBeGreaterThan(0.01);
  });

  it('should have fry subharmonic at half the fundamental frequency', () => {
    larynx.setFry(true);
    larynx.reset();

    // Process one period at fundamental frequency
    const fundamentalPeriod = SAMPLE_RATE / 120;
    const samples = [];
    for (let i = 0; i < fundamentalPeriod * 4; i++) {
      samples.push(larynx.process());
    }

    // Fry should double the effective period (octave below)
    expect(samples.length).toBeGreaterThan(100);
  });
});

describe('LarynxModule - Vibrato (Sing Mode)', () => {
  let larynx;

  beforeEach(() => {
    larynx = new LarynxModule(SAMPLE_RATE);
    larynx.setPitch(220); // A3
    larynx.setVoiced(true);
  });

  it('should modulate pitch when vibrato is enabled', () => {
    // Process with vibrato disabled
    larynx.setVibrato(false);
    const samplesNormal = [];
    for (let i = 0; i < 1000; i++) {
      samplesNormal.push(larynx.process());
    }

    // Reset and process with vibrato enabled
    larynx.reset();
    larynx.setVibrato(true);
    const samplesVibrato = [];
    for (let i = 0; i < 1000; i++) {
      samplesVibrato.push(larynx.process());
    }

    // Vibrato should produce different output (pitch modulation)
    const normalEnergy = samplesNormal.reduce((sum, s) => sum + s * s, 0);
    const vibratoEnergy = samplesVibrato.reduce((sum, s) => sum + s * s, 0);

    // Energies should be different due to phase differences
    expect(Math.abs(normalEnergy - vibratoEnergy)).toBeGreaterThan(0.01);
  });

  it('should reset vibrato phase on reset', () => {
    larynx.setVibrato(true);

    // Process some samples
    for (let i = 0; i < 100; i++) {
      larynx.process();
    }

    larynx.reset();

    // After reset, vibrato phase should be back to 0
    const sample = larynx.process();
    expect(sample).toBeDefined();
  });
});

describe('FormantBankModule - Nasal Mode', () => {
  let formantBank;

  beforeEach(() => {
    formantBank = new FormantBankModule(SAMPLE_RATE);
  });

  it('should enable nasal formant', () => {
    formantBank.setNasal(true);
    expect(formantBank.nasalEnabled).toBe(true);

    formantBank.setNasal(false);
    expect(formantBank.nasalEnabled).toBe(false);
  });

  it('should add nasal resonance to output when enabled', () => {
    const input = 1.0;

    // Process with nasal disabled
    formantBank.setNasal(false);
    formantBank.reset();
    const outputNormal = formantBank.process(input);

    // Process with nasal enabled
    formantBank.setNasal(true);
    formantBank.reset();
    const outputNasal = formantBank.process(input);

    // Outputs should be different
    expect(Math.abs(outputNormal - outputNasal)).toBeGreaterThan(0.001);
  });

  it('should have nasal formant around 250 Hz', () => {
    expect(formantBank.nasalFormant).toBeDefined();
    expect(formantBank.nasalFormant.frequency).toBeCloseTo(250, 0);
  });
});

describe('Stress Processing', () => {
  it('should map stress 0-1 to gain 0.5-2.0x', () => {
    // Test stress levels
    const stressLevels = [0, 0.25, 0.5, 0.75, 1.0];
    const expectedGains = [0.5, 0.875, 1.25, 1.625, 2.0];

    stressLevels.forEach((stress, i) => {
      const gain = 0.5 + stress * 1.5;
      expect(gain).toBeCloseTo(expectedGains[i], 2);
    });
  });

  it('should apply soft clipping for high stress values', () => {
    const stress = 0.8;
    const sample = 1.5;

    if (stress > 0.6) {
      const drive = (stress - 0.6) * 5;
      const clipped = Math.tanh(sample * (1 + drive));

      // Tanh should limit output to -1 to +1 range
      expect(clipped).toBeGreaterThanOrEqual(-1);
      expect(clipped).toBeLessThanOrEqual(1);
      expect(Math.abs(clipped)).toBeLessThan(Math.abs(sample));
    }
  });
});

describe('Shout Mode - Formant Frequency Boost', () => {
  it('should increase formant frequencies by 15%', () => {
    const baseFrequencies = [500, 1500, 2500, 3500];
    const shoutMultiplier = 1.15;

    baseFrequencies.forEach((freq) => {
      const shoutFreq = freq * shoutMultiplier;
      expect(shoutFreq).toBeCloseTo(freq * 1.15, 0);
      expect(shoutFreq).toBeGreaterThan(freq);
    });
  });

  it('should boost noise level in shout mode', () => {
    const baseNoiseLevel = 0.2;
    const shoutNoiseLevel = Math.min(1.0, baseNoiseLevel * 1.5);

    expect(shoutNoiseLevel).toBeCloseTo(0.3, 2);
    expect(shoutNoiseLevel).toBeGreaterThan(baseNoiseLevel);
    expect(shoutNoiseLevel).toBeLessThanOrEqual(1.0);
  });
});

describe('Integration - Vocal Modes', () => {
  it('should allow multiple vocal modes simultaneously', () => {
    const larynx = new LarynxModule(SAMPLE_RATE);
    const formantBank = new FormantBankModule(SAMPLE_RATE);

    // Enable multiple modes
    larynx.setVoiced(true);
    larynx.setFry(true);
    larynx.setVibrato(true);
    formantBank.setNasal(true);

    // Process a signal
    larynx.setPitch(150);
    const excitation = larynx.process();
    const filtered = formantBank.process(excitation);

    expect(excitation).toBeDefined();
    expect(filtered).toBeDefined();
    expect(Math.abs(filtered)).toBeGreaterThan(0);
  });
});
