// ClarinetProcessor.js
// Audio processor using Web Audio API ScriptProcessorNode

import { ClarinetEngine } from './ClarinetEngine.js';

export class ClarinetProcessor {
    constructor() {
        this.audioContext = null;
        this.scriptNode = null;
        this.engine = null;
        this.analyser = null;
        this.isActive = false;
    }

    async initialize() {
        this.audioContext = new (window.AudioContext || window.webkitAudioContext)();
        this.engine = new ClarinetEngine(this.audioContext.sampleRate);

        // Create analyser for visualization
        this.analyser = this.audioContext.createAnalyser();
        this.analyser.fftSize = 2048;
        this.analyser.connect(this.audioContext.destination);

        // Create script processor (4096 buffer size)
        this.scriptNode = this.audioContext.createScriptProcessor(4096, 0, 1);

        this.scriptNode.onaudioprocess = (event) => {
            const output = event.outputBuffer.getChannelData(0);

            for (let i = 0; i < output.length; i++) {
                output[i] = this.engine.process();
            }
        };

        this.scriptNode.connect(this.analyser);
        this.isActive = true;
    }

    noteOn(frequency) {
        if (this.audioContext.state === 'suspended') {
            this.audioContext.resume();
        }
        this.engine.noteOn(frequency);
    }

    noteOff() {
        this.engine.noteOff();
    }

    setParameter(param, value) {
        switch(param) {
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

    getAnalyserData() {
        if (!this.analyser) return null;
        const dataArray = new Uint8Array(this.analyser.frequencyBinCount);
        this.analyser.getByteTimeDomainData(dataArray);
        return dataArray;
    }

    shutdown() {
        if (this.scriptNode) {
            this.scriptNode.disconnect();
        }
        if (this.audioContext) {
            this.audioContext.close();
        }
        this.isActive = false;
    }
}
