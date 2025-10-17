// InterfaceModule.test.js
import { describe, it, expect, beforeEach } from 'vitest';
import { InterfaceModule, InterfaceType } from '../../../src/audio/modules/InterfaceModule.js';
import { TestSignals } from '../../utils/signal-generators.js';
import { SignalAnalysis } from '../../utils/signal-analyzers.js';

describe('InterfaceModule', () => {
    let interface_;

    beforeEach(() => {
        interface_ = new InterfaceModule();
    });

    describe('Constructor', () => {
        it('should initialize with default values', () => {
            expect(interface_.getType()).toBe(InterfaceType.REED);
            expect(interface_.getIntensity()).toBe(0.5);
        });
    });

    describe('Parameter setters', () => {
        it('should set interface type', () => {
            interface_.setType(InterfaceType.PLUCK);
            expect(interface_.getType()).toBe(InterfaceType.PLUCK);

            interface_.setType(InterfaceType.BRASS);
            expect(interface_.getType()).toBe(InterfaceType.BRASS);
        });

        it('should clamp intensity to 0-1 range', () => {
            interface_.setIntensity(0.8);
            expect(interface_.getIntensity()).toBe(0.8);

            interface_.setIntensity(1.5);
            expect(interface_.getIntensity()).toBe(1.0);

            interface_.setIntensity(-0.5);
            expect(interface_.getIntensity()).toBe(0.0);
        });
    });

    describe('Pluck interface', () => {
        beforeEach(() => {
            interface_.setType(InterfaceType.PLUCK);
        });

        it('should pass initial impulse', () => {
            const input = 0.5;
            const output = interface_.process(input);
            expect(output).toBe(input);
        });

        it('should dampen subsequent signals', () => {
            interface_.setIntensity(0.5);
            interface_.process(1.0); // Initial peak

            const output = interface_.process(0.5);
            expect(output).toBeLessThan(0.5);
        });
    });

    describe('Hit interface', () => {
        beforeEach(() => {
            interface_.setType(InterfaceType.HIT);
        });

        it('should apply hard waveshaping', () => {
            interface_.setIntensity(0.8);
            const output = interface_.process(0.5);
            expect(Math.abs(output)).toBeLessThanOrEqual(1.0);
        });
    });

    describe('Reed interface', () => {
        beforeEach(() => {
            interface_.setType(InterfaceType.REED);
        });

        it('should apply nonlinear transfer', () => {
            interface_.setIntensity(0.5);

            const output1 = interface_.process(0.1);
            const output2 = interface_.process(0.2);

            // Should be nonlinear (not simply 2x)
            expect(output2 / output1).not.toBeCloseTo(2.0, 1);
        });

        it('should saturate at high inputs', () => {
            interface_.setIntensity(0.5);
            const output = interface_.process(10.0);
            expect(Math.abs(output)).toBeLessThanOrEqual(1.0);
        });
    });

    describe('Flute interface', () => {
        beforeEach(() => {
            interface_.setType(InterfaceType.FLUTE);
        });

        it('should apply soft saturation', () => {
            interface_.setIntensity(0.5);
            const output = interface_.process(0.5);
            expect(Math.abs(output)).toBeLessThan(0.5);
        });
    });

    describe('Brass interface', () => {
        beforeEach(() => {
            interface_.setType(InterfaceType.BRASS);
        });

        it('should have asymmetric response', () => {
            interface_.setIntensity(0.5);

            const outputPos = interface_.process(0.5);
            const outputNeg = interface_.process(-0.5);

            expect(Math.abs(outputPos)).not.toBeCloseTo(Math.abs(outputNeg), 1);
            expect(Math.abs(outputPos)).toBeLessThanOrEqual(1);
            expect(Math.abs(outputNeg)).toBeLessThanOrEqual(1);
        });
    });

    describe('Bow interface', () => {
        beforeEach(() => {
            interface_.setType(InterfaceType.BOW);
        });

        it('should respond smoothly to continuous input', () => {
            interface_.setIntensity(0.5);
            const sine = TestSignals.sine(10, 44100, 512, 0.5);
            const output = new Float32Array(sine.length);
            for (let i = 0; i < sine.length; i++) {
                output[i] = interface_.process(sine[i]);
            }
            expect(SignalAnalysis.hasValidOutput(output, 1)).toBe(true);
            const variance = SignalAnalysis.variance(output);
            expect(variance).toBeGreaterThan(0.00001); // Bow state builds up gradually
        });
    });

    describe('Bell interface', () => {
        beforeEach(() => {
            interface_.setType(InterfaceType.BELL);
        });

        it('should introduce metallic overtones', () => {
            interface_.setIntensity(0.7);
            const burst = TestSignals.impulse(256, 0.8);
            const output = new Float32Array(burst.length);
            for (let i = 0; i < burst.length; i++) {
                output[i] = interface_.process(burst[i]);
            }
            expect(SignalAnalysis.hasValidOutput(output, 1)).toBe(true);
            const crest = SignalAnalysis.crestFactor(output);
            expect(crest).toBeGreaterThan(1.2);
        });
    });

    describe('Drum interface', () => {
        beforeEach(() => {
            interface_.setType(InterfaceType.DRUM);
        });

        it('should build energy with loud hits', () => {
            interface_.setIntensity(0.8);
            const hits = TestSignals.square(60, 44100, 512, 0.6);
            const output = new Float32Array(hits.length);
            for (let i = 0; i < hits.length; i++) {
                output[i] = interface_.process(hits[i]);
            }
            expect(SignalAnalysis.hasValidOutput(output, 1.5)).toBe(true);
            const rms = SignalAnalysis.rmsLevel(output);
            expect(rms).toBeGreaterThan(0.2);
        });
    });

    describe('Reset', () => {
        it('should reset pluck state', () => {
            interface_.setType(InterfaceType.PLUCK);
            interface_.process(1.0);
            const output1 = interface_.process(0.5);

            interface_.reset();
            const output2 = interface_.process(0.5);

            // After reset, behavior should be like initial condition
            expect(output2).toBe(0.5); // First sample passes through
        });
    });

    describe('Signal Behaviour', () => {
        it('should dampen sustained energy in pluck mode after the initial strike', () => {
            const module = new InterfaceModule();
            module.setType(InterfaceType.PLUCK);
            module.setIntensity(1.0);

            const impulse = TestSignals.impulse(128, 1.0);
            const output = new Float32Array(impulse.length);

            for (let i = 0; i < impulse.length; i++) {
                output[i] = module.process(impulse[i]);
            }

            const firstSample = Math.abs(output[0]);
            let maxAfter = 0;
            for (let i = 1; i < output.length; i++) {
                const value = Math.abs(output[i]);
                if (value > maxAfter) maxAfter = value;
            }

            expect(firstSample).toBeGreaterThan(0);
            expect(maxAfter).toBeLessThan(firstSample * 0.7);
        });

        it('should keep hit output bounded under heavy excitation', () => {
            const module = new InterfaceModule();
            module.setType(InterfaceType.HIT);
            module.setIntensity(1.0);

            const noise = TestSignals.whiteNoise(2000, 1.0);
            const output = new Float32Array(noise.length);

            for (let i = 0; i < noise.length; i++) {
                output[i] = module.process(noise[i]);
            }

            expect(SignalAnalysis.hasValidOutput(output, 1.2)).toBe(true);
            const peak = SignalAnalysis.peakLevel(output);
            expect(peak).toBeLessThanOrEqual(1.0);
        });

        it('should increase output level with reed intensity', () => {
            const module = new InterfaceModule();
            module.setType(InterfaceType.REED);

            module.setIntensity(0.1);
            const lowIntensity = module.process(0.5);

            module.setIntensity(0.9);
            const highIntensity = module.process(0.5);

            expect(Math.abs(highIntensity)).toBeGreaterThan(Math.abs(lowIntensity));
            expect(Math.abs(highIntensity)).toBeLessThanOrEqual(1.0);
        });

        it('should softly saturate in flute mode', () => {
            const module = new InterfaceModule();
            module.setType(InterfaceType.FLUTE);
            module.setIntensity(1.0);

            const sine = TestSignals.sine(440, 44100, 512, 0.8);
            const output = new Float32Array(sine.length);

            for (let i = 0; i < sine.length; i++) {
                output[i] = module.process(sine[i]);
            }

            const peak = SignalAnalysis.peakLevel(output);
            expect(peak).toBeLessThan(0.8);
            expect(SignalAnalysis.hasValidOutput(output, 1.0)).toBe(true);
        });

        it('should introduce asymmetry in brass mode over time', () => {
            const module = new InterfaceModule();
            module.setType(InterfaceType.BRASS);
            module.setIntensity(1.0);

            const sine = TestSignals.sine(220, 44100, 2048, 0.7);
            const output = new Float32Array(sine.length);

            for (let i = 0; i < sine.length; i++) {
                output[i] = module.process(sine[i]);
            }

            const mean = SignalAnalysis.mean(output);
            expect(Math.abs(mean)).toBeGreaterThan(0.01);
        });
    });
});
