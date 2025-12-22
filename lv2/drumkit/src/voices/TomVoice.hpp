// TomVoice.hpp
// Tom synthesis with resonant bandpass and pitch envelope
// Parameters: Pitch (note-dependent), Decay

#ifndef TOM_VOICE_HPP
#define TOM_VOICE_HPP

#include "../modules/PitchEnvelope.hpp"
#include "../modules/ADEnvelope.hpp"
#include "../modules/BiquadFilter.hpp"
#include "../modules/NoiseGenerator.hpp"
#include <algorithm>
#include <cmath>

namespace flues::drumkit {

class TomVoice {
private:
    static constexpr float TWO_PI = 6.28318530718f;

    float sampleRate;
    PitchEnvelope pitchEnv;
    ADEnvelope ampEnv;
    BiquadFilter resonator;
    NoiseGenerator noise;

    float phase;
    float basePitch;     // Note-dependent base frequency
    float velocity;
    float level;

    static float expoMap(float value, float min, float max) {
        return min * std::pow(max / min, std::clamp(value, 0.0f, 1.0f));
    }

public:
    explicit TomVoice(float sampleRate = 48000.0f, float baseFreq = 100.0f)
        : sampleRate(sampleRate)
        , pitchEnv(sampleRate)
        , ampEnv(sampleRate)
        , resonator(sampleRate, BiquadFilter::Type::Bandpass)
        , noise(777888999u)
        , phase(0.0f)
        , basePitch(baseFreq)
        , velocity(1.0f)
        , level(1.0f)
    {
        resonator.setQ(20.0f);  // High Q for metallic ring
        ampEnv.setAttackTime(0.0005f);
        ampEnv.setDecayTime(0.3f);
    }

    void setBasePitch(float freq) {
        basePitch = freq;
    }

    void setPitch(float value) {
        const float startFreq = basePitch * (1.0f + value * 0.5f);
        const float endFreq = basePitch * 0.8f;
        pitchEnv.setParameters(startFreq, endFreq, 0.15f);
    }

    void setDecay(float value) {
        const float decayTime = expoMap(value, 0.08f, 0.8f);
        ampEnv.setDecayTime(decayTime);
    }

    void setLevel(float value) {
        level = std::clamp(value, 0.0f, 1.5f);
    }

    void trigger(float vel = 1.0f) {
        velocity = std::clamp(vel, 0.0f, 1.0f);
        phase = 0.0f;
        pitchEnv.trigger();
        ampEnv.trigger();
        resonator.reset();
    }

    float process() {
        if (!ampEnv.isActive()) {
            return 0.0f;
        }

        const float freq = pitchEnv.process();

        // Triangle wave oscillator
        phase += freq / sampleRate;
        if (phase >= 1.0f) phase -= 1.0f;

        float sample = (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);

        // Add initial noise burst
        if (ampEnv.getValue() > 0.8f) {
            sample += noise.process() * 0.2f;
        }

        // Resonant filter
        resonator.setFrequency(freq);
        sample = resonator.process(sample);

        sample *= ampEnv.process() * velocity;
        return sample * 0.7f * level;
    }

    bool isActive() const { return ampEnv.isActive(); }
    void reset() { ampEnv.reset(); pitchEnv.reset(); resonator.reset(); phase = 0.0f; }
};

} // namespace flues::drumkit

#endif // TOM_VOICE_HPP
