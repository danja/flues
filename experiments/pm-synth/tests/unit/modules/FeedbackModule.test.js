// FeedbackModule.test.js
import { describe, it, expect, beforeEach } from 'vitest';
import { FeedbackModule } from '../../../src/audio/modules/FeedbackModule.js';

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
});
