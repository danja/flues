const TWO_PI = Math.PI * 2;
const EPSILON = 1e-8;

export class OscillatorModule {
  constructor(sampleRate) {
    this.sampleRate = sampleRate;
    this.reset();
  }

  reset() {
    this.phase = 0;
    this.modPhase = 0;
    this.secondaryPhase = 0;
  }

  stepPhase(phase, frequency) {
    const next = phase + frequency / this.sampleRate;
    return next - Math.floor(next);
  }

  process(algorithm, params, frequency) {
    switch (algorithm) {
      case 'bandLimitedPulse':
        return this.processDirichletPulse(params, frequency);
      case 'dsfSingleSided':
        return this.processDSF(params, frequency);
      case 'tanhSquare':
        return this.processTanhSquare(params, frequency);
      case 'tanhSaw':
        return this.processTanhSaw(params, frequency);
      case 'paf':
        return this.processPAF(params, frequency);
      case 'modFm':
        return this.processModFM(params, frequency);
      default:
        return this.processSine(frequency);
    }
  }

  processSine(frequency) {
    this.phase = this.stepPhase(this.phase, frequency);
    return Math.sin(this.phase * TWO_PI);
  }

  processDirichletPulse(params, frequency) {
    const harmonics = Math.max(1, params.harmonics?.mapped ?? 8);
    const tilt = params.tilt?.mapped ?? 0;

    this.phase = this.stepPhase(this.phase, frequency);
    const theta = this.phase * TWO_PI;

    const numerator = Math.sin((2 * harmonics + 1) * theta * 0.5);
    const denominator = Math.sin(theta * 0.5);

    let value;
    if (Math.abs(denominator) < EPSILON) {
      value = 1;
    } else {
      value = numerator / denominator - 1;
    }

    const tiltFactor = Math.pow(10, tilt / 20);
    return (value / harmonics) * tiltFactor;
  }

  processDSF(params, frequency) {
    const decay = Math.min(params.decay?.mapped ?? 0.5, 0.98);
    const ratio = params.ratio?.mapped ?? 1.5;

    this.phase = this.stepPhase(this.phase, frequency);
    this.secondaryPhase = this.stepPhase(this.secondaryPhase, frequency * ratio);

    const w = this.phase * TWO_PI;
    const t = this.secondaryPhase * TWO_PI;

    const numerator = Math.sin(w) - decay * Math.sin(w - t);
    const denominator = 1 - 2 * decay * Math.cos(t) + decay * decay;

    if (Math.abs(denominator) < EPSILON) {
      return 0;
    }

    const normalise = Math.sqrt(1 - decay * decay);
    return (numerator / denominator) * normalise;
  }

  processTanhSquare(params, frequency) {
    const drive = params.drive?.mapped ?? 0.5;
    const trim = params.trim?.mapped ?? 0.8;

    this.phase = this.stepPhase(this.phase, frequency);
    const carrier = Math.sin(this.phase * TWO_PI);
    return Math.tanh(carrier * drive) * trim;
  }

  processTanhSaw(params, frequency) {
    const drive = params.drive?.mapped ?? 0.6;
    const blend = params.blend?.mapped ?? 0.5;

    this.phase = this.stepPhase(this.phase, frequency);
    const sine = Math.sin(this.phase * TWO_PI);
    const square = Math.tanh(sine * drive);
    this.secondaryPhase = this.stepPhase(this.secondaryPhase, frequency);
    const cosine = Math.cos(this.secondaryPhase * TWO_PI);
    const saw = square + cosine * (1 - square * square);
    return square * (1 - blend) + saw * blend;
  }

  processPAF(params, frequency) {
    const ratio = params.formant?.mapped ?? 1.2;
    const bandwidth = params.bandwidth?.mapped ?? 800;

    this.phase = this.stepPhase(this.phase, frequency);
    this.secondaryPhase = this.stepPhase(this.secondaryPhase, frequency * ratio);

    const carrier = Math.sin(this.secondaryPhase * TWO_PI);
    const mod = Math.sin(this.phase * TWO_PI);
    const decay = Math.exp(-bandwidth / this.sampleRate);
    this.modPhase = decay * this.modPhase + (1 - decay) * mod;
    return carrier * (0.6 + 0.4 * this.modPhase);
  }

  processModFM(params, frequency) {
    const index = params.index?.mapped ?? 1;
    const ratio = params.ratio?.mapped ?? 2;

    this.phase = this.stepPhase(this.phase, frequency);
    this.modPhase = this.stepPhase(this.modPhase, frequency * ratio);

    const carrier = Math.cos(this.phase * TWO_PI);
    const modulator = Math.cos(this.modPhase * TWO_PI);
    const envelope = Math.exp(-index);
    return carrier * Math.exp(index * (modulator - 1)) * envelope;
  }
}

