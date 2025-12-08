/**
 * Visualizer - Waveform and spectrum display for WebSocket audio data
 * Adapted from pm-synth/src/ui/Visualizer.js for external audio input
 */

export class Visualizer {
    constructor(canvasElement) {
        this.canvas = canvasElement;
        this.ctx = this.canvas.getContext('2d');
        this.isRunning = false;

        // Audio data from WebSocket
        this.audioData = {
            timeDomain: null,
            freqDomain: null,
            rms: 0,
            peak: 0
        };

        this.resize();
        window.addEventListener('resize', () => this.resize());
    }

    resize() {
        const rect = this.canvas.getBoundingClientRect();
        this.canvas.width = rect.width;
        this.canvas.height = rect.height;
    }

    start() {
        this.isRunning = true;
        this.draw();
    }

    stop() {
        this.isRunning = false;
    }

    /**
     * Update with new audio data from WebSocket
     * @param {Object} data - { timeDomain: Float32Array, freqDomain: Uint8Array, rms, peak }
     */
    update(data) {
        this.audioData = data;
    }

    draw() {
        if (!this.isRunning) return;

        this._drawBackground();
        this._drawWaveform(this.audioData.timeDomain);
        this._drawSpectrum(this.audioData.freqDomain);
        this._drawStats(this.audioData.rms, this.audioData.peak);

        requestAnimationFrame(() => this.draw());
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
        const bars = frequencyDomain.length;  // Use all 40 bars from server
        const barWidth = width / bars;

        // Background for spectrum
        this.ctx.fillStyle = 'rgba(74, 158, 255, 0.2)';
        this.ctx.fillRect(0, spectrumTop, width, spectrumHeight + 6);

        // Draw bars
        for (let i = 0; i < bars; i++) {
            const value = frequencyDomain[i] ?? 0;
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

    _drawStats(rms, peak) {
        // Draw RMS and peak as text overlay
        this.ctx.fillStyle = '#ccc';
        this.ctx.font = '12px monospace';
        this.ctx.textAlign = 'right';

        const x = this.canvas.width - 10;
        this.ctx.fillText(`RMS: ${rms.toFixed(3)}`, x, 20);
        this.ctx.fillText(`Peak: ${peak.toFixed(3)}`, x, 35);
    }
}
