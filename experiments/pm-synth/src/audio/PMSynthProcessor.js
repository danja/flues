// PMSynthProcessor.js
// Audio processor using Web Audio API AudioWorklet (with ScriptProcessor fallback)

import { PMSynthEngine } from './PMSynthEngine.js';
// @ts-ignore - Vite special import for worklet URL
import workletUrl from './pm-synth-worklet.js?worker&url';

export class PMSynthProcessor {
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
        console.log('[PMSynthProcessor] Initializing...');
        this.audioContext = new (window.AudioContext || window.webkitAudioContext)();
        this._installIOSUnlock(this.audioContext);
        console.log(`[PMSynthProcessor] audioContext.state: ${this.audioContext.state}`);

        // Explicitly resume context
        if (this.audioContext.state !== 'running') {
            await this.audioContext.resume();
            console.log(`[PMSynthProcessor] audioContext resumed, state: ${this.audioContext.state}`);
        }

        // Create gain node for volume control
        this.gainNode = this.audioContext.createGain();
        this.gainNode.gain.value = 1.0;

        // Create analyser for visualization
        this.analyser = this.audioContext.createAnalyser();
        this.analyser.fftSize = 2048;

        // Try to use AudioWorklet (modern, better performance)
        try {
            await this.audioContext.audioWorklet.addModule(workletUrl);

            this.workletNode = new AudioWorkletNode(this.audioContext, 'pm-synth-worklet');
            this.workletNode.connect(this.gainNode);
            this.gainNode.connect(this.analyser);
            this.analyser.connect(this.audioContext.destination);

            this.useWorklet = true;
            console.log('[PMSynthProcessor] Using AudioWorklet');
        } catch (error) {
            console.warn('[PMSynthProcessor] AudioWorklet not available, using ScriptProcessor:', error);
            this.engine = new PMSynthEngine(this.audioContext.sampleRate);

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
            console.log('[PMSynthProcessor] Using ScriptProcessorNode');
        }

        this.isActive = true;
    }

    async noteOn(frequency) {
        console.log(`[PMSynthProcessor] noteOn: ${frequency}Hz`);
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
            // Map parameter names to engine methods
            switch (param) {
                case 'dcLevel': this.engine.setDCLevel(value); break;
                case 'noiseLevel': this.engine.setNoiseLevel(value); break;
                case 'toneLevel': this.engine.setToneLevel(value); break;
                case 'attack': this.engine.setAttack(value); break;
                case 'release': this.engine.setRelease(value); break;
                case 'interfaceType': this.engine.setInterfaceType(value); break;
                case 'interfaceIntensity': this.engine.setInterfaceIntensity(value); break;
                case 'tuning': this.engine.setTuning(value); break;
                case 'ratio': this.engine.setRatio(value); break;
                case 'delay1Feedback': this.engine.setDelay1Feedback(value); break;
                case 'delay2Feedback': this.engine.setDelay2Feedback(value); break;
                case 'filterFeedback': this.engine.setFilterFeedback(value); break;
                case 'filterFrequency': this.engine.setFilterFrequency(value); break;
                case 'filterQ': this.engine.setFilterQ(value); break;
                case 'filterShape': this.engine.setFilterShape(value); break;
                case 'lfoFrequency': this.engine.setLFOFrequency(value); break;
                case 'modulationTypeLevel': this.engine.setModulationTypeLevel(value); break;
            }
        }
    }

    getAnalyserData() {
        if (!this.analyser || !this.audioContext) return null;

        const timeDomain = new Float32Array(this.analyser.fftSize);
        this.analyser.getFloatTimeDomainData(timeDomain);

        const frequencyDomain = new Uint8Array(this.analyser.frequencyBinCount);
        this.analyser.getByteFrequencyData(frequencyDomain);

        return {
            timeDomain,
            frequencyDomain,
            sampleRate: this.audioContext.sampleRate
        };
    }

    _installIOSUnlock(ctx) {
        if (!ctx) return;
        let unlocked = ctx.state === 'running';

        const cleanup = () => {
            document.removeEventListener('pointerdown', unlock, true);
            document.removeEventListener('touchstart', unlock, true);
            document.removeEventListener('keydown', unlock, true);
        };

        const unlock = async () => {
            if (unlocked) return;
            try {
                if (ctx.state !== 'running') {
                    await ctx.resume();
                }
                const b = ctx.createBuffer(1, 1, ctx.sampleRate);
                const s = ctx.createBufferSource();
                s.buffer = b;
                s.connect(ctx.destination);
                s.start(0);
                setTimeout(() => s.disconnect(), 0);
                unlocked = true;
                cleanup();
                console.log('[PMSynthProcessor] iOS audio unlocked');
            } catch (e) {
                console.warn('[PMSynthProcessor] unlock failed', e);
            }
        };

        document.addEventListener('pointerdown', unlock, { capture: true, passive: true });
        document.addEventListener('touchstart', unlock, { capture: true, passive: true });
        document.addEventListener('keydown', unlock, { capture: true });
        ctx.onstatechange = () => console.log('[PMSynthProcessor] state:', ctx.state);
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
