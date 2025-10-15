// SourcesModule.test.js
import { describe, it, expect, beforeEach } from 'vitest';
import { SourcesModule } from '../../../src/audio/modules/SourcesModule.js';
import { TestSignals } from '../../utils/signal-generators.js';
import { SignalAnalysis } from '../../utils/signal-analyzers.js';
import { TestHelpers } from '../../utils/test-helpers.js';

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

    describe('Signal Analysis Tests', () => {
        it('should produce stable DC output over extended duration', () => {
            sources.setDCLevel(0.5);
            sources.setNoiseLevel(0);
            sources.setToneLevel(0);

            const output = new Float32Array(10000);
            for (let i = 0; i < output.length; i++) {
                output[i] = sources.process(440);
            }

            // Check mean is close to expected DC level
            const mean = SignalAnalysis.mean(output);
            expect(mean).toBeCloseTo(0.5, 5);

            // Variance should be essentially zero (no drift)
            const variance = SignalAnalysis.variance(output);
            expect(variance).toBeLessThan(0.000001);
        });

        it('should produce noise with approximately zero mean', () => {
            sources.setDCLevel(0);
            sources.setNoiseLevel(1.0);
            sources.setToneLevel(0);

            const output = new Float32Array(10000);
            for (let i = 0; i < output.length; i++) {
                output[i] = sources.process(440);
            }

            // Mean should be close to zero (no bias)
            const mean = SignalAnalysis.mean(output);
            expect(Math.abs(mean)).toBeLessThan(0.05); // Within 5% of zero

            // Should have reasonable variance
            const stdDev = SignalAnalysis.standardDeviation(output);
            expect(stdDev).toBeGreaterThan(0.2); // Noise has spread
            expect(stdDev).toBeLessThan(0.8); // But not too much
        });

        it('should generate sawtooth at correct frequency', () => {
            sources.setDCLevel(0);
            sources.setNoiseLevel(0);
            sources.setToneLevel(1.0);

            const testFrequency = 440;
            const duration = 1.0; // 1 second
            const samples = Math.floor(sampleRate * duration);
            const output = new Float32Array(samples);

            for (let i = 0; i < samples; i++) {
                output[i] = sources.process(testFrequency);
            }

            // Estimate frequency from zero crossings
            const estimatedFreq = SignalAnalysis.estimateFrequency(output, sampleRate);

            // Should be within 1% of target frequency
            expect(estimatedFreq).toBeGreaterThan(testFrequency * 0.99);
            expect(estimatedFreq).toBeLessThan(testFrequency * 1.01);
        });

        it('should not clip when all sources are active', () => {
            sources.setDCLevel(0.5);
            sources.setNoiseLevel(0.3);
            sources.setToneLevel(0.5);

            const output = new Float32Array(1000);
            for (let i = 0; i < output.length; i++) {
                output[i] = sources.process(440);
            }

            // All samples should be valid and within reasonable bounds
            expect(SignalAnalysis.hasValidOutput(output, 10)).toBe(true);

            // Peak shouldn't exceed reasonable limits
            const peak = SignalAnalysis.peakLevel(output);
            expect(peak).toBeLessThan(2.0); // Sum of all sources at max
        });

        it('should maintain sawtooth phase continuity', () => {
            sources.setDCLevel(0);
            sources.setNoiseLevel(0);
            sources.setToneLevel(1.0);

            const output = new Float32Array(1000);
            for (let i = 0; i < output.length; i++) {
                output[i] = sources.process(440);
            }

            // Check for discontinuities (should be smooth sawtooth)
            const discontinuities = SignalAnalysis.detectDiscontinuities(output, 1.5);
            // Sawtooth has one big jump per cycle, but shouldn't have excessive clicks
            expect(discontinuities.count).toBeLessThan(100);
        });

        it('should handle different frequencies correctly', () => {
            sources.setDCLevel(0);
            sources.setNoiseLevel(0);
            sources.setToneLevel(1.0);

            const frequencies = [100, 440, 1000, 5000];

            for (const freq of frequencies) {
                sources.reset();
                const output = new Float32Array(sampleRate); // 1 second

                for (let i = 0; i < output.length; i++) {
                    output[i] = sources.process(freq);
                }

                const estimatedFreq = SignalAnalysis.estimateFrequency(output, sampleRate);
                expect(estimatedFreq).toBeGreaterThan(freq * 0.95);
                expect(estimatedFreq).toBeLessThan(freq * 1.05);
            }
        });
    });
});
