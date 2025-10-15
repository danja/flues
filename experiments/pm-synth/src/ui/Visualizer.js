// Visualizer.js
// Handles waveform visualization and live signal diagnostics

import { analyseBuffer } from '../utils/signalAnalysis.js';

export class Visualizer {
    constructor(canvasElement, statsElement = null) {
        this.canvas = canvasElement;
        this.ctx = this.canvas.getContext('2d');
        this.isRunning = false;
        this.sampleRate = 44100;

        this.statsElement = statsElement;
        this.statsFields = statsElement ? this._collectStatsFields(statsElement) : null;
        this.statsUpdateInterval = 4;
        this.frameCounter = 0;

        this.resize();
        window.addEventListener('resize', () => this.resize());
    }

    _collectStatsFields(container) {
        const query = (name) => container.querySelector(`[data-stat="${name}"]`);
        return {
            rms: query('rms'),
            peak: query('peak'),
            crest: query('crest'),
            dc: query('dc'),
            p2p: query('p2p')
        };
    }

    resize() {
        const rect = this.canvas.getBoundingClientRect();
        this.canvas.width = rect.width;
        this.canvas.height = rect.height;
    }

    start(getDataFunction) {
        this.isRunning = true;
        this.dataFunction = getDataFunction;
        this.draw();
    }

    stop() {
        this.isRunning = false;
    }

    draw() {
        if (!this.isRunning) return;

        const analyserData = typeof this.dataFunction === 'function'
            ? this.dataFunction()
            : null;

        const timeDomain = this._normaliseTimeDomain(analyserData);
        const frequencyDomain = analyserData && analyserData.frequencyDomain
            ? analyserData.frequencyDomain
            : null;

        if (analyserData && analyserData.sampleRate) {
            this.sampleRate = analyserData.sampleRate;
        }

        this._drawBackground();
        this._drawWaveform(timeDomain);
        this._drawSpectrum(frequencyDomain);
        this._updateStats(timeDomain);

        this.frameCounter = (this.frameCounter + 1) % Number.MAX_SAFE_INTEGER;
        requestAnimationFrame(() => this.draw());
    }

    _normaliseTimeDomain(data) {
        if (!data) return null;

        if (data.timeDomain instanceof Float32Array) {
            return data.timeDomain;
        }

        const source = Array.isArray(data) || data instanceof Uint8Array ? data : null;
        if (!source) return null;

        const buffer = new Float32Array(source.length);
        for (let i = 0; i < source.length; i++) {
            buffer[i] = (source[i] - 128) / 128; // convert 0-255 to -1..1
        }
        return buffer;
    }

    _drawBackground() {
        this.ctx.fillStyle = '#111';
        this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
    }

    _drawWaveform(buffer) {
        if (!buffer || buffer.length === 0) {
            return;
        }

        this.ctx.lineWidth = 2;
        this.ctx.strokeStyle = '#4a9eff';
        this.ctx.beginPath();

        const height = this.canvas.height;
        const width = this.canvas.width;
        const waveformHeight = height * 0.65;
        const yOffset = (height - waveformHeight) * 0.5;

        const slice = buffer.length / width;
        let x = 0;

        for (let i = 0; i < width; i++) {
            const sampleIndex = Math.floor(i * slice);
            const sample = buffer[sampleIndex] ?? 0;
            const y = yOffset + (0.5 - sample * 0.5) * waveformHeight;

            if (i === 0) {
                this.ctx.moveTo(x, y);
            } else {
                this.ctx.lineTo(x, y);
            }

            x += 1;
        }

        this.ctx.stroke();
    }

    _drawSpectrum(frequencyDomain) {
        if (!frequencyDomain || frequencyDomain.length === 0) {
            return;
        }

        const height = this.canvas.height;
        const width = this.canvas.width;
        const spectrumHeight = height * 0.25;
        const spectrumTop = height - spectrumHeight - 6;
        const bars = 40;
        const step = Math.floor(frequencyDomain.length / bars);
        const barWidth = width / bars;

        this.ctx.fillStyle = 'rgba(74, 158, 255, 0.2)';
        this.ctx.fillRect(0, spectrumTop, width, spectrumHeight + 6);

        for (let i = 0; i < bars; i++) {
            const index = i * step;
            const value = frequencyDomain[index] ?? 0;
            const magnitude = value / 255;
            const barHeight = magnitude * spectrumHeight;

            const x = i * barWidth;
            const y = spectrumTop + spectrumHeight - barHeight;

            const gradient = this.ctx.createLinearGradient(x, y, x, y + barHeight);
            gradient.addColorStop(0, '#4a9eff');
            gradient.addColorStop(1, '#1a3f70');

            this.ctx.fillStyle = gradient;
            this.ctx.fillRect(x, y, barWidth - 1, Math.max(1, barHeight));
        }
    }

    _updateStats(buffer) {
        if (!this.statsFields || !buffer || buffer.length === 0) {
            return;
        }

        if (this.frameCounter % this.statsUpdateInterval !== 0) {
            return;
        }

        const metrics = analyseBuffer(buffer, this.sampleRate, { includeSpectrum: false });

        if (this.statsFields.rms) {
            this.statsFields.rms.textContent = metrics.rms.toFixed(3);
        }
        if (this.statsFields.peak) {
            this.statsFields.peak.textContent = metrics.peak.toFixed(3);
        }
        if (this.statsFields.p2p) {
            this.statsFields.p2p.textContent = metrics.peakToPeak.toFixed(3);
        }
        if (this.statsFields.crest) {
            this.statsFields.crest.textContent = metrics.crestFactor.toFixed(2);
        }
        if (this.statsFields.dc) {
            this.statsFields.dc.textContent = metrics.dcOffset.toFixed(3);
        }
    }
}
