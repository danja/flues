// ClarinetProcessor.js
// Audio processor using Web Audio API AudioWorklet (with ScriptProcessor fallback)

import { ClarinetEngine } from './ClarinetEngine.js';
// @ts-ignore - Vite special import for worklet URL
import workletUrl from './clarinet-worklet.js?worker&url';

export class ClarinetProcessor {
    constructor() {
        this.audioContext = null;
        this.workletNode = null;
        this.scriptNode = null;
        this.engine = null;
        this.analyser = null;
        this.gainNode = null;
        this.isActive = false;
        this.useWorklet = false;
    }

    async initialize() {
        console.log(`ClarinetProcessor.initialize()`)
        this.audioContext = new (window.AudioContext || window.webkitAudioContext)();
        this._installIOSUnlock(this.audioContext);
        console.log(`[ClarinetProcessor] audioContext.state after installIOSUnlock: ${this.audioContext.state}`);

        // Explicitly resume context, even if already running, for Chrome compatibility
        if (this.audioContext.state !== 'running') {
            await this.audioContext.resume();
            console.log(`[ClarinetProcessor] audioContext.resume() called in initialize, new state: ${this.audioContext.state}`);
        }

        // Create gain node for volume control
        this.gainNode = this.audioContext.createGain();
        this.gainNode.gain.value = 1.0;
        console.log(`[ClarinetProcessor] gainNode.gain.value: ${this.gainNode.gain.value}`);

        // Create analyser for visualization
        this.analyser = this.audioContext.createAnalyser();
        this.analyser.fftSize = 2048;

        // Try to use AudioWorklet (modern, better performance)
        try {
            // workletUrl is imported at the top with ?worker&url suffix
            await this.audioContext.audioWorklet.addModule(workletUrl);

            this.workletNode = new AudioWorkletNode(this.audioContext, 'clarinet-worklet');
            console.log(`[ClarinetProcessor] audioContext.state before connecting workletNode: ${this.audioContext.state}`);
            this.workletNode.connect(this.gainNode);
            this.gainNode.connect(this.analyser);
            this.analyser.connect(this.audioContext.destination);

            this.useWorklet = true;
            console.log('[ClarinetProcessor] Using AudioWorklet for audio processing');
        } catch (error) {
            console.warn('[ClarinetProcessor] AudioWorklet not available, falling back to ScriptProcessor:', error);
            console.error('[ClarinetProcessor] AudioWorklet initialization error details:', error);
            this.engine = new ClarinetEngine(this.audioContext.sampleRate);

            this.scriptNode = this.audioContext.createScriptProcessor(2048, 0, 1);
            this.scriptNode.onaudioprocess = (event) => {
                const output = event.outputBuffer.getChannelData(0);
                for (let i = 0; i < output.length; i++) {
                    output[i] = this.engine.process();
                }
            };

            this.scriptNode.connect(this.gainNode);
            this.gainNode.connect(this.analyser);
            this.analyser.connect(this.audioContext.destination);

            this.useWorklet = false;
            console.log('[ClarinetProcessor] Using ScriptProcessorNode for audio processing');
        }

        this.isActive = true;
        console.log(`[ClarinetProcessor] audioContext.destination.maxChannelCount: ${this.audioContext.destination.maxChannelCount}`);

        this.isActive = true;
        console.log(`[ClarinetProcessor] audioContext.destination.maxChannelCount: ${this.audioContext.destination.maxChannelCount}`);
    }

    async noteOn(frequency) {
        console.log(`[ClarinetProcessor] noteOn: ${frequency}, audioContext.state: ${this.audioContext.state}`);
        if (this.audioContext.state !== 'running') {
            console.log('[ClarinetProcessor] Resuming audio context from noteOn...');
            await this.audioContext.resume();
            console.log(`[ClarinetProcessor] audioContext.state after resume: ${this.audioContext.state}`);
        }

        if (this.useWorklet && this.workletNode) {
            this.workletNode.port.postMessage({
                type: 'noteOn',
                data: { frequency }
            });
        } else if (this.engine) {
            this.engine.noteOn(frequency);
        }
    }

    _installIOSUnlock(ctx) {
        if (!ctx) return;
        let unlocked = ctx.state === 'running';
        console.log(`[ClarinetProcessor] _installIOSUnlock: initial state=${ctx.state}, unlocked=${unlocked}`);
        const cleanup = () => {
            document.removeEventListener('pointerdown', unlock, true);
            document.removeEventListener('touchstart', unlock, true);
            document.removeEventListener('keydown', unlock, true);
            console.log('[ClarinetProcessor] _installIOSUnlock: cleanup done.');
        };
        async function unlock() {
            console.log(`[ClarinetProcessor] _installIOSUnlock: unlock called, unlocked=${unlocked}, ctx.state=${ctx.state}`);
            if (unlocked) return;
            try {
                if (ctx.state !== 'running') {
                    await ctx.resume();
                    console.log(`[ClarinetProcessor] _installIOSUnlock: ctx.resume() called, new state=${ctx.state}`);
                }
                // Start a 1-frame buffer to produce audio in the same gesture
                const b = ctx.createBuffer(1, 1, ctx.sampleRate);
                const s = ctx.createBufferSource();
                s.buffer = b;
                s.connect(ctx.destination);
                s.start(0);
                setTimeout(() => s.disconnect(), 0);
                unlocked = true;
                cleanup();
                console.log('[audio] iOS unlocked');
            } catch (e) {
                console.warn('[audio] unlock failed', e);
            }
        }
        document.addEventListener('pointerdown', unlock, { capture: true, passive: true });
        document.addEventListener('touchstart', unlock, { capture: true, passive: true });
        document.addEventListener('keydown', unlock, { capture: true });
        ctx.onstatechange = () => console.log('[audio] state:', ctx.state);
    }

    noteOff() {
        if (this.useWorklet && this.workletNode) {
            this.workletNode.port.postMessage({
                type: 'noteOff'
            });
        } else if (this.engine) {
            this.engine.noteOff();
        }
    }

    setParameter(param, value) {
        console.log(`[ClarinetProcessor] setParameter: ${param}, ${value}`);
        if (this.useWorklet && this.workletNode) {
            this.workletNode.port.postMessage({
                type: 'setParameter',
                data: { param, value }
            });
        } else if (this.engine) {
            switch (param) {
                case 'breath':
                    this.engine.setBreath(value);
                    break;
                case 'reed':
                    this.engine.setReedStiffness(value);
                    break;
                case 'noise':
                    this.engine.setNoise(value);
                    break;
                case 'attack':
                    this.engine.setAttack(value);
                    break;
                case 'release':
                    this.engine.setRelease(value);
                    break;
                case 'damping':
                    this.engine.setDamping(value);
                    break;
                case 'brightness':
                    this.engine.setBrightness(value);
                    break;
                case 'vibrato':
                    this.engine.setVibrato(value);
                    break;
            }
        }
    }

    getAnalyserData() {
        if (!this.analyser) return null;
        const dataArray = new Uint8Array(this.analyser.frequencyBinCount);
        this.analyser.getByteTimeDomainData(dataArray);
        return dataArray;
    }

    shutdown() {
        if (this.workletNode) {
            this.workletNode.disconnect();
        }
        if (this.scriptNode) {
            this.scriptNode.disconnect();
        }
        if (this.gainNode) {
            this.gainNode.disconnect();
        }
        if (this.analyser) {
            this.analyser.disconnect();
        }
        if (this.audioContext) {
            this.audioContext.close();
        }
        this.isActive = false;
    }
}
