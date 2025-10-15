// ReverbModule.test.js
import { describe, it, expect, beforeEach } from 'vitest';
import { ReverbModule } from '../../../src/audio/modules/ReverbModule.js';

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
});
