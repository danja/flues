// SourcesModule.js
// Generates excitation signals: DC, White Noise, and Sawtooth Tone

export class SourcesModule {
    constructor(sampleRate = 44100) {
        this.sampleRate = sampleRate;

        // Source levels (0-1)
        this.dcLevel = 0.5;
        this.noiseLevel = 0.15;
        this.toneLevel = 0.0;

        // Sawtooth state
        this.sawtoothPhase = 0;
        this.sawtoothFrequency = 440;
    }

    /**
     * Set DC level (constant pressure source)
     * @param {number} value - Level 0-1
     */
    setDCLevel(value) {
        this.dcLevel = Math.max(0, Math.min(1, value));
    }

    /**
     * Set white noise level (turbulence source)
     * @param {number} value - Level 0-1
     */
    setNoiseLevel(value) {
        this.noiseLevel = Math.max(0, Math.min(1, value));
    }

    /**
     * Set sawtooth tone level (harmonic source)
     * @param {number} value - Level 0-1
     */
    setToneLevel(value) {
        this.toneLevel = Math.max(0, Math.min(1, value));
    }

    /**
     * Process one sample
     * @param {number} cv - Control voltage (frequency in Hz)
     * @returns {number} Mixed output of all sources
     */
    process(cv) {
        // Update sawtooth frequency from CV
        this.sawtoothFrequency = cv;

        // Generate DC (constant pressure)
        // This provides steady-state excitation for sustained interfaces
        const dc = this.dcLevel;

        // Generate white noise
        const noise = (Math.random() * 2 - 1) * this.noiseLevel;

        // Generate naive sawtooth (-1 to +1)
        const phaseIncrement = this.sawtoothFrequency / this.sampleRate;
        this.sawtoothPhase += phaseIncrement;
        if (this.sawtoothPhase >= 1.0) {
            this.sawtoothPhase -= 1.0;
        }
        const sawtooth = (this.sawtoothPhase * 2 - 1) * this.toneLevel;

        // Sum all sources
        return dc + noise + sawtooth;
    }

    /**
     * Reset state (called on note-on)
     */
    reset() {
        this.sawtoothPhase = 0;
    }
}
