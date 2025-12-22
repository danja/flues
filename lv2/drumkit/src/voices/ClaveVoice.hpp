// ClaveVoice.hpp
// Sharp wooden clave click using filtered noise burst
// Parameters: Tone, Decay

#ifndef CLAVE_VOICE_HPP
#define CLAVE_VOICE_HPP

#include "../modules/NoiseGenerator.hpp"
#include "../modules/ADEnvelope.hpp"
#include "../modules/BiquadFilter.hpp"
#include <algorithm>
#include <cmath>

namespace flues::drumkit {

class ClaveVoice {
private:
    float sampleRate;

    NoiseGenerator noise;
    ADEnvelope env;
    BiquadFilter bandpass;

    float toneFreq;
    float decayTime;
    float velocity;

    static float expoMap(float value, float min, float max) {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        return min * std::pow(max / min, clamped);
    }

public:
    explicit ClaveVoice(float sampleRate = 48000.0f)
        : sampleRate(sampleRate)
        , noise(910231u)
        , env(sampleRate)
        , bandpass(sampleRate, BiquadFilter::Type::Bandpass)
        , toneFreq(1800.0f)
        , decayTime(0.08f)
        , velocity(1.0f)
    {
        env.setAttackTime(0.0008f);
        setTone(0.5f);
        setDecay(0.25f);
    }

    void setTone(float value) {
        toneFreq = expoMap(value, 900.0f, 4200.0f);
        bandpass.setParameters(toneFreq, 10.0f);
    }

    void setDecay(float value) {
        decayTime = expoMap(value, 0.02f, 0.4f);
        env.setDecayTime(decayTime);
    }

    void trigger(float vel = 1.0f) {
        velocity = std::clamp(vel, 0.0f, 1.0f);
        env.trigger();
        bandpass.reset();
    }

    float process() {
        if (!env.isActive()) {
            return 0.0f;
        }

        const float envValue = env.process();
        float sample = noise.process() * (0.8f + envValue * 0.4f);
        sample = bandpass.process(sample);

        return sample * envValue * velocity * 0.5f;
    }

    bool isActive() const { return env.isActive(); }

    void reset() {
        env.reset();
        bandpass.reset();
    }
};

} // namespace flues::drumkit

#endif // CLAVE_VOICE_HPP
