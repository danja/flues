// FilterModule.test.js
import { describe, it, expect, beforeEach } from 'vitest';
import { FilterModule } from '../../../src/audio/modules/FilterModule.js';

describe('FilterModule', () => {
    let filter;
    const sampleRate = 44100;

    beforeEach(() => {
        filter = new FilterModule(sampleRate);
    });

    describe('Constructor', () => {
        it('should initialize with default values', () => {
            expect(filter.sampleRate).toBe(sampleRate);
            expect(filter.frequency).toBe(1000);
            expect(filter.q).toBe(1.0);
            expect(filter.shape).toBe(0.0);
            expect(filter.low).toBe(0);
            expect(filter.band).toBe(0);
            expect(filter.high).toBe(0);
        });
    });

    describe('Parameter setters', () => {
        it('should set frequency with exponential mapping', () => {
            filter.setFrequency(0);
            expect(filter.frequency).toBeCloseTo(20, 0);

            filter.setFrequency(1);
            expect(filter.frequency).toBeCloseTo(20000, -2);
        });

        it('should set Q with exponential mapping', () => {
            filter.setQ(0);
            expect(filter.q).toBeCloseTo(0.5, 1);

            filter.setQ(1);
            expect(filter.q).toBeCloseTo(20, 0);
        });

        it('should set shape within range', () => {
            filter.setShape(0.5);
            expect(filter.shape).toBe(0.5);

            filter.setShape(1.5);
            expect(filter.shape).toBe(1.0);

            filter.setShape(-0.5);
            expect(filter.shape).toBe(0.0);
        });
    });

    describe('Process', () => {
        it('should output lowpass at shape=0', () => {
            filter.setShape(0.0);
            filter.setFrequency(0.5);

            // Process some samples
            const outputs = [];
            for (let i = 0; i < 100; i++) {
                outputs.push(filter.process(Math.sin(i * 0.1)));
            }

            // Should produce output
            const hasOutput = outputs.some(val => Math.abs(val) > 0.001);
            expect(hasOutput).toBe(true);
        });

        it('should output bandpass at shape=0.5', () => {
            filter.setShape(0.5);
            filter.setFrequency(0.5);

            const outputs = [];
            for (let i = 0; i < 100; i++) {
                outputs.push(filter.process(Math.sin(i * 0.1)));
            }

            const hasOutput = outputs.some(val => Math.abs(val) > 0.001);
            expect(hasOutput).toBe(true);
        });

        it('should output highpass at shape=1.0', () => {
            filter.setShape(1.0);
            filter.setFrequency(0.5);

            const outputs = [];
            for (let i = 0; i < 100; i++) {
                outputs.push(filter.process(Math.sin(i * 0.1)));
            }

            const hasOutput = outputs.some(val => Math.abs(val) > 0.001);
            expect(hasOutput).toBe(true);
        });

        it('should handle NaN/infinity gracefully', () => {
            const output = filter.process(Infinity);
            expect(isFinite(output)).toBe(true);
        });
    });

    describe('Reset', () => {
        it('should reset filter state', () => {
            // Process to build up state
            for (let i = 0; i < 100; i++) {
                filter.process(1.0);
            }

            filter.reset();

            expect(filter.low).toBe(0);
            expect(filter.band).toBe(0);
            expect(filter.high).toBe(0);
        });
    });
});
