class EnvelopeAR {
  constructor({
    sampleRate,
    attackSeconds = 0.01,
    releaseSeconds = 0.05,
    minAttack = 0.001,
    maxAttack = 1.0,
    minRelease = 0.01,
    maxRelease = 3.0,
  }) {
    this.sampleRate = sampleRate;
    this.minAttack = minAttack;
    this.maxAttack = maxAttack;
    this.minRelease = minRelease;
    this.maxRelease = maxRelease;
    this.attackSeconds = attackSeconds;
    this.releaseSeconds = releaseSeconds;
    this.value = 0;
    this.gate = false;
    this.active = false;
  }

  static map(value, min, max) {
    const clamped = Math.min(Math.max(value, 0), 1);
    return min * Math.pow(max / min, clamped);
  }

  setAttackNormalized(value) {
    this.attackSeconds = EnvelopeAR.map(value, this.minAttack, this.maxAttack);
  }

  setReleaseNormalized(value) {
    this.releaseSeconds = EnvelopeAR.map(value, this.minRelease, this.maxRelease);
  }

  setGate(on) {
    this.gate = !!on;
    if (this.gate) {
      this.active = true;
    }
  }

  reset() {
    this.value = 0;
    this.active = true;
  }

  process() {
    if (this.gate) {
      const attackRate = 1 / Math.max(this.attackSeconds * this.sampleRate, 1);
      this.value += attackRate;
      if (this.value > 1) this.value = 1;
    } else {
      const releaseRate = 1 / Math.max(this.releaseSeconds * this.sampleRate, 1);
      this.value -= releaseRate;
      if (this.value <= 0) {
        this.value = 0;
        this.active = false;
      }
    }

    return this.value;
  }
}

export class EnvelopeModule {
  constructor(sampleRate) {
    this.envelope = new EnvelopeAR({ sampleRate });
  }

  configure({ attack, release }) {
    if (typeof attack === 'number') {
      this.envelope.setAttackNormalized(attack);
    }
    if (typeof release === 'number') {
      this.envelope.setReleaseNormalized(release);
    }
  }

  gate(on) {
    this.envelope.setGate(on);
    if (on) {
      this.envelope.reset();
    }
  }

  process() {
    return this.envelope.process();
  }

  get active() {
    return this.envelope.active;
  }
}
