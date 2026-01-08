#pragma once

#include <cmath>
#include <algorithm>

#include "algorithms/AlgorithmOutput.hpp"
#include "algorithms/AlgorithmTypes.hpp"
#include "modules/OscillatorModule.hpp"
#include "modules/EnvelopeModule.hpp"
#include "modules/ReverbModule.hpp"

namespace flues::disyn {

class DisynEngine {
public:
    explicit DisynEngine(float sampleRate = 44100.0f)
        : sampleRate(sampleRate),
          oscillator(sampleRate),
          envelope(sampleRate),
          reverbLeft(sampleRate),
          reverbRight(sampleRate),
          frequency(440.0f),
          algorithmType(AlgorithmType::TANH_SQUARE),
          param1(0.55f),  // Default drive for tanh square
          param2(0.5f),   // Default trim for tanh square
          param3(0.5f),
          masterGain(0.8f),
          velocity(1.0f),
          gate(false),
          isPlaying(false) {}

    void noteOn(float freq, float vel = 1.0f) {
        frequency = freq;
        velocity = std::clamp(vel, 0.0f, 1.0f);
        gate = true;
        isPlaying = true;

        oscillator.reset();
        envelope.reset();
        reverbLeft.reset();
        reverbRight.reset();

        envelope.setGate(true);
    }

    void noteOff() {
        gate = false;
        envelope.setGate(false);
    }

    AlgorithmOutput process() {
        if (!isPlaying) {
            return {0.0f, 0.0f};
        }

        // Generate oscillator sample
        const AlgorithmOutput oscOutput = oscillator.process(algorithmType, frequency, param1, param2, param3);

        // Apply envelope
        const float env = envelope.process();

        // Apply velocity and master gain
        const float leftSample = oscOutput.primary * env * velocity * masterGain;
        const float rightSample = oscOutput.secondary * env * velocity * masterGain;

        // Apply reverb
        const float left = reverbLeft.process(leftSample);
        const float right = reverbRight.process(rightSample);

        // Voice tail detection - stop if envelope is silent
        if (!envelope.isPlaying() &&
            std::max(std::abs(left), std::abs(right)) < 1e-5f) {
            isPlaying = false;
        }

        return {left, right};
    }

    // Parameter setters
    void setAlgorithm(int type) {
        if (type >= 0 && type <= 18) {
            algorithmType = static_cast<AlgorithmType>(type);
        }
    }

    void setParam1(float value) {
        param1 = std::clamp(value, 0.0f, 1.0f);
    }

    void setParam2(float value) {
        param2 = std::clamp(value, 0.0f, 1.0f);
    }

    void setParam3(float value) {
        param3 = std::clamp(value, 0.0f, 1.0f);
    }

    void setAttack(float value) {
        envelope.setAttack(value);
    }

    void setRelease(float value) {
        envelope.setRelease(value);
    }

    void setReverbSize(float value) {
        reverbLeft.setSize(value);
        reverbRight.setSize(value);
    }

    void setReverbLevel(float value) {
        reverbLeft.setLevel(value);
        reverbRight.setLevel(value);
    }

    void setMasterGain(float value) {
        masterGain = std::clamp(value, 0.0f, 1.0f);
    }

    bool getIsPlaying() const {
        return isPlaying;
    }

private:
    float sampleRate;
    OscillatorModule oscillator;
    EnvelopeModule envelope;
    ReverbModule reverbLeft;
    ReverbModule reverbRight;

    float frequency;
    AlgorithmType algorithmType;
    float param1;
    float param2;
    float param3;
    float masterGain;
    float velocity;
    bool gate;
    bool isPlaying;
};

} // namespace flues::disyn
