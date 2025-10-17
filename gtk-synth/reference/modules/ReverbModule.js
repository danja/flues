// ReverbModule.js
// Simple Schroeder reverb with Size and Level controls

export class ReverbModule {
    constructor(sampleRate) {
        this.sampleRate = sampleRate;

        // Parameters
        this.size = 0.5;        // Room size (0-1)
        this.level = 0.3;       // Wet/dry mix (0-1)

        // Comb filter delays (in samples) - scaled by size
        this.combDelays = [
            Math.floor(0.0297 * sampleRate),  // ~29.7ms
            Math.floor(0.0371 * sampleRate),  // ~37.1ms
            Math.floor(0.0411 * sampleRate),  // ~41.1ms
            Math.floor(0.0437 * sampleRate)   // ~43.7ms
        ];

        // Allpass filter delays (in samples)
        this.allpassDelays = [
            Math.floor(0.005 * sampleRate),   // ~5ms
            Math.floor(0.0017 * sampleRate)   // ~1.7ms
        ];

        // Initialize buffers
        this.combBuffers = this.combDelays.map(len => new Float32Array(len));
        this.combIndices = new Array(this.combDelays.length).fill(0);

        this.allpassBuffers = this.allpassDelays.map(len => new Float32Array(len));
        this.allpassIndices = new Array(this.allpassDelays.length).fill(0);
    }

    setSize(value) {
        // Size affects feedback amount (0.5 = small room, 1.0 = large room)
        this.size = Math.max(0, Math.min(1, value));
    }

    setLevel(value) {
        // Wet/dry mix level
        this.level = Math.max(0, Math.min(1, value));
    }

    process(input) {
        // Comb filters in parallel
        let combSum = 0;
        const feedback = 0.7 + this.size * 0.28; // 0.7 to 0.98

        for (let i = 0; i < this.combBuffers.length; i++) {
            const buffer = this.combBuffers[i];
            const index = this.combIndices[i];
            const delay = this.combDelays[i];

            // Read from delay line
            const delayed = buffer[index];

            // Write input + feedback to delay line
            buffer[index] = input + delayed * feedback;

            // Add to output sum
            combSum += delayed;

            // Advance index
            this.combIndices[i] = (index + 1) % delay;
        }

        // Average the comb outputs
        let output = combSum / this.combBuffers.length;

        // Allpass filters in series
        for (let i = 0; i < this.allpassBuffers.length; i++) {
            const buffer = this.allpassBuffers[i];
            const index = this.allpassIndices[i];
            const delay = this.allpassDelays[i];

            // Read from delay line
            const delayed = buffer[index];

            // Allpass feedback coefficient
            const g = 0.5;

            // Allpass filter equation
            const newOutput = -output * g + delayed;
            buffer[index] = output + delayed * g;
            output = newOutput;

            // Advance index
            this.allpassIndices[i] = (index + 1) % delay;
        }

        // Wet/dry mix
        return input * (1 - this.level) + output * this.level;
    }

    reset() {
        // Clear all buffers
        this.combBuffers.forEach(buffer => buffer.fill(0));
        this.allpassBuffers.forEach(buffer => buffer.fill(0));

        // Reset indices
        this.combIndices.fill(0);
        this.allpassIndices.fill(0);
    }
}
