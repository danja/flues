import { getAlgorithmById, getDefaultParamState, resolveParamValue, DEFAULT_ALGORITHM_ID } from './AlgorithmRegistry.js';

export class DisynEngine {
  constructor({
    onStateChange = () => {},
    onError = (err) => console.error('[DisynEngine] Error:', err),
  } = {}) {
    this.audioContext = null;
    this.node = null;
    this.currentAlgorithmId = DEFAULT_ALGORITHM_ID;
    this.paramState = getDefaultParamState();
    this.envelope = { attack: 0.2, release: 0.4 };
    this.reverb = { size: 0.5, level: 0.3 };
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

      await ctx.audioWorklet.addModule(new URL('./disyn-worklet.js', import.meta.url));

      const node = new AudioWorkletNode(ctx, 'disyn-processor', {
        numberOfInputs: 0,
        numberOfOutputs: 1,
        outputChannelCount: [2],
      });

      node.port.onmessage = (event) => this.handleWorkletMessage(event.data);
      node.onprocessorerror = (error) => this.onError(error);

      node.connect(ctx.destination);

      this.node = node;
      this.ready = true;

      this.postStateToWorklet({
        type: 'init',
        sampleRate: ctx.sampleRate,
        algorithmId: this.currentAlgorithmId,
        params: this.getResolvedParams(),
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

  setAlgorithm(algorithmId) {
    const algorithm = getAlgorithmById(algorithmId);
    this.currentAlgorithmId = algorithm.id;
    this.paramState = getDefaultParamState(algorithm.id);
    this.postStateToWorklet({
      type: 'algorithm',
      id: this.currentAlgorithmId,
      params: this.getResolvedParams(),
    });
    this.notifyState();
  }

  setAlgorithmParam(paramId, normalizedValue) {
    this.paramState[paramId] = Math.min(Math.max(normalizedValue, 0), 1);
    this.postStateToWorklet({
      type: 'param',
      id: paramId,
      value: this.getResolvedParam(paramId),
    });
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

  handleWorkletMessage(message) {
    if (message?.type === 'log') {
      console.log('[DisynWorklet]', message.data);
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

  getResolvedParams() {
    const algorithm = getAlgorithmById(this.currentAlgorithmId);
    const result = {};
    algorithm.params.forEach((param) => {
      const normalized = this.paramState[param.id] ?? param.default ?? 0;
      result[param.id] = resolveParamValue(algorithm.id, param.id, normalized);
    });
    return result;
  }

  getResolvedParam(paramId) {
    return resolveParamValue(this.currentAlgorithmId, paramId, this.paramState[paramId] ?? 0);
  }

  notifyState() {
    this.onStateChange({
      contextState: this.audioContext?.state ?? 'pending',
      algorithmId: this.currentAlgorithmId,
      params: this.getResolvedParams(),
      envelope: this.envelope,
      reverb: this.reverb,
      masterGain: this.masterGain,
    });
  }
}
