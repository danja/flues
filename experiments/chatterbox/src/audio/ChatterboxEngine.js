// ChatterboxEngine.js
// Main audio engine coordinator for Chatterbox speech synthesizer

const mapPitchHz = (normalized) => {
  // Map 0-1 to 80-400 Hz (exponential for musical perception)
  const min = 80;
  const max = 400;
  return min * Math.pow(max / min, normalized);
};

const mapFormantHz = (normalized, min, max) => {
  // Exponential mapping for formant frequencies
  return min * Math.pow(max / min, normalized);
};

export class ChatterboxEngine {
  constructor({
    onStateChange = () => {},
    onError = (err) => console.error('[ChatterboxEngine] Error:', err),
  } = {}) {
    this.audioContext = null;
    this.node = null;

    // Source parameters
    this.pitch = 0.3; // normalized 0-1
    this.voiced = true;
    this.aspirated = false;
    this.noiseLevel = 0.2;

    // Vocal modes
    this.nasal = false;
    this.sing = false;
    this.shout = false;
    this.fry = false;
    this.stress = 0.5; // normalized 0-1

    // Formant parameters (normalized 0-1)
    this.formants = {
      f1: 0.5, // Maps to ~500-700 Hz
      f2: 0.4, // Maps to ~1200-1500 Hz
      f3: 0.5, // Maps to ~2500 Hz
      f4: 0.5, // Maps to ~3500 Hz
    };

    // Envelope
    this.envelope = { attack: 0.01, release: 0.15 };

    // Reverb (optional)
    this.reverb = { size: 0.3, level: 0.2 };

    // Master
    this.masterGain = 0.8;

    this.onStateChange = onStateChange;
    this.onError = onError;
    this.ready = false;
    this.pendingMessages = [];
  }

  async initialize() {
    if (this.ready) return;

    try {
      const ctx = new (window.AudioContext || window.webkitAudioContext)();
      this.audioContext = ctx;

      await ctx.audioWorklet.addModule(new URL('./chatterbox-worklet.js', import.meta.url));

      const node = new AudioWorkletNode(ctx, 'chatterbox-processor', {
        numberOfInputs: 0,
        numberOfOutputs: 1,
        outputChannelCount: [2],
      });

      node.port.onmessage = (event) => this.handleWorkletMessage(event.data);
      node.onprocessorerror = (error) => this.onError(error);

      node.connect(ctx.destination);

      this.node = node;
      this.ready = true;

      // Send initial state to worklet
      this.postStateToWorklet({
        type: 'init',
        sampleRate: ctx.sampleRate,
        pitch: mapPitchHz(this.pitch),
        voiced: this.voiced,
        aspirated: this.aspirated,
        noiseLevel: this.noiseLevel,
        formants: this.getResolvedFormants(),
        envelope: this.envelope,
        reverb: this.reverb,
        master: this.masterGain,
      });

      this.flushPendingMessages();
      this.notifyState();
    } catch (error) {
      this.onError(error);
    }
  }

  async ensureRunning() {
    if (!this.audioContext) {
      await this.initialize();
    }
    if (this.audioContext.state === 'suspended') {
      await this.audioContext.resume();
      this.notifyState();
    }
  }

  suspend() {
    if (!this.audioContext) return;
    if (this.audioContext.state !== 'closed') {
      this.audioContext.suspend().then(() => this.notifyState());
    }
  }

  setPitch(normalized) {
    this.pitch = Math.min(Math.max(normalized, 0), 1);
    this.postStateToWorklet({
      type: 'pitch',
      value: mapPitchHz(this.pitch),
    });
  }

  setVoiced(enabled) {
    this.voiced = !!enabled;
    this.postStateToWorklet({ type: 'voiced', value: this.voiced });
  }

  setAspirated(enabled) {
    this.aspirated = !!enabled;
    this.postStateToWorklet({ type: 'aspirated', value: this.aspirated });
  }

  setNoiseLevel(level) {
    this.noiseLevel = Math.min(Math.max(level, 0), 1);
    this.postStateToWorklet({ type: 'noiseLevel', value: this.noiseLevel });
  }

