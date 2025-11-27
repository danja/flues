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

    // Vocal modes
    this.nasal = false;
    this.sing = false;
    this.shout = false;
    this.fry = false;
    this.stress = 0.5;

    // Store original formant frequencies for shout mode
    this.baseFormants = [
      { frequency: 500, bandwidth: 80 },
      { frequency: 1500, bandwidth: 120 },
      { frequency: 2500, bandwidth: 150 },
      { frequency: 3500, bandwidth: 200 },
    ];

    this.masterGain = 0.8;

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

      case 'nasal':
        this.nasal = !!message.value;
        this.formantBank.setNasal(this.nasal);
        break;

      case 'sing':
        this.sing = !!message.value;
        this.larynx.setVibrato(this.sing);
        break;

      case 'shout':
        this.shout = !!message.value;
        this.updateFormants();
        break;

      case 'fry':
        this.fry = !!message.value;
        this.larynx.setFry(this.fry);
        break;

      case 'stress':
        this.stress = message.value ?? 0.5;
        break;

      case 'formant':
        if (typeof message.index === 'number' && message.frequency && message.bandwidth) {
          this.baseFormants[message.index] = {
            frequency: message.frequency,
            bandwidth: message.bandwidth,
          };
          this.updateFormants();
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

  updateFormants() {
    // Apply shout mode (increase formant frequencies by 15%)
    const shoutMultiplier = this.shout ? 1.15 : 1.0;

    for (let i = 0; i < this.baseFormants.length; i++) {
      const base = this.baseFormants[i];
      if (base) {
        const freq = base.frequency * shoutMultiplier;
        this.formantBank.setFormant(i, freq, base.bandwidth);
      }
    }

    // Shout mode also increases noise level
    if (this.shout) {
      this.aspirator.setLevel(Math.min(1.0, this.aspirator.level * 1.5));
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
    let sample = filtered * env * this.voice.velocity;

    // Apply stress (amplitude + distortion)
    // Stress maps 0-1 to 0.5-2.0x gain with slight saturation
    const stressGain = 0.5 + this.stress * 1.5;
    sample *= stressGain;

    // Add soft clipping for high stress values
    if (this.stress > 0.6) {
      const drive = (this.stress - 0.6) * 5; // 0-2 drive
      sample = Math.tanh(sample * (1 + drive));
    }

    // Apply master gain
    sample *= this.masterGain;

    // Apply reverb
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

registerProcessor('chatterbox-processor', ChatterboxProcessor);
