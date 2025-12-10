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
    int burstLength;
    int burstSamples;

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
        , burstLength(240)     // 5ms burst at 48kHz
        , burstSamples(0)
    {
        bandpass.setParameters(1500.0f, 6.0f);  // Higher Q for more resonance
        env.setAttackTime(0.001f);  // Very fast attack
        env.setDecayTime(0.3f);     // Longer decay
    }

    void setDensity(float value) {
        densityParam = std::clamp(value, 0.0f, 1.0f);
        impulseCount = static_cast<int>(3.0f + densityParam * 4.0f);  // 3-7 impulses
        impulseSpacing = static_cast<int>(sampleRate * (0.025f - densityParam * 0.015f));  // 25-10ms
        burstLength = static_cast<int>(sampleRate * (0.003f + densityParam * 0.004f));  // 3-7ms bursts
    }

    void setTone(float value) {
        toneParam = std::clamp(value, 0.0f, 1.0f);
        const float freq = expoMap(toneParam, 1000.0f, 4500.0f);  // Higher frequency range
        const float q = 4.0f + toneParam * 4.0f;  // Q from 4-8 (more resonance at high tone)
        bandpass.setParameters(freq, q);
    }

    void trigger(float vel = 1.0f) {
        currentImpulse = 0;
        samplesUntilNext = 0;
        burstSamples = 0;
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
            if (burstSamples > 0) {
                // Continue current burst
                sample = noise.process() * 1.2f;  // Louder bursts
                burstSamples--;
            } else if (samplesUntilNext <= 0) {
                // Start new burst
                sample = noise.process() * 1.2f;
                burstSamples = burstLength;
                currentImpulse++;
                samplesUntilNext = impulseSpacing;
            } else {
                samplesUntilNext--;
            }
        }

        // Filter and envelope
        sample = bandpass.process(sample);
        sample *= env.process();

        return sample * 0.8f;  // Higher output level
    }

    bool isActive() const { return env.isActive(); }
    void reset() { env.reset(); bandpass.reset(); }
};

} // namespace flues::drumkit

#endif // CLAP_VOICE_HPP
