// formant.spec.js
// Unit tests for Chatterbox DSP modules

import { describe, it, expect, beforeEach } from 'vitest';
import { FormantModule } from '../src/audio/modules/FormantModule.js';
import { FormantBankModule } from '../src/audio/modules/FormantBankModule.js';
import { LarynxModule } from '../src/audio/modules/LarynxModule.js';
import { AspiratorModule } from '../src/audio/modules/AspiratorModule.js';

describe('FormantModule', () => {
  let formant;

  beforeEach(() => {
    formant = new FormantModule(44100);
  });

  it('should initialize with default parameters', () => {
    expect(formant.frequency).toBe(500);
    expect(formant.bandwidth).toBe(100);
  });

  it('should set frequency within valid range', () => {
    formant.setFrequency(1000);
    expect(formant.frequency).toBe(1000);

    // Test clamping
    formant.setFrequency(50000);
    expect(formant.frequency).toBeLessThanOrEqual(22050); // Nyquist
  });

  it('should set bandwidth within valid range', () => {
    formant.setBandwidth(200);
    expect(formant.bandwidth).toBe(200);

    // Test minimum
    formant.setBandwidth(5);
    expect(formant.bandwidth).toBeGreaterThanOrEqual(10);
  });

  it('should process input without NaN or Infinity', () => {
    const input = 0.5;
    const output = formant.process(input);

    expect(isFinite(output)).toBe(true);
    expect(isNaN(output)).toBe(false);
  });

  it('should reset filter state', () => {
    formant.process(1.0);
    formant.process(0.8);
    formant.process(0.6);

    formant.reset();

    expect(formant.x1).toBe(0);
    expect(formant.x2).toBe(0);
    expect(formant.y1).toBe(0);
    expect(formant.y2).toBe(0);
  });

  it('should attenuate DC component', () => {
    const dcInput = 1.0;
    let sum = 0;

    // Process several DC samples
    for (let i = 0; i < 100; i++) {
      sum += Math.abs(formant.process(dcInput));
    }

    const avgOutput = sum / 100;

    // Bandpass filter should attenuate DC (0 Hz)
    expect(avgOutput).toBeLessThan(0.5);
  });
});

describe('FormantBankModule', () => {
  let bank;

  beforeEach(() => {
    bank = new FormantBankModule(44100);
  });

  it('should initialize with four formants', () => {
    expect(bank.formants.length).toBe(4);
  });

  it('should set individual formant frequencies', () => {
    bank.setFormant(0, 700, 60);
    expect(bank.formants[0].frequency).toBe(700);
    expect(bank.formants[0].bandwidth).toBe(60);
  });

  it('should load vowel presets', () => {
    bank.setVowel('a');
    expect(bank.formants[0].frequency).toBe(730); // F1 for 'a'

    bank.setVowel('i');
    expect(bank.formants[0].frequency).toBe(270); // F1 for 'i'
  });

  it('should process signal through cascade', () => {
    const input = 0.5;
    const output = bank.process(input);

    expect(isFinite(output)).toBe(true);
    expect(isNaN(output)).toBe(false);
  });

  it('should apply makeup gain', () => {
    const input = 0.1;
    const output = bank.process(input);

    // With makeup gain, output should be amplified
    expect(Math.abs(output)).toBeGreaterThan(0);
  });

  it('should reset all formants', () => {
    bank.process(0.5);
    bank.process(0.3);
    bank.reset();

    bank.formants.forEach((formant) => {
      expect(formant.x1).toBe(0);
      expect(formant.y1).toBe(0);
    });
  });
});

describe('LarynxModule', () => {
  let larynx;

  beforeEach(() => {
    larynx = new LarynxModule(44100);
  });

  it('should initialize with default pitch', () => {
    expect(larynx.frequency).toBe(120);
    expect(larynx.enabled).toBe(true);
  });

  it('should set pitch within valid range', () => {
    larynx.setPitch(200);
    expect(larynx.frequency).toBe(200);

    // Test clamping
    larynx.setPitch(5000);
    expect(larynx.frequency).toBeLessThanOrEqual(2000);

    larynx.setPitch(10);
    expect(larynx.frequency).toBeGreaterThanOrEqual(20);
  });

  it('should generate output when enabled', () => {
    larynx.setVoiced(true);
    const output = larynx.process();

    expect(isFinite(output)).toBe(true);
    expect(Math.abs(output)).toBeGreaterThan(0);
  });

  it('should generate zero output when disabled', () => {
    larynx.setVoiced(false);
    const output = larynx.process();

    expect(output).toBe(0);
  });

  it('should generate periodic waveform', () => {
    larynx.setPitch(1000); // 1000 Hz at 44100 samples/sec
    const samplesPerCycle = 44100 / 1000; // ~44 samples

    const samples = [];
    for (let i = 0; i < samplesPerCycle * 2; i++) {
      samples.push(larynx.process());
    }

    // Should see some variation (not all zeros)
    const hasVariation = samples.some((s, i) => i > 0 && Math.abs(s - samples[i - 1]) > 0.01);
    expect(hasVariation).toBe(true);
  });

  it('should reset phase', () => {
    larynx.process();
    larynx.process();
    larynx.reset();

    expect(larynx.phase).toBe(0);
  });
});