  setNasal(enabled) {
    this.nasal = !!enabled;
    this.postStateToWorklet({ type: 'nasal', value: this.nasal });
  }

  setSing(enabled) {
    this.sing = !!enabled;
    this.postStateToWorklet({ type: 'sing', value: this.sing });
  }

  setShout(enabled) {
    this.shout = !!enabled;
    this.postStateToWorklet({ type: 'shout', value: this.shout });
  }

  setFry(enabled) {
    this.fry = !!enabled;
    this.postStateToWorklet({ type: 'fry', value: this.fry });
  }

  setStress(level) {
    this.stress = Math.min(Math.max(level, 0), 1);
    this.postStateToWorklet({ type: 'stress', value: this.stress });
  }

  setFormant(index, normalized) {
    const key = `f${index + 1}`;
    if (this.formants[key] !== undefined) {
      this.formants[key] = Math.min(Math.max(normalized, 0), 1);
      const resolved = this.getResolvedFormants();
      this.postStateToWorklet({
        type: 'formant',
        index,
        frequency: resolved[index].frequency,
        bandwidth: resolved[index].bandwidth,
      });
    }
  }

  setMasterGain(value) {
    this.masterGain = Math.min(Math.max(value, 0), 1);
    this.postStateToWorklet({ type: 'master', value: this.masterGain });
  }

  setEnvelope({ attack, release }) {
    if (typeof attack === 'number') {
      this.envelope.attack = attack;
    }
    if (typeof release === 'number') {
      this.envelope.release = release;
    }
    this.postStateToWorklet({ type: 'envelope', value: this.envelope });
  }

  setReverb({ size, level }) {
    if (typeof size === 'number') {
      this.reverb.size = size;
    }
    if (typeof level === 'number') {
      this.reverb.level = level;
    }
    this.postStateToWorklet({ type: 'reverb', value: this.reverb });
  }

  noteOn({ midi, frequency, velocity = 1 }) {
    if (!this.ready) return;
    this.postStateToWorklet({
      type: 'noteOn',
      midi,
      frequency,
      velocity,
      time: this.audioContext.currentTime,
    });
  }

  noteOff({ midi }) {
    if (!this.ready) return;
    this.postStateToWorklet({
      type: 'noteOff',
      midi,
      time: this.audioContext.currentTime,
    });
  }

  getResolvedFormants() {
    // F1: 200-1000 Hz (jaw opening)
    // F2: 500-3000 Hz (tongue front/back)
    // F3: 1500-4000 Hz (lip rounding)
    // F4: 2500-4500 Hz (voice quality)
    return [
      {
        frequency: mapFormantHz(this.formants.f1, 200, 1000),
        bandwidth: 80,
      },
      {
        frequency: mapFormantHz(this.formants.f2, 500, 3000),
        bandwidth: 120,
      },
      {
        frequency: mapFormantHz(this.formants.f3, 1500, 4000),
        bandwidth: 150,
      },
      {
        frequency: mapFormantHz(this.formants.f4, 2500, 4500),
        bandwidth: 200,
      },
    ];
  }

  handleWorkletMessage(message) {
    if (message?.type === 'log') {
      console.log('[ChatterboxWorklet]', message.data);
    } else if (message?.type === 'error') {
      this.onError(new Error(message.message));
    }
  }

  postStateToWorklet(message) {
    if (!this.node) {
      this.pendingMessages.push(message);
      return;
    }
    this.node.port.postMessage(message);
  }

  flushPendingMessages() {
    if (!this.node || this.pendingMessages.length === 0) return;
    this.pendingMessages.forEach((msg) => this.node.port.postMessage(msg));
    this.pendingMessages.length = 0;
  }

  notifyState() {
    this.onStateChange({
      contextState: this.audioContext?.state ?? 'pending',
      pitch: this.pitch,
      voiced: this.voiced,
      aspirated: this.aspirated,
      noiseLevel: this.noiseLevel,
      nasal: this.nasal,
      sing: this.sing,
      shout: this.shout,
      fry: this.fry,
      stress: this.stress,
      formants: this.formants,
      envelope: this.envelope,
      reverb: this.reverb,
      masterGain: this.masterGain,
    });
  }
}
