import { OscillatorModule } from './modules/OscillatorModule.js';
import { EnvelopeModule } from './modules/EnvelopeModule.js';
import { ReverbModule } from './modules/ReverbModule.js';

const sampleRate = globalThis.sampleRate;

class DisynProcessor extends AudioWorkletProcessor {
  constructor() {
    super();

    this.sampleRate = sampleRate;
    this.oscillator = new OscillatorModule(this.sampleRate);
    this.envelope = new EnvelopeModule(this.sampleRate);
    this.reverb = new ReverbModule(this.sampleRate);

    this.masterGain = 0.8;
    this.algorithmId = 'tanhSquare';
    this.params = {};

    this.voice = {
      active: false,
      midi: null,
      frequency: 440,
      velocity: 1,
      gate: false,
    };

    this.port.onmessage = (event) => this.handleMessage(event.data);
  }

  handleMessage(message) {
    if (!message || typeof message !== 'object') return;

    switch (message.type) {
      case 'init':
        this.algorithmId = message.algorithmId ?? this.algorithmId;
        this.params = this.extractParams(message.params);
        if (message.envelope) {
          this.envelope.configure(message.envelope);
        }
        if (message.reverb) {
          this.reverb.setSize(message.reverb.size ?? 0.5);
          this.reverb.setLevel(message.reverb.level ?? 0.3);
        }
        if (typeof message.master === 'number') {
          this.masterGain = message.master;
        }
        break;

      case 'algorithm':
        this.algorithmId = message.id ?? this.algorithmId;
        this.params = this.extractParams(message.params);
        break;

      case 'param':
        if (!this.params) this.params = {};
        this.params[message.id] = message.value;
        break;

      case 'envelope':
        this.envelope.configure(message.value ?? {});
        break;

      case 'reverb':
        if (message.value) {
          this.reverb.setSize(message.value.size ?? 0.5);
          this.reverb.setLevel(message.value.level ?? 0.3);
        }
        break;

      case 'master':
        this.masterGain = message.value ?? this.masterGain;
        break;

      case 'noteOn':
        this.voice.active = true;
        this.voice.midi = message.midi;
        this.voice.frequency = message.frequency ?? this.voice.frequency;
        this.voice.velocity = message.velocity ?? 1;
        this.voice.gate = true;
        this.envelope.gate(true);
        break;

      case 'noteOff':
        if (this.voice.midi === message.midi) {
          this.voice.gate = false;
          this.envelope.gate(false);
        }
        break;

      default:
        break;
    }
  }

  extractParams(raw) {
    if (!raw) return {};
    const result = {};
    for (const [key, value] of Object.entries(raw)) {
      result[key] = value;
    }
    return result;
  }

  generateSample() {
    if (!this.voice.active && !this.envelope.active) {
      return 0;
    }

    const env = this.envelope.process();
    if (env <= 0 && !this.voice.gate) {
      this.voice.active = false;
      return 0;
    }

    const oscSample = this.oscillator.process(
      this.algorithmId,
      this.params,
      this.voice.frequency
    );

    const sample = oscSample * env * this.voice.velocity * this.masterGain;
    return this.reverb.process(sample);
  }

  process(_inputs, outputs) {
    const output = outputs[0];
    if (!output) {
      return true;
    }

    const left = output[0];
    const right = output[1] ?? output[0];

    for (let i = 0; i < left.length; i++) {
      const sample = this.generateSample();
      left[i] = sample;
      right[i] = sample;
    }

    return true;
  }
}

registerProcessor('disyn-processor', DisynProcessor);
