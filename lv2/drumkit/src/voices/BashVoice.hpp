// BashVoice.hpp
// Metallic plate/pipe strike voice for harsh industrial hits
// Parameters: Size, Spread, Decay, Drive, Noise, Edge

#ifndef BASH_VOICE_HPP
#define BASH_VOICE_HPP

#include "../modules/NoiseGenerator.hpp"
#include "../modules/ADEnvelope.hpp"
#include "../modules/BiquadFilter.hpp"
#include "../modules/Distortion.hpp"
#include <algorithm>
#include <cmath>

namespace flues::drumkit {

class BashVoice {
private:
    static constexpr float TWO_PI = 6.28318530718f;

    float sampleRate;

    NoiseGenerator noise;
    ADEnvelope env;
    BiquadFilter hp;
    BiquadFilter resA;
    BiquadFilter resB;
    Distortion saturator;

    // Oscillator state
    float phaseA;
    float phaseB;
    float modPhase;

    // Parameters
    float baseFreq;
    float spreadRatio;
    float decayTime;
    float driveAmount;
    float noiseAmount;
    float edgeFreq;
    float velocity;

    float oscFreqA;
    float oscFreqB;

    static float expoMap(float value, float min, float max) {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        return min * std::pow(max / min, clamped);
    }

    void updateResonators() {
        // Fundamental plus inharmonic overtone cluster
        const float freqA = baseFreq;
        const float freqB = baseFreq * (1.25f + spreadRatio * 1.6f);

        const float qBase = 7.0f + spreadRatio * 9.0f;

        resA.setParameters(freqA, qBase);
        resB.setParameters(freqB, qBase * 0.9f);

        oscFreqA = freqA;
        oscFreqB = freqB * 1.05f;  // Slight offset for beating
    }

public:
    explicit BashVoice(float sampleRate = 48000.0f)
        : sampleRate(sampleRate)
        , noise(444121u)
        , env(sampleRate)
        , hp(sampleRate, BiquadFilter::Type::Highpass)
        , resA(sampleRate, BiquadFilter::Type::Bandpass)
        , resB(sampleRate, BiquadFilter::Type::Bandpass)
        , saturator()
        , phaseA(0.0f)
        , phaseB(0.0f)
        , modPhase(0.0f)
        , baseFreq(420.0f)
        , spreadRatio(0.55f)
        , decayTime(0.8f)
        , driveAmount(4.0f)
        , noiseAmount(0.6f)
        , edgeFreq(4000.0f)
        , velocity(1.0f)
        , oscFreqA(420.0f)
        , oscFreqB(780.0f)
    {
        env.setAttackTime(0.002f);   // Fast but not clicky
        setSize(0.45f);
        setSpread(0.55f);
        setDecay(0.70f);
        setDrive(0.65f);
        setNoise(0.60f);
        setEdge(0.70f);
    }

    void setSize(float value) {
        baseFreq = expoMap(value, 180.0f, 1200.0f);
        updateResonators();
    }

    void setSpread(float value) {
        spreadRatio = std::clamp(value, 0.0f, 1.0f);
        updateResonators();
    }

    void setDecay(float value) {
        decayTime = expoMap(value, 0.12f, 1.6f);
        env.setDecayTime(decayTime);
    }

    void setDrive(float value) {
        driveAmount = 1.0f + value * 9.0f;  // 1-10×
        saturator.setDrive(driveAmount);
    }

    void setNoise(float value) {
        noiseAmount = std::clamp(value, 0.0f, 1.0f);
    }

    void setEdge(float value) {
        edgeFreq = expoMap(value, 800.0f, 8000.0f);
        hp.setParameters(edgeFreq, 0.8f);
    }

    void trigger(float vel = 1.0f) {
        velocity = std::clamp(vel, 0.0f, 1.0f);
        phaseA = 0.0f;
        phaseB = 0.0f;
        modPhase = 0.0f;
        env.trigger();
        hp.reset();
        resA.reset();
        resB.reset();
    }

    float process() {
        if (!env.isActive()) {
            return 0.0f;
        }

        // Oscillator cluster
        const float envValue = env.process();

        // Envelope-driven modulation to smear pitch identity
        const float modFreq = 35.0f + envValue * 1800.0f;  // Faster when loud
        modPhase += TWO_PI * modFreq / sampleRate;
        if (modPhase >= TWO_PI) modPhase -= TWO_PI;
        const float mod = std::sin(modPhase);
        const float jitter = noise.process() * 0.015f * (envValue + 0.1f);

        const float modulatedFreqA = oscFreqA * (1.0f + mod * 0.18f + jitter);
        const float modulatedFreqB = oscFreqB * (1.0f - mod * 0.22f + jitter * 1.2f);

        const float phaseIncA = TWO_PI * modulatedFreqA / sampleRate;
        const float phaseIncB = TWO_PI * modulatedFreqB / sampleRate;

        phaseA += phaseIncA;
        phaseB += phaseIncB;

        if (phaseA >= TWO_PI) phaseA -= TWO_PI;
        if (phaseB >= TWO_PI) phaseB -= TWO_PI;

        const float oscA = std::sin(phaseA);
        const float oscB = std::sin(phaseB);
        const float ring = oscA * oscB;  // Ring mod clang

        // Noisy strike with highpass lift
        const float burst = noise.process() * (0.3f + noiseAmount * 1.4f) * (envValue + 0.15f);

        float sample = ring + 0.35f * (oscA + oscB) + burst;
        sample = hp.process(sample);

        // Dual resonators for metal body
        sample = resA.process(sample) + resB.process(sample * 0.9f);

        // Aggressive saturation
        sample = saturator.process(sample * 0.85f);

        // Apply envelope and velocity
        return sample * envValue * velocity * 0.7f;
    }

    bool isActive() const { return env.isActive(); }

    void reset() {
        env.reset();
        hp.reset();
        resA.reset();
        resB.reset();
        phaseA = phaseB = 0.0f;
    }
};

} // namespace flues::drumkit

#endif // BASH_VOICE_HPP
