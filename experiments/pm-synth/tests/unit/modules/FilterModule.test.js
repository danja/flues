// FilterModule.test.js
import { describe, it, expect, beforeEach } from 'vitest';
import { FilterModule } from '../../../src/audio/modules/FilterModule.js';
import { TestSignals } from '../../utils/signal-generators.js';
import { SignalAnalysis } from '../../utils/signal-analyzers.js';

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

    describe('Signal Behaviour', () => {
        const bufferLength = 4096;
        const cutoff = 1000;
        const frequencyValue = Math.log(cutoff / 20) / Math.log(1000);
        const analysisStart = 512;

        it('should attenuate high frequencies in lowpass mode', () => {
            const lowpass = new FilterModule(sampleRate);
            lowpass.setShape(0.0);
            lowpass.setFrequency(frequencyValue);
            lowpass.setQ(0.3);

            const lowSignal = TestSignals.sine(200, sampleRate, bufferLength, 1.0);
            const highSignal = TestSignals.sine(5000, sampleRate, bufferLength, 1.0);

            const lowOutput = new Float32Array(bufferLength);
            const highOutput = new Float32Array(bufferLength);

            for (let i = 0; i < bufferLength; i++) {
                lowOutput[i] = lowpass.process(lowSignal[i]);
            }

            lowpass.reset();
            lowpass.setShape(0.0);
            lowpass.setFrequency(frequencyValue);
            lowpass.setQ(0.3);

            for (let i = 0; i < bufferLength; i++) {
                highOutput[i] = lowpass.process(highSignal[i]);
            }

            const lowRMS = SignalAnalysis.rmsLevel(lowOutput, analysisStart, bufferLength);
            const highRMS = SignalAnalysis.rmsLevel(highOutput, analysisStart, bufferLength);

            expect(lowRMS).toBeGreaterThan(highRMS * 2);
            expect(SignalAnalysis.hasValidOutput(lowOutput)).toBe(true);
            expect(SignalAnalysis.hasValidOutput(highOutput)).toBe(true);
        });

        it('should attenuate low frequencies in highpass mode', () => {
            const highpass = new FilterModule(sampleRate);
            highpass.setShape(1.0);
            highpass.setFrequency(frequencyValue);
            highpass.setQ(0.3);

            const lowSignal = TestSignals.sine(200, sampleRate, bufferLength, 1.0);
            const highSignal = TestSignals.sine(5000, sampleRate, bufferLength, 1.0);

            const lowOutput = new Float32Array(bufferLength);
            const highOutput = new Float32Array(bufferLength);

            for (let i = 0; i < bufferLength; i++) {
                lowOutput[i] = highpass.process(lowSignal[i]);
            }

            highpass.reset();
            highpass.setShape(1.0);
            highpass.setFrequency(frequencyValue);
            highpass.setQ(0.3);

            for (let i = 0; i < bufferLength; i++) {
                highOutput[i] = highpass.process(highSignal[i]);
            }

            const lowRMS = SignalAnalysis.rmsLevel(lowOutput, analysisStart, bufferLength);
            const highRMS = SignalAnalysis.rmsLevel(highOutput, analysisStart, bufferLength);

            expect(highRMS).toBeGreaterThan(lowRMS * 2);
            expect(SignalAnalysis.hasValidOutput(lowOutput)).toBe(true);
            expect(SignalAnalysis.hasValidOutput(highOutput)).toBe(true);
        });

        it('should emphasise centre frequency in bandpass mode', () => {
            const bandpass = new FilterModule(sampleRate);
            bandpass.setShape(0.5);
            bandpass.setFrequency(frequencyValue);
            bandpass.setQ(0.6); // narrower band for clarity

            const centreSignal = TestSignals.sine(cutoff, sampleRate, bufferLength, 1.0);
            const lowSignal = TestSignals.sine(200, sampleRate, bufferLength, 1.0);
            const highSignal = TestSignals.sine(5000, sampleRate, bufferLength, 1.0);

            const centreOutput = new Float32Array(bufferLength);
            const lowOutput = new Float32Array(bufferLength);
            const highOutput = new Float32Array(bufferLength);

            for (let i = 0; i < bufferLength; i++) {
                centreOutput[i] = bandpass.process(centreSignal[i]);
            }

            bandpass.reset();
            bandpass.setShape(0.5);
            bandpass.setFrequency(frequencyValue);
            bandpass.setQ(0.6);

            for (let i = 0; i < bufferLength; i++) {
                lowOutput[i] = bandpass.process(lowSignal[i]);
            }

            bandpass.reset();
            bandpass.setShape(0.5);
            bandpass.setFrequency(frequencyValue);
            bandpass.setQ(0.6);

            for (let i = 0; i < bufferLength; i++) {
                highOutput[i] = bandpass.process(highSignal[i]);
            }

            const centreRMS = SignalAnalysis.rmsLevel(centreOutput, analysisStart, bufferLength);
            const lowRMS = SignalAnalysis.rmsLevel(lowOutput, analysisStart, bufferLength);
            const highRMS = SignalAnalysis.rmsLevel(highOutput, analysisStart, bufferLength);

            expect(centreRMS).toBeGreaterThan(lowRMS * 1.5);
            expect(centreRMS).toBeGreaterThan(highRMS * 1.5);
            expect(SignalAnalysis.hasValidOutput(centreOutput)).toBe(true);
            expect(SignalAnalysis.hasValidOutput(lowOutput)).toBe(true);
            expect(SignalAnalysis.hasValidOutput(highOutput)).toBe(true);
        });

        it('should remain numerically stable with noise input', () => {
            const filterUnderTest = new FilterModule(sampleRate);
            filterUnderTest.setShape(0.25);
            filterUnderTest.setFrequency(0.7);
            filterUnderTest.setQ(0.4);

            const noise = TestSignals.whiteNoise(bufferLength, 0.5);
            const output = new Float32Array(bufferLength);

            for (let i = 0; i < bufferLength; i++) {
                output[i] = filterUnderTest.process(noise[i]);
            }

            expect(SignalAnalysis.hasValidOutput(output)).toBe(true);
            const rms = SignalAnalysis.rmsLevel(output, analysisStart, bufferLength);
            expect(rms).toBeGreaterThan(0.001);
        });
    });
});
