// ClapVoice.hpp
// Clap synthesis with multi-impulse burst and bandpass filtering
// Parameters: Density (impulse count + spacing), Tone (BP frequency)

#ifndef CLAP_VOICE_HPP
#define CLAP_VOICE_HPP

#include "../modules/NoiseGenerator.hpp"
#include "../modules/BiquadFilter.hpp"
#include "../modules/ADEnvelope.hpp"
#include <algorithm>
#include <cmath>

namespace flues::drumkit {

class ClapVoice {
private:
    float sampleRate;

    NoiseGenerator noise;
    BiquadFilter bandpass;
    ADEnvelope env;

    float densityParam;
    float toneParam;

    // Multi-impulse state
    int impulseCount;
    int currentImpulse;
    int samplesUntilNext;
    int impulseSpacing;

    static float expoMap(float value, float min, float max) {
        return min * std::pow(max / min, std::clamp(value, 0.0f, 1.0f));
    }

public:
    explicit ClapVoice(float sampleRate = 48000.0f)
        : sampleRate(sampleRate)
        , noise(444555666u)
        , bandpass(sampleRate, BiquadFilter::Type::Bandpass)
        , env(sampleRate)
        , densityParam(0.55f)
        , toneParam(0.45f)
        , impulseCount(5)
        , currentImpulse(0)
        , samplesUntilNext(0)
        , impulseSpacing(480)  // 10ms at 48kHz
    {
        bandpass.setParameters(1500.0f, 3.0f);
        env.setAttackTime(0.01f);
        env.setDecayTime(0.2f);
    }

    void setDensity(float value) {
        densityParam = std::clamp(value, 0.0f, 1.0f);
        impulseCount = static_cast<int>(3.0f + densityParam * 4.0f);  // 3-7 impulses
        impulseSpacing = static_cast<int>(sampleRate * (0.03f - densityParam * 0.02f));  // 30-10ms
    }

    void setTone(float value) {
        toneParam = std::clamp(value, 0.0f, 1.0f);
        const float freq = expoMap(toneParam, 800.0f, 3500.0f);
        bandpass.setFrequency(freq);
    }

    void trigger(float vel = 1.0f) {
        currentImpulse = 0;
        samplesUntilNext = 0;
        env.trigger();
        bandpass.reset();
    }

    float process() {
        if (!env.isActive()) {
            return 0.0f;
        }

        float sample = 0.0f;

        // Generate impulse bursts
        if (currentImpulse < impulseCount) {
            if (samplesUntilNext <= 0) {
                // Generate short burst (50 samples)
                sample = noise.process() * 0.8f;
                currentImpulse++;
                samplesUntilNext = impulseSpacing;
            }
            samplesUntilNext--;
        }

        // Filter and envelope
        sample = bandpass.process(sample);
        sample *= env.process();

        return sample * 0.5f;
    }

    bool isActive() const { return env.isActive(); }
    void reset() { env.reset(); bandpass.reset(); }
};

} // namespace flues::drumkit

#endif // CLAP_VOICE_HPP
