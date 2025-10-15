// ModulationModule.test.js
import { describe, it, expect, beforeEach } from 'vitest';
import { ModulationModule } from '../../../src/audio/modules/ModulationModule.js';
import { SignalAnalysis } from '../../utils/signal-analyzers.js';

describe('ModulationModule', () => {
    let modulation;
    const sampleRate = 44100;

    beforeEach(() => {
        modulation = new ModulationModule(sampleRate);
    });

    describe('Constructor', () => {
        it('should initialize with default values', () => {
            expect(modulation.sampleRate).toBe(sampleRate);
            expect(modulation.lfoFrequency).toBe(5);
            expect(modulation.lfoPhase).toBe(0);
            expect(modulation.typeLevel).toBe(0.5);
            expect(modulation.amDepth).toBe(0);
            expect(modulation.fmDepth).toBe(0);
        });
    });

    describe('Parameter setters', () => {
        it('should set LFO frequency with exponential mapping', () => {
            modulation.setFrequency(0);
            expect(modulation.lfoFrequency).toBeCloseTo(0.1, 1);

            modulation.setFrequency(1);
            expect(modulation.lfoFrequency).toBeCloseTo(20, 0);
        });

        it('should set AM mode when typeLevel < 0.5', () => {
            modulation.setTypeLevel(0.25);
            expect(modulation.typeLevel).toBe(0.25);
            expect(modulation.amDepth).toBeGreaterThan(0);
            expect(modulation.fmDepth).toBe(0);
        });

        it('should set FM mode when typeLevel > 0.5', () => {
            modulation.setTypeLevel(0.75);
            expect(modulation.typeLevel).toBe(0.75);
            expect(modulation.amDepth).toBe(0);
            expect(modulation.fmDepth).toBeGreaterThan(0);
        });

        it('should have no modulation at typeLevel = 0.5', () => {
            modulation.setTypeLevel(0.5);
            expect(modulation.amDepth).toBe(0);
            expect(modulation.fmDepth).toBe(0);
        });
    });

    describe('Process', () => {
        it('should generate LFO between -1 and 1', () => {
            const outputs = [];
            for (let i = 0; i < 1000; i++) {
                const result = modulation.process();
                outputs.push(result.lfo);
            }

            const max = Math.max(...outputs);
            const min = Math.min(...outputs);

            expect(max).toBeLessThanOrEqual(1);
            expect(min).toBeGreaterThanOrEqual(-1);
        });

        it('should generate AM multiplier around 1', () => {
            modulation.setTypeLevel(0.0); // Max AM

            const outputs = [];
            for (let i = 0; i < 1000; i++) {
                const result = modulation.process();
                outputs.push(result.am);
            }

            const max = Math.max(...outputs);
            const min = Math.min(...outputs);

            // AM should vary around 1
            expect(max).toBeLessThanOrEqual(1.5);
            expect(min).toBeGreaterThanOrEqual(0);
        });

        it('should generate FM multiplier around 1', () => {
            modulation.setTypeLevel(1.0); // Max FM

            const outputs = [];
            for (let i = 0; i < 1000; i++) {
                const result = modulation.process();
                outputs.push(result.fm);
            }

            const max = Math.max(...outputs);
            const min = Math.min(...outputs);

            // FM should vary around 1 by ±10%
            expect(max).toBeLessThanOrEqual(1.1);
            expect(min).toBeGreaterThanOrEqual(0.9);
        });

        it('should advance LFO phase', () => {
            expect(modulation.lfoPhase).toBe(0);

            modulation.process();
            const phase1 = modulation.lfoPhase;
            expect(phase1).toBeGreaterThan(0);

            modulation.process();
            const phase2 = modulation.lfoPhase;
            expect(phase2).toBeGreaterThan(phase1);
        });

        it('should wrap phase at 2π', () => {
            modulation.lfoPhase = 2 * Math.PI - 0.001;

            modulation.process();
            modulation.process();

            expect(modulation.lfoPhase).toBeLessThan(2 * Math.PI);
        });
    });

    describe('Reset', () => {
        it('should reset LFO phase to zero', () => {
            modulation.process();
            modulation.process();
            expect(modulation.lfoPhase).toBeGreaterThan(0);

            modulation.reset();
            expect(modulation.lfoPhase).toBe(0);
        });
    });

    describe('Signal Behaviour', () => {
        it('should generate LFO at the configured frequency', () => {
            const targetFrequency = 7.5;
            const value = Math.log(targetFrequency / 0.1) / Math.log(200);

            modulation.setFrequency(value);
            modulation.setTypeLevel(0.5); // No AM/FM modulation

            const length = sampleRate * 2; // 2 seconds for accuracy
            const lfoBuffer = new Float32Array(length);

            for (let i = 0; i < length; i++) {
                const { lfo } = modulation.process();
                lfoBuffer[i] = lfo;
            }

            const estimated = SignalAnalysis.estimateFrequency(lfoBuffer, sampleRate);
            expect(estimated).toBeGreaterThan(targetFrequency * 0.9);
            expect(estimated).toBeLessThan(targetFrequency * 1.1);
        });

        it('should leave FM multiplier neutral when in AM mode', () => {
            modulation.setTypeLevel(0.25); // AM engaged

            let maxDeviation = 0;
            for (let i = 0; i < 2000; i++) {
                const { fm } = modulation.process();
                const deviation = Math.abs(fm - 1);
                if (deviation > maxDeviation) maxDeviation = deviation;
            }

            expect(maxDeviation).toBeLessThan(0.001);
        });

        it('should leave AM multiplier neutral when in FM mode', () => {
            modulation.setTypeLevel(0.75); // FM engaged

            let maxDeviation = 0;
            for (let i = 0; i < 2000; i++) {
                const { am } = modulation.process();
                const deviation = Math.abs(am - 1);
                if (deviation > maxDeviation) maxDeviation = deviation;
            }

            expect(maxDeviation).toBeLessThan(0.001);
        });
    });
});
