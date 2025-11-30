// LarynxModule.hpp
// Modified sawtooth oscillator for vocal excitation
// Ported from experiments/chatterbox/src/audio/modules/LarynxModule.js

#ifndef LARYNX_MODULE_HPP
#define LARYNX_MODULE_HPP

#include <cmath>
#include <algorithm>

class LarynxModule {
private:
    float sampleRate;
    float phase;
    float frequency;
    bool enabled;

    // Vocal fry mode (subharmonics)
    bool fryEnabled;
    float fryPhase;

    // Vibrato (sing mode)
    bool vibratoEnabled;
    float vibratoPhase;
    float vibratoRate;
    float vibratoDepth;

    static constexpr float TWO_PI = 6.28318530718f;

    /**
     * Step the phase accumulator forward
     * @param freq - Frequency in Hz
     * @return New phase value (0-1)
     */
    float stepPhase(float freq) {
        phase += freq / sampleRate;
        if (phase >= 1.0f) {
            phase -= std::floor(phase);
        }
        return phase;
    }

public:
    LarynxModule(float sampleRate)
        : sampleRate(sampleRate)
        , phase(0.0f)
        , frequency(120.0f)  // Default pitch (Hz)
        , enabled(true)
        , fryEnabled(false)
        , fryPhase(0.0f)
        , vibratoEnabled(false)
        , vibratoPhase(0.0f)
        , vibratoRate(5.5f)      // Hz
        , vibratoDepth(0.015f)   // ±1.5%
    {
    }

    /**
     * Set the pitch frequency
     * @param freq - Frequency in Hz (typical range: 80-400 Hz)
     */
    void setPitch(float freq) {
        frequency = std::clamp(freq, 20.0f, 2000.0f);
    }

    /**
     * Enable or disable voiced excitation
     * @param enable - True to enable larynx output
     */
    void setVoiced(bool enable) {
        enabled = enable;
    }

    /**
     * Enable or disable vocal fry mode
     * @param enable - True to add subharmonics
     */
    void setFry(bool enable) {
        fryEnabled = enable;
    }

    /**
     * Enable or disable vibrato (sing mode)
     * @param enable - True to add vibrato
     */
    void setVibrato(bool enable) {
        vibratoEnabled = enable;
    }

    /**
     * Process one sample
     * @return Output sample
     */
    float process() {
        if (!enabled) {
            return 0.0f;
        }

        // Apply vibrato if enabled
        float currentFreq = frequency;
        if (vibratoEnabled) {
            vibratoPhase += vibratoRate / sampleRate;
            if (vibratoPhase >= 1.0f) {
                vibratoPhase -= 1.0f;
            }
            const float vibrato = std::sin(TWO_PI * vibratoPhase);
            currentFreq *= 1.0f + vibrato * vibratoDepth;
        }

        stepPhase(currentFreq);

        // Generate modified sawtooth wave
        // Add some waveshaping for richer harmonic content
        const float naive = phase * 2.0f - 1.0f;  // Basic sawtooth (-1 to +1)

        // Apply mild cubic waveshaping to emphasize odd harmonics
        // This makes the waveform more "vocal-like"
        float shaped = naive + 0.15f * (naive * naive * naive);

        // Add vocal fry (subharmonics) if enabled
        if (fryEnabled) {
            fryPhase += (currentFreq * 0.5f) / sampleRate;  // Octave below
            if (fryPhase >= 1.0f) {
                fryPhase -= 1.0f;
            }
            const float fry = (fryPhase * 2.0f - 1.0f) * 0.3f;  // Attenuated subharmonic
            shaped += fry;
        }

        return shaped * 0.5f;  // Scale down to prevent clipping
    }

    /**
     * Reset phase (useful for hard sync or note-on events)
     */
    void reset() {
        phase = 0.0f;
        fryPhase = 0.0f;
        vibratoPhase = 0.0f;
    }
};

#endif // LARYNX_MODULE_HPP
