// SourcesModule.test.js
import { describe, it, expect, beforeEach } from 'vitest';
import { SourcesModule } from '../../../src/audio/modules/SourcesModule.js';

describe('SourcesModule', () => {
    let sources;
    const sampleRate = 44100;

    beforeEach(() => {
        sources = new SourcesModule(sampleRate);
    });

    describe('Constructor', () => {
        it('should initialize with default values', () => {
            expect(sources.sampleRate).toBe(sampleRate);
            expect(sources.dcLevel).toBe(0.5);
            expect(sources.noiseLevel).toBe(0.15);
            expect(sources.toneLevel).toBe(0.0);
            expect(sources.sawtoothPhase).toBe(0);
        });
    });

    describe('Parameter setters', () => {
        it('should set DC level within range', () => {
            sources.setDCLevel(0.8);
            expect(sources.dcLevel).toBe(0.8);

            sources.setDCLevel(1.5); // Over max
            expect(sources.dcLevel).toBe(1.0);

            sources.setDCLevel(-0.5); // Under min
            expect(sources.dcLevel).toBe(0.0);
        });

        it('should set noise level within range', () => {
            sources.setNoiseLevel(0.3);
            expect(sources.noiseLevel).toBe(0.3);
        });

        it('should set tone level within range', () => {
            sources.setToneLevel(0.5);
            expect(sources.toneLevel).toBe(0.5);
        });
    });

    describe('Process', () => {
        it('should generate DC component', () => {
            sources.setDCLevel(0.5);
            sources.setNoiseLevel(0);
            sources.setToneLevel(0);

            const output = sources.process(440);
            expect(output).toBeCloseTo(0.5, 10);
        });

        it('should generate output with noise', () => {
            sources.setDCLevel(0);
            sources.setNoiseLevel(0.5);
            sources.setToneLevel(0);

            const outputs = [];
            for (let i = 0; i < 100; i++) {
                outputs.push(sources.process(440));
            }

            // Noise should vary
            const variance = outputs.reduce((sum, val) => sum + val * val, 0) / outputs.length;
            expect(variance).toBeGreaterThan(0);
        });

        it('should generate sawtooth tone', () => {
            sources.setDCLevel(0);
            sources.setNoiseLevel(0);
            sources.setToneLevel(1.0);

            const output1 = sources.process(440);
            const output2 = sources.process(440);

            // Sawtooth should be different values (phase advancing)
            expect(output1).not.toBe(output2);
        });

        it('should sum all sources', () => {
            sources.setDCLevel(0.3);
            sources.setNoiseLevel(0.0);
            sources.setToneLevel(0.0);

            const output = sources.process(440);
            expect(Math.abs(output - 0.3)).toBeLessThan(0.1); // Should be close to DC value
        });
    });

    describe('Reset', () => {
        it('should reset sawtooth phase to zero', () => {
            sources.process(440);
            sources.process(440);
            expect(sources.sawtoothPhase).toBeGreaterThan(0);

            sources.reset();
            expect(sources.sawtoothPhase).toBe(0);
        });
    });
});