describe('AspiratorModule', () => {
  let aspirator;

  beforeEach(() => {
    aspirator = new AspiratorModule(44100);
  });

  it('should initialize disabled with zero level', () => {
    expect(aspirator.enabled).toBe(false);
    expect(aspirator.level).toBe(0);
  });

  it('should set level within valid range', () => {
    aspirator.setLevel(0.5);
    expect(aspirator.level).toBe(0.5);

    aspirator.setLevel(1.5);
    expect(aspirator.level).toBe(1);

    aspirator.setLevel(-0.5);
    expect(aspirator.level).toBe(0);
  });

  it('should generate zero output when disabled', () => {
    aspirator.setAspirated(false);
    aspirator.setLevel(0.5);
    const output = aspirator.process();

    expect(output).toBe(0);
  });

  it('should generate noise when enabled', () => {
    aspirator.setAspirated(true);
    aspirator.setLevel(0.5);

    const samples = [];
    for (let i = 0; i < 100; i++) {
      samples.push(aspirator.process());
    }

    // Noise should have variation
    const avg = samples.reduce((sum, s) => sum + s, 0) / samples.length;
    const variance = samples.reduce((sum, s) => sum + Math.pow(s - avg, 2), 0) / samples.length;

    expect(variance).toBeGreaterThan(0.01); // Should have significant variation
  });

  it('should respect level parameter', () => {
    aspirator.setAspirated(true);

    aspirator.setLevel(0.1);
    const lowSamples = [];
    for (let i = 0; i < 100; i++) {
      lowSamples.push(Math.abs(aspirator.process()));
    }

    aspirator.setLevel(1.0);
    const highSamples = [];
    for (let i = 0; i < 100; i++) {
      highSamples.push(Math.abs(aspirator.process()));
    }

    const lowAvg = lowSamples.reduce((sum, s) => sum + s, 0) / lowSamples.length;
    const highAvg = highSamples.reduce((sum, s) => sum + s, 0) / highSamples.length;

    expect(highAvg).toBeGreaterThan(lowAvg);
  });
});

describe('Integration: Speech Synthesis Chain', () => {
  it('should produce finite output through complete signal chain', () => {
    const sampleRate = 44100;

    const larynx = new LarynxModule(sampleRate);
    const aspirator = new AspiratorModule(sampleRate);
    const formantBank = new FormantBankModule(sampleRate);

    larynx.setPitch(120);
    larynx.setVoiced(true);
    aspirator.setAspirated(true);
    aspirator.setLevel(0.1);

    formantBank.setVowel('a');

    // Process several samples through the complete chain
    for (let i = 0; i < 100; i++) {
      const excitation = larynx.process() + aspirator.process();
      const output = formantBank.process(excitation);

      expect(isFinite(output)).toBe(true);
      expect(isNaN(output)).toBe(false);
    }
  });

  it('should produce different outputs for different vowels', () => {
    const sampleRate = 44100;

    const larynx = new LarynxModule(sampleRate);
    const formantBankA = new FormantBankModule(sampleRate);
    const formantBankI = new FormantBankModule(sampleRate);

    larynx.setPitch(120);
    larynx.setVoiced(true);

    formantBankA.setVowel('a');
    formantBankI.setVowel('i');

    // Generate samples for both vowels
    const samplesA = [];
    const samplesI = [];

    for (let i = 0; i < 100; i++) {
      const excitation = larynx.process();
      samplesA.push(formantBankA.process(excitation));
      samplesI.push(formantBankI.process(excitation));
    }

    // Calculate average absolute values
    const avgA = samplesA.reduce((sum, s) => sum + Math.abs(s), 0) / samplesA.length;
    const avgI = samplesI.reduce((sum, s) => sum + Math.abs(s), 0) / samplesI.length;

    // Different formant settings should produce different energy levels
    expect(Math.abs(avgA - avgI)).toBeGreaterThan(0);
  });
});
