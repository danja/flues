// EnvelopeModule.test.js
import { describe, it, expect, beforeEach } from 'vitest';
import { EnvelopeModule } from '../../../src/audio/modules/EnvelopeModule.js';

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
});
