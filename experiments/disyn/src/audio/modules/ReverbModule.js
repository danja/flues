class ReverbSchroeder {
  constructor(sampleRate) {
    this.size = 0.5;
    this.level = 0.3;

    this.combDelays = [
      Math.floor(0.0297 * sampleRate),
      Math.floor(0.0371 * sampleRate),
      Math.floor(0.0411 * sampleRate),
      Math.floor(0.0437 * sampleRate),
    ];

    this.allpassDelays = [
      Math.floor(0.005 * sampleRate),
      Math.floor(0.0017 * sampleRate),
    ];

    this.combBuffers = this.combDelays.map((len) => new Float32Array(len));
    this.combIndices = this.combDelays.map(() => 0);
    this.allpassBuffers = this.allpassDelays.map((len) => new Float32Array(len));
    this.allpassIndices = this.allpassDelays.map(() => 0);
  }

  setSize(value) {
    this.size = Math.min(Math.max(value, 0), 1);
  }

  setLevel(value) {
    this.level = Math.min(Math.max(value, 0), 1);
  }

  reset() {
    this.combBuffers.forEach((buffer) => buffer.fill(0));
    this.allpassBuffers.forEach((buffer) => buffer.fill(0));
    this.combIndices.fill(0);
    this.allpassIndices.fill(0);
  }

  process(input) {
    let combSum = 0;
    const feedback = 0.7 + this.size * 0.28;

    for (let i = 0; i < this.combBuffers.length; i++) {
      const buffer = this.combBuffers[i];
      const index = this.combIndices[i];
      const delayed = buffer[index];
      buffer[index] = input + delayed * feedback;
      combSum += delayed;
      this.combIndices[i] = (index + 1) % buffer.length;
    }

    let output = combSum / this.combBuffers.length;

    for (let i = 0; i < this.allpassBuffers.length; i++) {
      const buffer = this.allpassBuffers[i];
      const index = this.allpassIndices[i];
      const delayed = buffer[index];
      const g = 0.5;
      const newOutput = -output * g + delayed;
      buffer[index] = output + delayed * g;
      output = newOutput;
      this.allpassIndices[i] = (index + 1) % buffer.length;
    }

    return input * (1 - this.level) + output * this.level;
  }
}

export class ReverbModule {
  constructor(sampleRate) {
    this.reverb = new ReverbSchroeder(sampleRate);
  }

  setSize(value) {
    this.reverb.setSize(value);
  }

  setLevel(value) {
    this.reverb.setLevel(value);
  }

  reset() {
    this.reverb.reset();
  }

  process(sample) {
    return this.reverb.process(sample);
  }
}
