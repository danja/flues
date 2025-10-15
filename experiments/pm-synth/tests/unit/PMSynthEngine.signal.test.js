// PMSynthEngine.signal.test.js
// Integration-style checks on engine output using shared signal analysis

import { describe, it, expect } from 'vitest';
import { PMSynthEngine, InterfaceType } from '../../src/audio/PMSynthEngine.js';
import { SignalAnalysis } from '../utils/signal-analyzers.js';

describe('PMSynthEngine signal behaviour', () => {
    const sampleRate = 44100;

    function renderEngineSamples({
        frequency = 220,
        durationSeconds = 0.9,
        noteOffTime = 0.45,
        setup
    } = {}) {
        const engine = new PMSynthEngine(sampleRate);

        // Favour deterministic sources for analysis
        engine.setDCLevel(0.05);
        engine.setNoiseLevel(0.0);
        engine.setToneLevel(1.0);
        engine.setInterfaceType(InterfaceType.PLUCK);
        engine.setInterfaceIntensity(0.6);
        engine.setReverbLevel(0.0);

        if (typeof setup === 'function') {
            setup(engine);
        }

        engine.noteOn(frequency);

        const totalSamples = Math.floor(durationSeconds * sampleRate);
        const buffer = new Float32Array(totalSamples);
        const noteOffSample = Math.floor(noteOffTime * sampleRate);

        for (let i = 0; i < totalSamples; i++) {
            if (i === noteOffSample) {
                engine.noteOff();
            }
            buffer[i] = engine.process();
        }

        return buffer;
    }

    it('produces a stable, non-silent sustain while the note is active', () => {
        const buffer = renderEngineSamples();

        const sustainStart = Math.floor(sampleRate * 0.1);
        const sustainEnd = Math.floor(sampleRate * 0.35);
        const sustainWindow = buffer.subarray(sustainStart, sustainEnd);

        const stats = SignalAnalysis.analyseBuffer(sustainWindow, sampleRate);

        expect(SignalAnalysis.hasValidOutput(sustainWindow, 5)).toBe(true);
        expect(stats.rms).toBeGreaterThan(0.01);
        expect(stats.peak).toBeGreaterThan(0.05);
        expect(Math.abs(stats.dcOffset)).toBeLessThan(0.4);

        const silenceCheck = SignalAnalysis.detectPrematureSilence(sustainWindow, 0.002, 256);
        expect(silenceCheck.premature).toBe(false);
    });

    it('decays in level after note off', () => {
        const buffer = renderEngineSamples();

        const sustainStart = Math.floor(sampleRate * 0.1);
        const sustainEnd = Math.floor(sampleRate * 0.35);
        const releaseStart = Math.floor(sampleRate * 0.5);
        const releaseEnd = Math.floor(sampleRate * 0.65);
        const tailStart = Math.floor(sampleRate * 0.75);
        const tailEnd = Math.floor(sampleRate * 0.9);

        const sustainWindow = buffer.subarray(sustainStart, sustainEnd);
        const releaseWindow = buffer.subarray(releaseStart, releaseEnd);
        const tailWindow = buffer.subarray(tailStart, tailEnd);

        const sustainStats = SignalAnalysis.analyseBuffer(sustainWindow, sampleRate);
        const releaseStats = SignalAnalysis.analyseBuffer(releaseWindow, sampleRate);
        const tailStats = SignalAnalysis.analyseBuffer(tailWindow, sampleRate);

        expect(SignalAnalysis.hasValidOutput(releaseWindow, 5)).toBe(true);
        expect(tailStats.rms).toBeLessThan(0.6);
    });

    const interfaceScenarios = [
        { name: 'pluck', type: InterfaceType.PLUCK, intensity: 0.6 },
        { name: 'hit', type: InterfaceType.HIT, intensity: 0.7 },
        { name: 'reed', type: InterfaceType.REED, intensity: 0.5 }
    ];

    interfaceScenarios.forEach(({ name, type, intensity }) => {
        it(`maintains energy and decays for interface type: ${name}`, () => {
            const buffer = renderEngineSamples({
                setup(engine) {
                    engine.setInterfaceType(type);
                    engine.setInterfaceIntensity(intensity);
                    engine.setNoiseLevel(0.05);
                }
            });

            const sustainStart = Math.floor(sampleRate * 0.12);
            const sustainEnd = Math.floor(sampleRate * 0.32);
            const releaseStart = Math.floor(sampleRate * 0.5);
            const releaseEnd = Math.floor(sampleRate * 0.65);
            const tailStart = Math.floor(sampleRate * 0.75);
            const tailEnd = Math.floor(sampleRate * 0.9);

            const sustainWindow = buffer.subarray(sustainStart, sustainEnd);
            const releaseWindow = buffer.subarray(releaseStart, releaseEnd);
            const tailWindow = buffer.subarray(tailStart, tailEnd);

            const sustainStats = SignalAnalysis.analyseBuffer(sustainWindow, sampleRate);
            const releaseStats = SignalAnalysis.analyseBuffer(releaseWindow, sampleRate);
            const tailStats = SignalAnalysis.analyseBuffer(tailWindow, sampleRate);

            expect(SignalAnalysis.hasValidOutput(sustainWindow, 5)).toBe(true);
            expect(sustainStats.rms).toBeGreaterThan(0.008);
            expect(SignalAnalysis.hasValidOutput(releaseWindow, 5)).toBe(true);
            expect(tailStats.rms).toBeLessThan(0.6);
            expect(Math.abs(sustainStats.dcOffset)).toBeLessThan(0.5);
        });
    });
});
