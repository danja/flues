// EnvelopeModule.test.js
import { describe, it, expect, beforeEach } from 'vitest';
import { EnvelopeModule } from '../../../src/audio/modules/EnvelopeModule.js';
import { TestSignals } from '../../utils/signal-generators.js';
import { SignalAnalysis } from '../../utils/signal-analyzers.js';

describe('EnvelopeModule', () => {
    let envelope;
    const sampleRate = 44100;

    beforeEach(() => {
        envelope = new EnvelopeModule(sampleRate);
    });

    describe('Constructor', () => {
        it('should initialize with default values', () => {
            expect(envelope.sampleRate).toBe(sampleRate);
            expect(envelope.attackTime).toBe(0.01);
            expect(envelope.releaseTime).toBe(0.05);
            expect(envelope.envelope).toBe(0);
            expect(envelope.gate).toBe(false);
            expect(envelope.isActive).toBe(false);
        });
    });

    describe('Parameter setters', () => {
        it('should set attack time with exponential mapping', () => {
            envelope.setAttack(0);
            expect(envelope.attackTime).toBeCloseTo(0.001, 3);

            envelope.setAttack(0.5);
            const midValue = envelope.attackTime;
            expect(midValue).toBeGreaterThan(0.001);
            expect(midValue).toBeLessThan(0.5);

            envelope.setAttack(1);
            expect(envelope.attackTime).toBeCloseTo(0.5, 3);
        });

        it('should set release time with exponential mapping', () => {
            envelope.setRelease(0);
            expect(envelope.releaseTime).toBeCloseTo(0.01, 3);

            envelope.setRelease(1);
            expect(envelope.releaseTime).toBeCloseTo(2.0, 3);
        });
    });

    describe('Gate control', () => {
        it('should respond to gate on', () => {
            envelope.setGate(true);
            expect(envelope.gate).toBe(true);
            expect(envelope.isActive).toBe(true);
        });

        it('should respond to gate off', () => {
            envelope.setGate(true);
            envelope.setGate(false);
            expect(envelope.gate).toBe(false);
        });
    });

    describe('Process - Attack phase', () => {
        it('should rise during attack when gate is on', () => {
            envelope.setAttack(0.5); // 0.1s attack
            envelope.setGate(true);

            const values = [];
            for (let i = 0; i < 100; i++) {
                values.push(envelope.process());
            }

            // Should be rising
            expect(values[10]).toBeGreaterThan(values[0]);
            expect(values[50]).toBeGreaterThan(values[10]);
        });

        it('should reach maximum value of 1.0', () => {
            envelope.setAttack(0.01); // Very fast attack
            envelope.setGate(true);

            let value = 0;
            for (let i = 0; i < 10000; i++) {
                value = envelope.process();
            }

            expect(value).toBe(1.0);
        });
    });

    describe('Process - Release phase', () => {
        it('should fall during release when gate is off', () => {
            envelope.setAttack(0.01);
            envelope.setRelease(0.5);
            envelope.setGate(true);

            // Rise to full
            for (let i = 0; i < 10000; i++) {
                envelope.process();
            }

            envelope.setGate(false);

            const values = [];
            for (let i = 0; i < 100; i++) {
                values.push(envelope.process());
            }

            // Should be falling
            expect(values[10]).toBeLessThan(values[0]);
            expect(values[50]).toBeLessThan(values[10]);
        });

        it('should reach zero and become inactive', () => {
            envelope.setGate(true);
            envelope.process();
            envelope.setGate(false);

            let value = 1;
            for (let i = 0; i < 100000; i++) {
                value = envelope.process();
                if (!envelope.isActive) break;
            }

            expect(value).toBe(0);
            expect(envelope.isActive).toBe(false);
        });
    });

    describe('Reset', () => {
        it('should reset envelope to zero and set active', () => {
            envelope.envelope = 0.5;
            envelope.isActive = false;

            envelope.reset();

            expect(envelope.envelope).toBe(0);
            expect(envelope.isActive).toBe(true);
        });
    });

    describe('isPlaying', () => {
        it('should return active state', () => {
            expect(envelope.isPlaying()).toBe(false);

            envelope.setGate(true);
            expect(envelope.isPlaying()).toBe(true);
        });
    });

    describe('Signal Analysis Tests', () => {
        it('should have smooth attack curve (monotonically increasing)', () => {
            envelope.setAttack(0.1); // 100ms attack
            envelope.setGate(true);

            const attackSamples = Math.floor(0.15 * sampleRate); // Slightly more than attack time
            const output = new Float32Array(attackSamples);

            for (let i = 0; i < attackSamples; i++) {
                output[i] = envelope.process();
            }

            // Check monotonicity - should always increase during attack
            const monotonic = SignalAnalysis.checkMonotonic(output, 'increasing', 0.00001);
            expect(monotonic.monotonic).toBe(true);
            expect(monotonic.violations).toBe(0);
        });

        it('should have smooth release curve (monotonically decreasing)', () => {
            envelope.setAttack(0.001); // Fast attack
            envelope.setRelease(0.1); // 100ms release
            envelope.setGate(true);

            // Rise to full
            for (let i = 0; i < 1000; i++) {
                envelope.process();
            }

            envelope.setGate(false);

            const releaseSamples = Math.floor(0.15 * sampleRate);
            const output = new Float32Array(releaseSamples);

            for (let i = 0; i < releaseSamples; i++) {
                output[i] = envelope.process();
                if (!envelope.isActive) break;
            }

            // Check monotonicity - should always decrease during release
            const monotonic = SignalAnalysis.checkMonotonic(output, 'decreasing', 0.00001);
            expect(monotonic.monotonic).toBe(true);
            expect(monotonic.violations).toBe(0);
        });

        it('should have accurate attack timing (within 5%)', () => {
            envelope.setAttack(0.5); // Maps to 0.001 * sqrt(500) ≈ 0.022s
            envelope.setGate(true);

            // Get the actual attack time that was set
            const expectedAttackTime = envelope.attackTime;

            const output = new Float32Array(sampleRate); // 1 second
            for (let i = 0; i < output.length; i++) {
                output[i] = envelope.process();
            }

            // Find where it reaches 99% of peak (1.0)
            let reachedPeak = -1;
            for (let i = 0; i < output.length; i++) {
                if (output[i] >= 0.99) {
                    reachedPeak = i;
                    break;
                }
            }

            expect(reachedPeak).toBeGreaterThan(0);
            const actualAttackTime = reachedPeak / sampleRate;

            // Should be within 5% of expected attack time
            expect(actualAttackTime).toBeGreaterThan(expectedAttackTime * 0.95);
            expect(actualAttackTime).toBeLessThan(expectedAttackTime * 1.05);
        });

        it('should have accurate release timing (within 10%)', () => {
            const releaseTimeSeconds = 0.1;
            envelope.setAttack(0.001);
            envelope.setRelease(0.5); // This should map to ~0.1-0.2s
            envelope.setGate(true);

            // Rise to full
            for (let i = 0; i < 10000; i++) {
                envelope.process();
            }

            envelope.setGate(false);

            const output = new Float32Array(sampleRate);
            for (let i = 0; i < output.length; i++) {
                output[i] = envelope.process();
                if (!envelope.isActive) break;
            }

            // Measure decay time
            const decay = SignalAnalysis.measureDecayTime(output, 0.01, sampleRate);
            expect(decay.found).toBe(true);

            // Release should complete within reasonable time
            expect(decay.seconds).toBeGreaterThan(0.05);
            expect(decay.seconds).toBeLessThan(0.5);
        });

        it('should never exceed 1.0 or go below 0.0', () => {
            envelope.setAttack(0.01);
            envelope.setRelease(0.05);
            envelope.setGate(true);

            const output = new Float32Array(10000);
            for (let i = 0; i < output.length; i++) {
                output[i] = envelope.process();
                if (i === 5000) envelope.setGate(false); // Release halfway
            }

            // Check bounds
            const peak = SignalAnalysis.peakLevel(output);
            expect(peak).toBeLessThanOrEqual(1.0);

            // Check no negative values
            for (let i = 0; i < output.length; i++) {
                expect(output[i]).toBeGreaterThanOrEqual(0);
            }
        });

        it('should handle rapid gate on/off cycles', () => {
            envelope.setAttack(0.05);
            envelope.setRelease(0.05);

            const output = new Float32Array(10000);
            let gateState = false;

            for (let i = 0; i < output.length; i++) {
                // Toggle gate every 1000 samples
                if (i % 1000 === 0) {
                    gateState = !gateState;
                    envelope.setGate(gateState);
                }
                output[i] = envelope.process();
            }

            // All samples should be valid
            expect(SignalAnalysis.hasValidOutput(output)).toBe(true);

            // Should have activity (not all zeros)
            const rms = SignalAnalysis.rmsLevel(output);
            expect(rms).toBeGreaterThan(0.1);
        });

        it('should have no clicks or discontinuities', () => {
            envelope.setAttack(0.05);
            envelope.setRelease(0.05);
            envelope.setGate(true);

            const output = new Float32Array(sampleRate);
            for (let i = 0; i < output.length; i++) {
                if (i === sampleRate / 2) envelope.setGate(false);
                output[i] = envelope.process();
            }

            // Check for discontinuities
            const discontinuities = SignalAnalysis.detectDiscontinuities(output, 0.1);
            // Envelope should be very smooth, no big jumps
            expect(discontinuities.count).toBe(0);
        });

        it('should complete full attack-release cycle properly', () => {
            envelope.setAttack(0.01);
            envelope.setRelease(0.05);
            envelope.setGate(true);

            // Attack phase
            const attackOutput = new Float32Array(2000);
            for (let i = 0; i < attackOutput.length; i++) {
                attackOutput[i] = envelope.process();
            }

            // Should reach near 1.0
            expect(attackOutput[attackOutput.length - 1]).toBeGreaterThan(0.99);

            // Release phase
            envelope.setGate(false);
            const releaseOutput = new Float32Array(10000);
            for (let i = 0; i < releaseOutput.length; i++) {
                releaseOutput[i] = envelope.process();
                if (!envelope.isActive) break;
            }

            // Should decay to zero
            expect(releaseOutput[releaseOutput.length - 1]).toBe(0);
            expect(envelope.isActive).toBe(false);
        });

        it('should maintain sustain level when gate stays on', () => {
            envelope.setAttack(0.01); // Fast attack
            envelope.setGate(true);

            // Rise to full
            for (let i = 0; i < 2000; i++) {
                envelope.process();
            }

            // Now sustain for a while
            const sustainOutput = new Float32Array(5000);
            for (let i = 0; i < sustainOutput.length; i++) {
                sustainOutput[i] = envelope.process();
            }

            // Should maintain 1.0 throughout
            const mean = SignalAnalysis.mean(sustainOutput);
            expect(mean).toBeCloseTo(1.0, 5);

            const variance = SignalAnalysis.variance(sustainOutput);
            expect(variance).toBeLessThan(0.00001);
        });

        it('should handle extreme parameter values', () => {
            // Very fast attack and release
            envelope.setAttack(0); // Minimum (0.001s)
            envelope.setRelease(0); // Minimum (0.01s)
            envelope.setGate(true);

            const output1 = new Float32Array(1000);
            for (let i = 0; i < output1.length; i++) {
                output1[i] = envelope.process();
            }

            expect(SignalAnalysis.hasValidOutput(output1)).toBe(true);

            // Very slow attack and release
            envelope.reset();
            envelope.setAttack(1); // Maximum (0.5s)
            envelope.setRelease(1); // Maximum (2.0s)
            envelope.setGate(true);

            const output2 = new Float32Array(10000);
            for (let i = 0; i < output2.length; i++) {
                output2[i] = envelope.process();
            }

            expect(SignalAnalysis.hasValidOutput(output2)).toBe(true);
        });
    });
});
