// HiHatVoice.hpp
// Hi-hat synthesis with inharmonic oscillators and noise
// Parameters: Brightness (HPF cutoff), Decay (note-dependent: closed/open)

#ifndef HIHAT_VOICE_HPP
#define HIHAT_VOICE_HPP

#include "../modules/NoiseGenerator.hpp"
#include "../modules/BiquadFilter.hpp"
#include "../modules/ADEnvelope.hpp"
#include <algorithm>
#include <cmath>

namespace flues::drumkit {

class HiHatVoice {
private:
    static constexpr float TWO_PI = 6.28318530718f;

    float sampleRate;
    NoiseGenerator noise;
    BiquadFilter hpf;
    ADEnvelope env;

    // 6 square wave oscillators (inharmonic ratios)
    float phases[6];
    const float ratios[6] = {1.0f, 1.34f, 1.71f, 2.08f, 2.56f, 3.01f};
    const float baseFreq = 320.0f;  // Base frequency

    float brightnessParam;
    float decayTime;
    bool isClosed;

    static float expoMap(float value, float min, float max) {
        return min * std::pow(max / min, std::clamp(value, 0.0f, 1.0f));
    }

public:
    explicit HiHatVoice(float sampleRate = 48000.0f, bool closed = true)
        : sampleRate(sampleRate)
        , noise(123123123u)
        , hpf(sampleRate, BiquadFilter::Type::Highpass)
        , env(sampleRate)
        , brightnessParam(0.6f)
        , decayTime(0.1f)
        , isClosed(closed)
    {
        for (int i = 0; i < 6; ++i) {
            phases[i] = 0.0f;
        }

        hpf.setParameters(6000.0f, 0.707f);
        env.setAttackTime(0.0003f);
        env.setDecayTime(closed ? 0.1f : 0.5f);
    }

    void setClosed(bool closed) {
        isClosed = closed;
        decayTime = closed ? 0.1f : 0.5f;
        env.setDecayTime(decayTime);
    }

    void setBrightness(float value) {
        brightnessParam = std::clamp(value, 0.0f, 1.0f);
        const float cutoff = expoMap(brightnessParam, 4000.0f, 12000.0f);
        hpf.setFrequency(cutoff);
    }

    void setDecay(float value) {
        const float minDecay = isClosed ? 0.05f : 0.2f;
        const float maxDecay = isClosed ? 0.2f : 1.2f;
        decayTime = expoMap(value, minDecay, maxDecay);
        env.setDecayTime(decayTime);
    }

    void trigger(float vel = 1.0f) {
        for (int i = 0; i < 6; ++i) {
            phases[i] = 0.0f;
        }
        env.trigger();
        hpf.reset();
    }

    float process() {
        if (!env.isActive()) {
            return 0.0f;
        }

        // Generate 6 inharmonic square waves
        float oscSum = 0.0f;
        for (int i = 0; i < 6; ++i) {
            const float freq = baseFreq * ratios[i];
            phases[i] += freq / sampleRate;
            if (phases[i] >= 1.0f) phases[i] -= 1.0f;

            // Square wave
            const float square = (phases[i] < 0.5f) ? 1.0f : -1.0f;
            oscSum += square * (1.0f / (i + 1));  // Decay amplitude with harmonic
        }

        // Ring modulation (multiply oscillators)
        float sample = oscSum * 0.25f;

        // Add noise
        sample += noise.process() * 0.7f;

        // High-pass filter
        sample = hpf.process(sample);

        // Envelope
        sample *= env.process();

        return sample * 0.5f;
    }

    bool isActive() const { return env.isActive(); }

    void reset() {
        env.reset();
        hpf.reset();
        for (int i = 0; i < 6; ++i) {
            phases[i] = 0.0f;
        }
    }

    void kill() {
        reset();
    }
};

} // namespace flues::drumkit

#endif // HIHAT_VOICE_HPP
