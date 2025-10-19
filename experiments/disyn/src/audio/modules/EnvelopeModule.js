import { EnvelopeAR } from '@shared/audio/EnvelopeAR.js';

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
    return this.envelope.isActive;
  }
}
