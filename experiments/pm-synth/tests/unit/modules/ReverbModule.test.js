// ReverbModule.test.js
import { describe, it, expect, beforeEach } from 'vitest';
import { ReverbModule } from '../../../src/audio/modules/ReverbModule.js';
import { TestSignals } from '../../utils/signal-generators.js';
import { SignalAnalysis } from '../../utils/signal-analyzers.js';

describe('ReverbModule', () => {
    let reverb;
    const sampleRate = 44100;

    beforeEach(() => {
        reverb = new ReverbModule(sampleRate);
    });

    describe('Constructor', () => {
        it('should initialize with default values', () => {
            expect(reverb.sampleRate).toBe(sampleRate);
            expect(reverb.size).toBe(0.5);
            expect(reverb.level).toBe(0.3);
        });

        it('should create comb filter buffers', () => {
            expect(reverb.combBuffers).toHaveLength(4);
            reverb.combBuffers.forEach(buffer => {
                expect(buffer).toBeInstanceOf(Float32Array);
                expect(buffer.length).toBeGreaterThan(0);
            });
        });

        it('should create allpass filter buffers', () => {
            expect(reverb.allpassBuffers).toHaveLength(2);
            reverb.allpassBuffers.forEach(buffer => {
                expect(buffer).toBeInstanceOf(Float32Array);
                expect(buffer.length).toBeGreaterThan(0);
            });
        });
    });

    describe('Parameter setters', () => {
        it('should set size with clamping', () => {
            reverb.setSize(0.7);
            expect(reverb.size).toBe(0.7);

            reverb.setSize(1.5);
            expect(reverb.size).toBe(1.0);

            reverb.setSize(-0.1);
            expect(reverb.size).toBe(0);
        });

        it('should set level with clamping', () => {
            reverb.setLevel(0.5);
            expect(reverb.level).toBe(0.5);

            reverb.setLevel(1.5);
            expect(reverb.level).toBe(1.0);

            reverb.setLevel(-0.1);
            expect(reverb.level).toBe(0);
        });
    });

    describe('Process', () => {
        it('should return a number', () => {
            const result = reverb.process(1.0);
            expect(typeof result).toBe('number');
            expect(isFinite(result)).toBe(true);
        });

        it('should pass through dry signal when level is 0', () => {
            reverb.setLevel(0);
            const input = 0.5;
            const result = reverb.process(input);
            expect(result).toBeCloseTo(input, 5);
        });

        it('should process signal with reverb level applied', () => {
            reverb.setLevel(1.0);

            const input = 0.5;
            const result = reverb.process(input);

            // With 100% wet, output should differ from dry input
            expect(result).not.toBeCloseTo(input, 5);
            expect(isFinite(result)).toBe(true);
        });

        it('should handle continuous input', () => {
            reverb.setLevel(0.5);
            reverb.setSize(0.7);

            for (let i = 0; i < 1000; i++) {
                const input = Math.sin(i * 0.1);
                const output = reverb.process(input);
                expect(isFinite(output)).toBe(true);
                expect(Math.abs(output)).toBeLessThan(10);
            }
        });
    });

    describe('Reset', () => {
        it('should clear all comb filter buffers', () => {
            for (let i = 0; i < 500; i++) {
                reverb.process(Math.random());
            }

            reverb.reset();

            reverb.combBuffers.forEach(buffer => {
                const sum = buffer.reduce((a, b) => a + Math.abs(b), 0);
                expect(sum).toBe(0);
            });
        });

        it('should clear all allpass filter buffers', () => {
            for (let i = 0; i < 500; i++) {
                reverb.process(Math.random());
            }

            reverb.reset();

            reverb.allpassBuffers.forEach(buffer => {
                const sum = buffer.reduce((a, b) => a + Math.abs(b), 0);
                expect(sum).toBe(0);
            });
        });

        it('should produce silence after reset', () => {
            for (let i = 0; i < 500; i++) {
                reverb.process(Math.random());
            }

            reverb.reset();

            const outputs = [];
            for (let i = 0; i < 100; i++) {
                outputs.push(reverb.process(0));
            }

            outputs.forEach(output => {
                expect(Math.abs(output)).toBeLessThan(0.001);
            });
        });
    });

    describe('Signal Behaviour', () => {
        it('should produce a decaying impulse response', () => {
            reverb.setLevel(1.0);
            reverb.setSize(0.5);

            const length = 8000;
            const output = new Float32Array(length);

            for (let i = 0; i < length; i++) {
                const input = i === 0 ? 1.0 : 0.0;
                output[i] = reverb.process(input);
            }

            expect(SignalAnalysis.hasValidOutput(output)).toBe(true);

            const peak = SignalAnalysis.peakLevel(output);
            const buildRMS = SignalAnalysis.rmsLevel(output, 500, 1500);
            const sustainRMS = SignalAnalysis.rmsLevel(output, 2500, 3500);
            const decayRMS = SignalAnalysis.rmsLevel(output, 4500, 5500);

            expect(peak).toBeGreaterThan(0.001);
            expect(buildRMS).toBeGreaterThan(0.0001);
            expect(sustainRMS).toBeGreaterThan(0.0001);
            expect(decayRMS).toBeLessThan(sustainRMS);
        });

        it('should extend decay time as size increases', () => {
            const length = 10000;
            reverb.setLevel(1.0);

            reverb.setSize(0.2);
            reverb.reset();
            const shortOutput = new Float32Array(length);
            for (let i = 0; i < length; i++) {
                const input = i === 0 ? 1.0 : 0.0;
                shortOutput[i] = reverb.process(input);
            }
            const shortTail = SignalAnalysis.rmsLevel(shortOutput, 4000, 5000);

            reverb.setSize(0.9);
            reverb.reset();
            const longOutput = new Float32Array(length);
            for (let i = 0; i < length; i++) {
                const input = i === 0 ? 1.0 : 0.0;
                longOutput[i] = reverb.process(input);
            }
            const longTail = SignalAnalysis.rmsLevel(longOutput, 4000, 5000);

            expect(longTail).toBeGreaterThan(shortTail);
        });

        it('should honour the wet/dry mix level', () => {
            const signal = TestSignals.sine(440, sampleRate, 4096, 0.5);
            const analysisStart = 256;

            reverb.setSize(0.6);

            reverb.reset();
            reverb.setLevel(0.0);
            const dryOutput = new Float32Array(signal.length);
            for (let i = 0; i < signal.length; i++) {
                dryOutput[i] = reverb.process(signal[i]);
            }

            reverb.reset();
            reverb.setLevel(1.0);
            const wetOutput = new Float32Array(signal.length);
            for (let i = 0; i < signal.length; i++) {
                wetOutput[i] = reverb.process(signal[i]);
            }

            reverb.reset();
            reverb.setLevel(0.5);
            const mixedOutput = new Float32Array(signal.length);
            for (let i = 0; i < signal.length; i++) {
                mixedOutput[i] = reverb.process(signal[i]);
            }

            for (let i = analysisStart; i < mixedOutput.length; i++) {
                const expectedSample = signal[i] * 0.5 + wetOutput[i] * 0.5;
                expect(Math.abs(mixedOutput[i] - expectedSample)).toBeLessThan(0.001);
            }
        });
    });
});
