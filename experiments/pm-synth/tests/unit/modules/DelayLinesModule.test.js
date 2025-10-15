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
});
