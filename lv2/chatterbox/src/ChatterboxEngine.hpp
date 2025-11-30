// ChatterboxEngine.hpp
// Main audio engine coordinator for Chatterbox speech synthesizer
// Ported from experiments/chatterbox/src/audio/chatterbox-worklet.js

#ifndef CHATTERBOX_ENGINE_HPP
#define CHATTERBOX_ENGINE_HPP

#include "modules/LarynxModule.hpp"
#include "modules/AspiratorModule.hpp"
#include "modules/FormantBankModule.hpp"
#include "modules/EnvelopeModule.hpp"
#include "ReverbModule.hpp"  // From pm-synth via CMake include path
#include <cmath>
#include <algorithm>

// Formant frequency/bandwidth pair
struct FormantParams {
    float frequency;
    float bandwidth;
};

// Voice state for monophonic playback
struct VoiceState {
    bool active;
    uint8_t midi;
    float frequency;
    float velocity;
    bool gate;
};

class ChatterboxEngine {
private:
    float sampleRate;

    // DSP modules
    LarynxModule larynx;
    AspiratorModule aspirator;
    FormantBankModule formantBank;
    EnvelopeModule envelope;
    flues::pm::ReverbModule reverb;

    // Voice state
    VoiceState voice;

    // Vocal modes
    bool nasal;
    bool sing;
    bool shout;
    bool fry;
    float stress;  // 0-1

    // Store original formant frequencies for shout mode
    FormantParams baseFormants[4];

    // Master gain
    float masterGain;

    /**
     * Update formant frequencies based on shout mode
     */
    void updateFormants() {
        // Apply shout mode (increase formant frequencies by 15%)
        const float shoutMultiplier = shout ? 1.15f : 1.0f;

        for (int i = 0; i < 4; i++) {
            const float freq = baseFormants[i].frequency * shoutMultiplier;
            formantBank.setFormant(i, freq, baseFormants[i].bandwidth);
        }
    }

public:
    ChatterboxEngine(float sampleRate)
        : sampleRate(sampleRate)
        , larynx(sampleRate)
        , aspirator(sampleRate)
        , formantBank(sampleRate)
        , envelope(sampleRate)
        , reverb(sampleRate)
        , voice{false, 0, 120.0f, 1.0f, false}
        , nasal(false)
        , sing(false)
        , shout(false)
        , fry(false)
        , stress(0.3f)
        , masterGain(0.8f)
    {
        // Initialize base formants (schwa neutral vowel)
        baseFormants[0] = {500.0f, 80.0f};
        baseFormants[1] = {1500.0f, 120.0f};
        baseFormants[2] = {2500.0f, 150.0f};
        baseFormants[3] = {3500.0f, 200.0f};

        // Set initial reverb parameters
        reverb.setSize(0.3f);
        reverb.setLevel(0.2f);
    }

    // ===== Parameter Setters =====

    void setPitch(float frequency) {
        larynx.setPitch(frequency);
        voice.frequency = frequency;
    }

    void setVoiced(bool enabled) {
        larynx.setVoiced(enabled);
    }

    void setAspirated(bool enabled) {
        aspirator.setAspirated(enabled);
    }

    void setNoiseLevel(float level) {
        aspirator.setLevel(level);
    }

    void setNasal(bool enabled) {
        nasal = enabled;
        formantBank.setNasal(enabled);
    }

    void setSing(bool enabled) {
        sing = enabled;
        larynx.setVibrato(enabled);
    }

    void setShout(bool enabled) {
        shout = enabled;
        updateFormants();

        // Shout mode also increases noise level by 50%
        // Note: This requires storing base noise level, simplified for now
    }

    void setFry(bool enabled) {
        fry = enabled;
        larynx.setFry(enabled);
    }

    void setStress(float value) {
        stress = std::clamp(value, 0.0f, 1.0f);
    }

    void setFormant(int index, float frequency, float bandwidth) {
        if (index >= 0 && index < 4) {
            baseFormants[index].frequency = frequency;
            baseFormants[index].bandwidth = bandwidth;
            updateFormants();
        }
    }

    void setEnvelope(float attack, float release) {
        envelope.configure(attack, release);
    }

    void setReverbSize(float size) {
        reverb.setSize(size);
    }

    void setReverbLevel(float level) {
        reverb.setLevel(level);
    }

    void setMasterGain(float gain) {
        masterGain = std::clamp(gain, 0.0f, 1.0f);
    }

    // ===== Note Control =====

    void noteOn(uint8_t midiNote, float frequency, float velocity = 1.0f) {
        voice.active = true;
        voice.midi = midiNote;
        voice.frequency = frequency;
        voice.velocity = std::clamp(velocity, 0.0f, 1.0f);
        voice.gate = true;

        larynx.setPitch(frequency);
        envelope.gate(true);
        formantBank.reset();
    }

    void noteOff(uint8_t midiNote) {
        if (voice.midi == midiNote) {
            voice.gate = false;
            envelope.gate(false);
        }
    }

    void allNotesOff() {
        voice.gate = false;
        voice.active = false;
        envelope.gate(false);
    }

    // ===== Audio Processing =====

    /**
     * Generate one sample of audio
     * @return Audio sample
     */
    float generateSample() {
        // Check if voice is active
        if (!voice.active && !envelope.isActive()) {
            return 0.0f;
        }

        // Process envelope
        const float env = envelope.process();
        if (env <= 0.0f && !voice.gate) {
            voice.active = false;
            return 0.0f;
        }

        // Generate excitation signal (mix larynx + aspirator)
        const float larynxSignal = larynx.process();
        const float aspiratorSignal = aspirator.process();
        const float excitation = larynxSignal + aspiratorSignal;

        // Pass excitation through formant filter bank
        const float filtered = formantBank.process(excitation);

        // Apply envelope and velocity
        float sample = filtered * env * voice.velocity;

        // Apply stress (amplitude + distortion)
        // Stress maps 0-1 to 0.5-2.0x gain with slight saturation
        const float stressGain = 0.5f + stress * 1.5f;
        sample *= stressGain;

        // Add soft clipping for high stress values
        if (stress > 0.6f) {
            const float drive = (stress - 0.6f) * 5.0f;  // 0-2 drive
            sample = std::tanh(sample * (1.0f + drive));
        }

        // Apply master gain
        sample *= masterGain;

        // Apply reverb
        return reverb.process(sample);
    }

    /**
     * Process a block of audio samples
     * @param output - Output buffer
     * @param nFrames - Number of frames to process
     */
    void process(float* output, uint32_t nFrames) {
        for (uint32_t i = 0; i < nFrames; i++) {
            output[i] = generateSample();
        }
    }

    /**
     * Check if voice is currently active
     * @return True if voice is generating sound
     */
    bool isActive() const {
        return voice.active || envelope.isActive();
    }
};

#endif // CHATTERBOX_ENGINE_HPP
