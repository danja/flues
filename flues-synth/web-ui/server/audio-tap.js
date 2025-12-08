/**
 * Audio Tap - Captures audio from ALSA loopback using arecord command
 * Uses child_process to spawn arecord and fft.js for frequency analysis
 */

import { spawn } from 'child_process';
import FFT from 'fft.js';

const SAMPLE_RATE = 48000;
const BUFFER_SIZE = 512;
const FFT_SIZE = 1024;
const FFT_BINS = 40;  // Number of logarithmic frequency bars

export class AudioTap {
    constructor(device = 'hw:Loopback,1') {
        this.device = device;
        this.arecord = null;
        this.fft = new FFT(FFT_SIZE);
        this.running = false;

        // Audio data buffers
        this.audioData = {
            timeDomain: new Float32Array(BUFFER_SIZE),
            freqDomain: new Uint8Array(FFT_BINS),
            rms: 0,
            peak: 0,
            voiceCount: 0  // Placeholder for future
        };

        // Partial buffer for incomplete chunks
        this.partialBuffer = Buffer.alloc(0);
        this.bytesPerSample = 4;  // FLOAT_LE = 4 bytes per sample
        this.chunkSize = BUFFER_SIZE * this.bytesPerSample;
    }

    async start() {
        try {
            // Spawn arecord process
            // -D device -f format -c channels -r rate -t raw
            this.arecord = spawn('arecord', [
                '-D', this.device,
                '-f', 'FLOAT_LE',
                '-c', '1',  // Mono
                '-r', SAMPLE_RATE.toString(),
                '-t', 'raw'
            ]);

            this.running = true;

            // Handle audio data
            this.arecord.stdout.on('data', (chunk) => {
                this.handleAudioChunk(chunk);
            });

            // Handle errors
            this.arecord.stderr.on('data', (data) => {
                const msg = data.toString();
                if (!msg.includes('overrun')) {  // Ignore XRUN warnings
                    console.error('arecord stderr:', msg);
                }
            });

            this.arecord.on('error', (err) => {
                console.error('Audio Tap: arecord error:', err);
            });

            this.arecord.on('close', (code) => {
                if (this.running) {
                    console.error(`Audio Tap: arecord exited with code ${code}`);
                }
            });

            console.log(`Audio Tap: Started (device=${this.device}, rate=${SAMPLE_RATE} Hz, buffer=${BUFFER_SIZE})`);
            return true;
        } catch (err) {
            console.error('Audio Tap: Failed to start:', err);
            console.error('Make sure arecord is installed: sudo apt install alsa-utils');
            return false;
        }
    }

    stop() {
        this.running = false;
        if (this.arecord) {
            this.arecord.kill();
            this.arecord = null;
            console.log('Audio Tap: Stopped');
        }
    }

    handleAudioChunk(chunk) {
        // Append to partial buffer
        this.partialBuffer = Buffer.concat([this.partialBuffer, chunk]);

        // Process complete chunks
        while (this.partialBuffer.length >= this.chunkSize) {
            const samples = new Float32Array(
                this.partialBuffer.buffer,
                this.partialBuffer.byteOffset,
                BUFFER_SIZE
            );

            // Update time-domain buffer
            this.audioData.timeDomain.set(samples);

            // Compute FFT
            this.computeFFT(samples);

            // Compute statistics
            this.computeStats(samples);

            // Remove processed chunk from buffer
            this.partialBuffer = this.partialBuffer.slice(this.chunkSize);
        }
    }

    computeFFT(samples) {
        // Zero-pad samples to FFT size
        const input = new Array(FFT_SIZE);
        for (let i = 0; i < BUFFER_SIZE && i < FFT_SIZE; i++) {
            input[i] = samples[i];
        }
        for (let i = BUFFER_SIZE; i < FFT_SIZE; i++) {
            input[i] = 0;
        }

        // Perform FFT (real input → complex output)
        const out = this.fft.createComplexArray();
        this.fft.realTransform(out, input);

        // Convert to logarithmic frequency bins (20Hz - 20kHz)
        const minFreq = 20;
        const maxFreq = 20000;
        const logMin = Math.log(minFreq);
        const logMax = Math.log(maxFreq);

        for (let i = 0; i < FFT_BINS; i++) {
            // Logarithmic frequency range
            const t = i / (FFT_BINS - 1);
            const freq = Math.exp(logMin + t * (logMax - logMin));
            let binIdx = Math.floor(freq * FFT_SIZE / SAMPLE_RATE);

            if (binIdx >= FFT_SIZE / 2) binIdx = FFT_SIZE / 2 - 1;
            if (binIdx < 0) binIdx = 0;

            // Compute magnitude from complex FFT output
            const real = out[binIdx * 2];
            const imag = out[binIdx * 2 + 1];
            const magnitude = Math.sqrt(real * real + imag * imag);

            // Convert to dB and normalize to 0-255
            const db = 20 * Math.log10(magnitude + 1e-6);  // Avoid log(0)
            const normalized = (db + 60) / 60;  // Map -60dB to 0dB → 0 to 1
            const clamped = Math.max(0, Math.min(1, normalized));

            this.audioData.freqDomain[i] = Math.floor(clamped * 255);
        }
    }

    computeStats(samples) {
        let sumSquares = 0;
        let maxAbs = 0;

        for (let i = 0; i < samples.length; i++) {
            const sample = samples[i];
            sumSquares += sample * sample;
            const absSample = Math.abs(sample);
            if (absSample > maxAbs) maxAbs = absSample;
        }

        this.audioData.rms = Math.sqrt(sumSquares / samples.length);
        this.audioData.peak = maxAbs;
    }

    /**
     * Get latest audio data (non-blocking)
     * @returns {Object} Audio data with timeDomain, freqDomain, rms, peak, voiceCount
     */
    getData() {
        return {
            timeDomain: this.audioData.timeDomain,
            freqDomain: this.audioData.freqDomain,
            rms: this.audioData.rms,
            peak: this.audioData.peak,
            voiceCount: this.audioData.voiceCount
        };
    }
}
