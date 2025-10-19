import { ReverbSchroeder } from '@shared/audio/ReverbSchroeder.js';

export class ReverbModule {
  constructor(sampleRate) {
    this.reverb = new ReverbSchroeder(sampleRate);
  }

  setSize(normalized) {
    this.reverb.setSize(normalized);
  }

  setLevel(normalized) {
    this.reverb.setLevel(normalized);
  }

  reset() {
    this.reverb.reset();
  }

  process(sample) {
    return this.reverb.process(sample);
  }
}
