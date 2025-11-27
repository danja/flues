// chatterbox-worklet.js
// AudioWorklet processor for Chatterbox speech synthesizer

import { LarynxModule } from './modules/LarynxModule.js';
import { AspiratorModule } from './modules/AspiratorModule.js';
import { FormantBankModule } from './modules/FormantBankModule.js';
import { EnvelopeModule } from './modules/EnvelopeModule.js';
import { ReverbModule } from './modules/ReverbModule.js';

const sampleRate = globalThis.sampleRate;

class ChatterboxProcessor extends AudioWorkletProcessor {
  constructor() {
    super();

    this.sampleRate = sampleRate;

    // Initialize DSP modules
    this.larynx = new LarynxModule(this.sampleRate);
    this.aspirator = new AspiratorModule(this.sampleRate);
    this.formantBank = new FormantBankModule(this.sampleRate);
    this.envelope = new EnvelopeModule(this.sampleRate);
    this.reverb = new ReverbModule(this.sampleRate);

    // Voice state
    this.voice = {
      active: false,
      midi: null,
      frequency: 120,
      velocity: 1,
      gate: false,
    };

    this.masterGain = 0.8;
    this.debugLogged = false;

    this.port.onmessage = (event) => this.handleMessage(event.data);
  }

  handleMessage(message) {
    if (!message || typeof message !== 'object') return;

    switch (message.type) {
      case 'init':
        if (typeof message.pitch === 'number') {
          this.larynx.setPitch(message.pitch);
        }
        if (typeof message.voiced === 'boolean') {
          this.larynx.setVoiced(message.voiced);
        }
        if (typeof message.aspirated === 'boolean') {
          this.aspirator.setAspirated(message.aspirated);
        }
        if (typeof message.noiseLevel === 'number') {
          this.aspirator.setLevel(message.noiseLevel);
        }
        if (message.formants && Array.isArray(message.formants)) {
          message.formants.forEach((formant, index) => {
            if (formant && formant.frequency && formant.bandwidth) {
              this.formantBank.setFormant(index, formant.frequency, formant.bandwidth);
            }
          });
        }
        if (message.envelope) {
          this.envelope.configure(message.envelope);
        }
        if (message.reverb) {
          this.reverb.setSize(message.reverb.size ?? 0.3);
          this.reverb.setLevel(message.reverb.level ?? 0.2);
        }
        if (typeof message.master === 'number') {
          this.masterGain = message.master;
        }
        break;

      case 'pitch':
        this.larynx.setPitch(message.value);
        break;

      case 'voiced':
        this.larynx.setVoiced(message.value);
        break;

      case 'aspirated':
        this.aspirator.setAspirated(message.value);
        break;

      case 'noiseLevel':
        this.aspirator.setLevel(message.value);
        break;

      case 'formant':
        if (typeof message.index === 'number' && message.frequency && message.bandwidth) {
          this.formantBank.setFormant(message.index, message.frequency, message.bandwidth);
        }
        break;

      case 'envelope':
        this.envelope.configure(message.value ?? {});
        break;

      case 'reverb':
        if (message.value) {
          this.reverb.setSize(message.value.size ?? 0.3);
          this.reverb.setLevel(message.value.level ?? 0.2);
        }
        break;

      case 'master':
        this.masterGain = message.value ?? this.masterGain;
        break;

      case 'noteOn':
        this.voice.active = true;
        this.voice.midi = message.midi;
        if (message.frequency) {
          this.voice.frequency = message.frequency;
          this.larynx.setPitch(message.frequency);
        }
        this.voice.velocity = message.velocity ?? 1;
        this.voice.gate = true;
        this.envelope.gate(true);
        this.formantBank.reset();
        this.port.postMessage({ type: 'log', data: `noteOn: freq=${this.voice.frequency}, voiced=${this.larynx.enabled}` });
        break;

      case 'noteOff':
        if (this.voice.midi === message.midi) {
          this.voice.gate = false;
          this.envelope.gate(false);
          this.debugLogged = false;
        }
        break;

      default:
        break;
    }
  }

  generateSample() {
    // Check if voice is active
    if (!this.voice.active && !this.envelope.active) {
      return 0;
    }

    // Process envelope
    const env = this.envelope.process();
    if (env <= 0 && !this.voice.gate) {
      this.voice.active = false;
      return 0;
    }

    // Generate excitation signal (mix larynx + aspirator)
    const larynxSignal = this.larynx.process();
    const aspiratorSignal = this.aspirator.process();
    const excitation = larynxSignal + aspiratorSignal;

    // Pass excitation through formant filter bank
    const filtered = this.formantBank.process(excitation);

    // Apply envelope and velocity
    const sample = filtered * env * this.voice.velocity * this.masterGain;

    // Apply reverb
    const final = this.reverb.process(sample);

    // Debug: log first sample through entire chain
    if (this.voice.active && !this.debugLogged) {
      this.debugLogged = true;
      this.port.postMessage({
        type: 'log',
        data: `Chain: larynx=${larynxSignal.toFixed(3)}, exc=${excitation.toFixed(3)}, filtered=${filtered.toFixed(3)}, env=${env.toFixed(3)}, sample=${sample.toFixed(3)}, final=${final.toFixed(3)}`
      });
    }

    return final;
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

registerProcessor('chatterbox-processor', ChatterboxProcessor);
