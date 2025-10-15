// DelayLinesModule.js
// Dual delay lines with tuning and ratio control

export class DelayLinesModule {
    constructor(sampleRate = 44100) {
        this.sampleRate = sampleRate;

        // Maximum delay length (for lowest note ~20Hz)
        this.maxDelayLength = Math.floor(sampleRate / 20);

        // Delay lines
        this.delayLine1 = new Float32Array(this.maxDelayLength);
        this.delayLine2 = new Float32Array(this.maxDelayLength);

        // Write positions
        this.writePos1 = 0;
        this.writePos2 = 0;

        // Parameters
        this.tuningSemitones = 0;  // -12 to +12 semitones
        this.ratio = 1.0;           // 0.5 to 2.0 (delay2/delay1 ratio)

        // Current delay lengths
        this.delayLength1 = 1000;
        this.delayLength2 = 1000;

        // Current frequency
        this.frequency = 440;
    }

    /**
     * Set tuning offset in semitones
     * @param {number} value - Normalized 0-1, maps to -12 to +12 semitones
     */
    setTuning(value) {
        this.tuningSemitones = (value - 0.5) * 24; // -12 to +12
    }

    /**
     * Set delay line ratio
     * @param {number} value - Normalized 0-1, maps to 0.5-2.0 (exponential)
     */
    setRatio(value) {
        // Exponential mapping, center (0.5) = 1.0
        if (value < 0.5) {
            // 0.0 -> 0.5, maps to 0.5 -> 1.0
            this.ratio = 0.5 + value;
        } else {
            // 0.5 -> 1.0, maps to 1.0 -> 2.0
            this.ratio = 1.0 + (value - 0.5) * 2;
        }
    }

    /**
     * Update delay lengths based on CV and tuning
     * @param {number} cv - Control voltage (frequency in Hz)
     */
    updateDelayLengths(cv) {
        this.frequency = cv;

        // Apply tuning offset
        const tuningFactor = Math.pow(2, this.tuningSemitones / 12);
        const tunedFrequency = cv * tuningFactor;

        // Calculate delay line 1 length
        this.delayLength1 = Math.max(2, Math.min(
            this.sampleRate / tunedFrequency,
            this.maxDelayLength - 1
        ));

        // Calculate delay line 2 length with ratio
        this.delayLength2 = Math.max(2, Math.min(
            this.delayLength1 * this.ratio,
            this.maxDelayLength - 1
        ));
    }

    /**
     * Process one sample through both delay lines
     * @param {number} input - Input sample
     * @param {number} cv - Control voltage (frequency in Hz)
     * @returns {Object} {delay1: number, delay2: number}
     */
    process(input, cv) {
        // Update delay lengths if frequency changed
        if (cv !== this.frequency) {
            this.updateDelayLengths(cv);
        }

        // Read from delay line 1 with linear interpolation
        const readPos1Float = this.writePos1 - this.delayLength1;
        const readPos1Wrapped = (readPos1Float + this.maxDelayLength) % this.maxDelayLength;
        const readPos1Int = Math.floor(readPos1Wrapped);
        const readPos1Frac = readPos1Wrapped - readPos1Int;
        const nextPos1 = (readPos1Int + 1) % this.maxDelayLength;

        const output1 = this.delayLine1[readPos1Int] * (1 - readPos1Frac) +
            this.delayLine1[nextPos1] * readPos1Frac;

        // Read from delay line 2 with linear interpolation
        const readPos2Float = this.writePos2 - this.delayLength2;
        const readPos2Wrapped = (readPos2Float + this.maxDelayLength) % this.maxDelayLength;
        const readPos2Int = Math.floor(readPos2Wrapped);
        const readPos2Frac = readPos2Wrapped - readPos2Int;
        const nextPos2 = (readPos2Int + 1) % this.maxDelayLength;

        const output2 = this.delayLine2[readPos2Int] * (1 - readPos2Frac) +
            this.delayLine2[nextPos2] * readPos2Frac;

        // Write input to both delay lines
        this.delayLine1[this.writePos1] = input;
        this.delayLine2[this.writePos2] = input;

        // Advance write positions
        this.writePos1 = (this.writePos1 + 1) % this.maxDelayLength;
        this.writePos2 = (this.writePos2 + 1) % this.maxDelayLength;

        return { delay1: output1, delay2: output2 };
    }

    /**
     * Reset state (called on note-on)
     */
    reset() {
        this.delayLine1.fill(0);
        this.delayLine2.fill(0);
        this.writePos1 = 0;
        this.writePos2 = 0;

        // Add small initial excitation
        for (let i = 0; i < Math.min(100, this.maxDelayLength); i++) {
            this.delayLine1[i] = (Math.random() * 2 - 1) * 0.01;
            this.delayLine2[i] = (Math.random() * 2 - 1) * 0.01;
        }
    }
}
