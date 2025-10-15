// FeedbackModule.test.js
import { describe, it, expect, beforeEach } from 'vitest';
import { FeedbackModule } from '../../../src/audio/modules/FeedbackModule.js';
import { TestSignals } from '../../utils/signal-generators.js';
import { SignalAnalysis } from '../../utils/signal-analyzers.js';

describe('FeedbackModule', () => {
    let feedback;

    beforeEach(() => {
        feedback = new FeedbackModule();
    });

    describe('Constructor', () => {
        it('should initialize with default values', () => {
            expect(feedback.delay1Gain).toBe(0.95);
            expect(feedback.delay2Gain).toBe(0.95);
            expect(feedback.filterGain).toBe(0.0);
        });
    });

    describe('Parameter setters', () => {
        it('should set delay1 gain clamped to 0-0.99', () => {
            feedback.setDelay1Gain(0.8);
            expect(feedback.delay1Gain).toBeCloseTo(0.8 * 0.99, 3);

            feedback.setDelay1Gain(1.0);
            expect(feedback.delay1Gain).toBe(0.99);
        });

        it('should set delay2 gain clamped to 0-0.99', () => {
            feedback.setDelay2Gain(0.5);
            expect(feedback.delay2Gain).toBeCloseTo(0.5 * 0.99, 3);
        });

        it('should set filter gain clamped to 0-0.99', () => {
            feedback.setFilterGain(0.3);
            expect(feedback.filterGain).toBeCloseTo(0.3 * 0.99, 3);
        });
    });

    describe('Process', () => {
        it('should mix all three inputs', () => {
            feedback.setDelay1Gain(0.5);
            feedback.setDelay2Gain(0.5);
            feedback.setFilterGain(0.5);

            const result = feedback.process(1.0, 1.0, 1.0);

            // Should be sum of all three with gains applied
            const expected = (1.0 * 0.5 * 0.99) * 3;
            expect(result).toBeCloseTo(expected, 2);
        });

        it('should handle zero inputs', () => {
            const result = feedback.process(0, 0, 0);
            expect(result).toBe(0);
        });

        it('should apply independent gains', () => {
            feedback.setDelay1Gain(1.0);
            feedback.setDelay2Gain(0.0);
            feedback.setFilterGain(0.0);

            const result = feedback.process(1.0, 1.0, 1.0);

            // Only delay1 should contribute
            expect(result).toBeCloseTo(0.99, 2);
        });
    });

    describe('Reset', () => {
        it('should not throw on reset', () => {
            expect(() => feedback.reset()).not.toThrow();
        });
    });

    describe('Signal Behaviour', () => {
        it('should keep summed feedback bounded with dynamic input', () => {
            feedback.setDelay1Gain(0.9);
            feedback.setDelay2Gain(0.6);
            feedback.setFilterGain(0.3);

            const length = 2048;
            const delay1Signal = TestSignals.whiteNoise(length, 0.6);
            const delay2Signal = TestSignals.whiteNoise(length, 0.6);
            const filterSignal = TestSignals.whiteNoise(length, 0.6);

            const output = new Float32Array(length);
            for (let i = 0; i < length; i++) {
                output[i] = feedback.process(
                    delay1Signal[i],
                    delay2Signal[i],
                    filterSignal[i]
                );
            }

            const maxExpectedGain = feedback.delay1Gain + feedback.delay2Gain + feedback.filterGain;
            expect(SignalAnalysis.hasValidOutput(output, maxExpectedGain + 0.5)).toBe(true);
            const peak = SignalAnalysis.peakLevel(output);
            expect(peak).toBeLessThanOrEqual(maxExpectedGain + 0.2);
        });

        it('should sum constant signals proportionally to gains', () => {
            feedback.setDelay1Gain(0.5);
            feedback.setDelay2Gain(0.25);
            feedback.setFilterGain(0.1);

            const constant = TestSignals.step(0.7, 256);
            const output = new Float32Array(constant.length);

            for (let i = 0; i < constant.length; i++) {
                output[i] = feedback.process(constant[i], constant[i], constant[i]);
            }

            const mean = SignalAnalysis.mean(output);
            const expected = 0.7 * (feedback.delay1Gain + feedback.delay2Gain + feedback.filterGain);
            expect(mean).toBeCloseTo(expected, 4);
        });
    });
});
