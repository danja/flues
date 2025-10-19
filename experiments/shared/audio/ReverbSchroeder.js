// ReverbSchroeder.js
// Lightweight Schroeder reverb with configurable room size and wet level.

export class ReverbSchroeder {
    constructor(sampleRate = 48000) {
        this.sampleRate = sampleRate;

        this.size = 0.5;
        this.level = 0.3;

        this._combDelays = [
            Math.floor(0.0297 * sampleRate),
            Math.floor(0.0371 * sampleRate),
            Math.floor(0.0411 * sampleRate),
            Math.floor(0.0437 * sampleRate),
        ];

        this._allpassDelays = [
            Math.floor(0.005 * sampleRate),
            Math.floor(0.0017 * sampleRate),
        ];

        this._combBuffers = this._combDelays.map((len) => new Float32Array(len));
        this._combIndices = this._combDelays.map(() => 0);

        this._allpassBuffers = this._allpassDelays.map((len) => new Float32Array(len));
        this._allpassIndices = this._allpassDelays.map(() => 0);
    }

    setSize(value) {
        this.size = Math.min(Math.max(value, 0), 1);
    }

    setLevel(value) {
        this.level = Math.min(Math.max(value, 0), 1);
    }

    reset() {
        this._combBuffers.forEach((buffer) => buffer.fill(0));
        this._allpassBuffers.forEach((buffer) => buffer.fill(0));
        this._combIndices.fill(0);
        this._allpassIndices.fill(0);
    }

    process(input) {
        let combSum = 0;
        const feedback = 0.7 + this.size * 0.28;

        for (let i = 0; i < this._combBuffers.length; i++) {
            const buffer = this._combBuffers[i];
            const index = this._combIndices[i];

            const delayed = buffer[index];
            buffer[index] = input + delayed * feedback;
            combSum += delayed;
            this._combIndices[i] = (index + 1) % buffer.length;
        }

        let output = combSum / this._combBuffers.length;

        for (let i = 0; i < this._allpassBuffers.length; i++) {
            const buffer = this._allpassBuffers[i];
            const index = this._allpassIndices[i];

            const delayed = buffer[index];
            const g = 0.5;
            const newOutput = -output * g + delayed;
            buffer[index] = output + delayed * g;
            output = newOutput;
            this._allpassIndices[i] = (index + 1) % buffer.length;
        }

        return input * (1 - this.level) + output * this.level;
    }
}

