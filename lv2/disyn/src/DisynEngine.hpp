#pragma once

#include <cmath>
#include <algorithm>

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
          reverb(sampleRate),
          frequency(440.0f),
          algorithmType(AlgorithmType::TANH_SQUARE),
          param1(0.55f),  // Default drive for tanh square
          param2(0.5f),   // Default trim for tanh square
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
        reverb.reset();

        envelope.setGate(true);
    }

    void noteOff() {
        gate = false;
        envelope.setGate(false);
    }

    float process() {
        if (!isPlaying) {
            return 0.0f;
        }

        // Generate oscillator sample
        const float oscSample = oscillator.process(algorithmType, param1, param2, frequency);

        // Apply envelope
        const float env = envelope.process();

        // Apply velocity and master gain
        const float sample = oscSample * env * velocity * masterGain;

        // Apply reverb
        const float output = reverb.process(sample);

        // Voice tail detection - stop if envelope is silent
        if (!envelope.isPlaying() && std::abs(output) < 1e-5f) {
            isPlaying = false;
        }

        return output;
    }

    // Parameter setters
    void setAlgorithm(int type) {
        if (type >= 0 && type <= 6) {
            algorithmType = static_cast<AlgorithmType>(type);
        }
    }

    void setParam1(float value) {
        param1 = std::clamp(value, 0.0f, 1.0f);
    }

    void setParam2(float value) {
        param2 = std::clamp(value, 0.0f, 1.0f);
    }

    void setAttack(float value) {
        envelope.setAttack(value);
    }

    void setRelease(float value) {
        envelope.setRelease(value);
    }

    void setReverbSize(float value) {
        reverb.setSize(value);
    }

    void setReverbLevel(float value) {
        reverb.setLevel(value);
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
    ReverbModule reverb;

    float frequency;
    AlgorithmType algorithmType;
    float param1;
    float param2;
    float masterGain;
    float velocity;
    bool gate;
    bool isPlaying;
};

} // namespace flues::disyn
