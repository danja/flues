// DelayLinesModule.test.js
import { describe, it, expect, beforeEach } from 'vitest';
import { DelayLinesModule } from '../../../src/audio/modules/DelayLinesModule.js';
import { TestSignals } from '../../utils/signal-generators.js';
import { SignalAnalysis } from '../../utils/signal-analyzers.js';

describe('DelayLinesModule', () => {
    let delayLines;
    const sampleRate = 44100;

    beforeEach(() => {
        delayLines = new DelayLinesModule(sampleRate);
    });

    describe('Constructor', () => {
        it('should initialize with default values', () => {
            expect(delayLines.sampleRate).toBe(sampleRate);
            expect(delayLines.tuningSemitones).toBe(0);
            expect(delayLines.ratio).toBe(1.0);
            expect(delayLines.frequency).toBe(440);
        });

        it('should create delay line buffers', () => {
            expect(delayLines.delayLine1).toBeInstanceOf(Float32Array);
            expect(delayLines.delayLine2).toBeInstanceOf(Float32Array);
            expect(delayLines.maxDelayLength).toBeGreaterThan(0);
        });
    });

    describe('Parameter setters', () => {
        it('should set tuning in semitones', () => {
            delayLines.setTuning(0.0); // Min
            expect(delayLines.tuningSemitones).toBeCloseTo(-12, 0);

            delayLines.setTuning(0.5); // Center
            expect(delayLines.tuningSemitones).toBeCloseTo(0, 0);

            delayLines.setTuning(1.0); // Max
            expect(delayLines.tuningSemitones).toBeCloseTo(12, 0);
        });

        it('should set ratio', () => {
            delayLines.setRatio(0.0); // Min
            expect(delayLines.ratio).toBeCloseTo(0.5, 1);

            delayLines.setRatio(0.5); // Center
            expect(delayLines.ratio).toBeCloseTo(1.0, 1);

            delayLines.setRatio(1.0); // Max
            expect(delayLines.ratio).toBeCloseTo(2.0, 1);
        });
    });

    describe('Delay length calculation', () => {
        it('should calculate delay length from frequency', () => {
            delayLines.updateDelayLengths(440);

            const expectedLength = sampleRate / 440;
            expect(delayLines.delayLength1).toBeCloseTo(expectedLength, 0);
        });

        it('should apply tuning offset', () => {
            delayLines.setTuning(0.5 + 1/24); // +1 semitone
            delayLines.updateDelayLengths(440);

            const tunedFreq = 440 * Math.pow(2, 1/12);
            const expectedLength = sampleRate / tunedFreq;
            expect(delayLines.delayLength1).toBeCloseTo(expectedLength, 0);
        });

        it('should apply ratio to delay line 2', () => {
            delayLines.setRatio(0.75); // 1.5x ratio
            delayLines.updateDelayLengths(440);

            expect(delayLines.delayLength2).toBeCloseTo(delayLines.delayLength1 * 1.5, 0);
        });
    });

    describe('Process', () => {
        it('should return dual outputs', () => {
            const result = delayLines.process(1.0, 440);

            expect(result).toHaveProperty('delay1');
            expect(result).toHaveProperty('delay2');
        });

        it('should delay the input signal', () => {
            delayLines.updateDelayLengths(440);

            // Write impulse
            delayLines.process(1.0, 440);

            // Should return zeros initially
            for (let i = 0; i < 10; i++) {
                const result = delayLines.process(0.0, 440);
                expect(Math.abs(result.delay1)).toBeLessThan(0.1);
            }
        });

        it('should use linear interpolation', () => {
            // Set frequency that requires fractional delay
            const result = delayLines.process(1.0, 440.5);

            // Should get interpolated values
            expect(isFinite(result.delay1)).toBe(true);
            expect(isFinite(result.delay2)).toBe(true);
        });
    });

    describe('Reset', () => {
        it('should clear delay line buffers', () => {
            // Fill with data
            for (let i = 0; i < 100; i++) {
                delayLines.process(1.0, 440);
            }

            delayLines.reset();

            // Check buffers are cleared (at least first 100 samples)
            for (let i = 0; i < 100; i++) {
                expect(Math.abs(delayLines.delayLine1[i])).toBeLessThan(0.02);
                expect(Math.abs(delayLines.delayLine2[i])).toBeLessThan(0.02);
            }
        });

        it('should reset write positions', () => {
            delayLines.process(1.0, 440);
            delayLines.process(1.0, 440);

            delayLines.reset();

            expect(delayLines.writePos1).toBe(0);
            expect(delayLines.writePos2).toBe(0);
        });
    });

    describe('Signal Behaviour', () => {
        it('should delay an impulse by the expected amount', () => {
            const frequency = 440;
            delayLines.setRatio(0.5); // centre position (ratio ≈ 1.0)
            delayLines.updateDelayLengths(frequency);

            const expectedDelay = delayLines.delayLength1;
            const impulse = TestSignals.impulse(400, 1.0);
            const outputs = new Float32Array(impulse.length);

            for (let i = 0; i < impulse.length; i++) {
                const { delay1 } = delayLines.process(impulse[i], frequency);
                outputs[i] = delay1;
            }

            const peakIndex = outputs.findIndex(sample => Math.abs(sample) > 0.1);
            expect(peakIndex).toBeGreaterThanOrEqual(Math.floor(expectedDelay) - 2);
            expect(peakIndex).toBeLessThanOrEqual(Math.ceil(expectedDelay) + 2);

            for (let i = 0; i < Math.max(0, peakIndex - 5); i++) {
                expect(Math.abs(outputs[i])).toBeLessThan(0.1);
            }

            expect(SignalAnalysis.hasValidOutput(outputs)).toBe(true);
        });

        it('should extend second delay output when ratio increases', () => {
            const frequency = 440;
            delayLines.setRatio(1.0); // Maps to ratio ≈ 2.0
            delayLines.updateDelayLengths(frequency);

            const expectedDelay1 = delayLines.delayLength1;
            const expectedDelay2 = delayLines.delayLength2;

            const impulse = TestSignals.impulse(800, 1.0);
            const outputs1 = new Float32Array(impulse.length);
            const outputs2 = new Float32Array(impulse.length);

            for (let i = 0; i < impulse.length; i++) {
                const { delay1, delay2 } = delayLines.process(impulse[i], frequency);
                outputs1[i] = delay1;
                outputs2[i] = delay2;
            }

            const peak1 = outputs1.findIndex(sample => Math.abs(sample) > 0.1);
            const peak2 = outputs2.findIndex(sample => Math.abs(sample) > 0.1);

            expect(peak1).toBeGreaterThan(0);
            expect(peak2).toBeGreaterThan(peak1);
            expect(peak2).toBeGreaterThanOrEqual(Math.floor(expectedDelay2) - 2);
            expect(peak2).toBeLessThanOrEqual(Math.ceil(expectedDelay2) + 2);

            expect(expectedDelay2).toBeGreaterThan(expectedDelay1);
        });

        it('should remain stable with white noise input', () => {
            const frequency = 220;
            delayLines.setRatio(0.5);
            delayLines.updateDelayLengths(frequency);

            const noise = TestSignals.whiteNoise(2000, 0.5);
            const outputs = new Float32Array(noise.length);

            for (let i = 0; i < noise.length; i++) {
                const { delay1, delay2 } = delayLines.process(noise[i], frequency);
                outputs[i] = (delay1 + delay2) * 0.5;
            }

            expect(SignalAnalysis.hasValidOutput(outputs)).toBe(true);

            // Skip initial transient
            const rms = SignalAnalysis.rmsLevel(outputs, 100, outputs.length);
            expect(rms).toBeGreaterThan(0.001);
            expect(rms).toBeLessThan(1.0);
        });
    });
});
