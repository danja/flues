// ClarinetProcessor.js
// Audio processor using Web Audio API AudioWorklet (with ScriptProcessor fallback)

import { ClarinetEngine } from './ClarinetEngine.js';

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

        // Create gain node for volume control
        this.gainNode = this.audioContext.createGain();
        this.gainNode.gain.value = 0.8;

        // Create analyser for visualization
        this.analyser = this.audioContext.createAnalyser();
        this.analyser.fftSize = 2048;

        // Try to use AudioWorklet (modern, better performance)
        try {
            await this.audioContext.audioWorklet.addModule(
                new URL('./clarinet-worklet.js', import.meta.url)
            );

            this.workletNode = new AudioWorkletNode(this.audioContext, 'clarinet-worklet');
            this.workletNode.connect(this.gainNode);
            this.gainNode.connect(this.analyser);
            this.analyser.connect(this.audioContext.destination);

            this.useWorklet = true;
            console.log('Using AudioWorklet for audio processing');
        } catch (error) {
            // Fallback to ScriptProcessorNode
            console.warn('AudioWorklet not available, falling back to ScriptProcessor:', error);
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
        }

        this.isActive = true;
    }

    async noteOn(frequency) {
        if (this.audioContext.state !== 'running') {
            await this.audioContext.resume();
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
        const cleanup = () => {
            document.removeEventListener('pointerdown', unlock, true);
            document.removeEventListener('touchstart', unlock, true);
            document.removeEventListener('keydown', unlock, true);
        };
        async function unlock() {
            if (unlocked) return;
            try {
                if (ctx.state !== 'running') await ctx.resume();
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
