#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace flues::pm {

class RMSTracker {
public:
    explicit RMSTracker(std::size_t windowSize = 1024)
        : buffer(windowSize, 0.0f),
          writePos(0),
          sumOfSquares(0.0f) {}

    float process(float sample) {
        const float oldSample = buffer[writePos];
        sumOfSquares -= oldSample * oldSample;

        buffer[writePos] = sample;
        sumOfSquares += sample * sample;

        writePos = (writePos + 1) % buffer.size();
        return std::sqrt(sumOfSquares / static_cast<float>(buffer.size()));
    }

    float getRMS() const {
        return std::sqrt(sumOfSquares / static_cast<float>(buffer.size()));
    }

    void reset() {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        sumOfSquares = 0.0f;
    }

private:
    std::vector<float> buffer;
    std::size_t writePos;
    float sumOfSquares;
};

class PeakEnvelopeFollower {
public:
    PeakEnvelopeFollower(float attackTime = 0.001f, float releaseTime = 0.1f, float sampleRate = 44100.0f)
        : peak(0.0f) {
        setTimes(attackTime, releaseTime, sampleRate);
    }

    void setTimes(float attackTime, float releaseTime, float sampleRate) {
        attackCoeff = std::exp(-1.0f / (attackTime * sampleRate));
        releaseCoeff = std::exp(-1.0f / (releaseTime * sampleRate));
    }

    float process(float sample) {
        const float rectified = std::abs(sample);
        if (rectified > peak) {
            peak = peak * attackCoeff + rectified * (1.0f - attackCoeff);
        } else {
            peak = peak * releaseCoeff + rectified * (1.0f - releaseCoeff);
        }
        return peak;
    }

    float getPeak() const {
        return peak;
    }

    void reset() {
        peak = 0.0f;
    }

private:
    float peak;
    float attackCoeff;
    float releaseCoeff;
};

class LeakyIntegrator {
public:
    LeakyIntegrator(float timeConstant = 0.1f, float sampleRate = 44100.0f)
        : value(0.0f) {
        setTimeConstant(timeConstant, sampleRate);
    }

    void setTimeConstant(float timeConstant, float sampleRate) {
        coefficient = std::exp(-1.0f / (timeConstant * sampleRate));
    }

    float process(float sample) {
        value = value * coefficient + sample * (1.0f - coefficient);
        return value;
    }

    float getValue() const {
        return value;
    }

    void reset(float initialValue = 0.0f) {
        value = initialValue;
    }

private:
    float value;
    float coefficient;
};

class EnergyAccumulator {
public:
    explicit EnergyAccumulator(float decayRate = 0.9f)
        : energy(0.0f),
          decayRate(decayRate) {}

    void setDecayRate(float rate) {
        decayRate = std::clamp(rate, 0.0f, 1.0f);
    }

    float process(float input) {
        energy = energy * decayRate + std::abs(input);
        return energy;
    }

    float getEnergy() const {
        return energy;
    }

    void reset() {
        energy = 0.0f;
    }

private:
    float energy;
    float decayRate;
};

class AmplitudeTracker {
public:
    AmplitudeTracker(float smoothing = 0.0f, float sampleRate = 44100.0f)
        : amplitude(0.0f) {
        setSmoothing(smoothing, sampleRate);
    }

    void setSmoothing(float time, float sampleRate) {
        if (time <= 0.0f) {
            coefficient = 0.0f;
        } else {
            coefficient = std::exp(-1.0f / (time * sampleRate));
        }
    }

    float process(float sample) {
        const float instant = std::abs(sample);
        if (coefficient == 0.0f) {
            amplitude = instant;
        } else {
            amplitude = amplitude * coefficient + instant * (1.0f - coefficient);
        }
        return amplitude;
    }

    float getAmplitude() const {
        return amplitude;
    }

    void reset() {
        amplitude = 0.0f;
    }

private:
    float amplitude;
    float coefficient;
};

class ZeroCrossingDetector {
public:
    explicit ZeroCrossingDetector(std::size_t windowSize = 256)
        : windowSize(windowSize),
          prevSample(0.0f),
          crossingCount(0),
          sampleCount(0) {}

    float process(float sample) {
        if ((prevSample >= 0.0f && sample < 0.0f) || (prevSample < 0.0f && sample >= 0.0f)) {
            ++crossingCount;
        }

        ++sampleCount;

        float rate = static_cast<float>(crossingCount) / std::max<std::size_t>(1, sampleCount);
        if (sampleCount >= windowSize) {
            rate = static_cast<float>(crossingCount) / static_cast<float>(windowSize);
            crossingCount = 0;
            sampleCount = 0;
        }

        prevSample = sample;
        return rate;
    }

    void reset() {
        prevSample = 0.0f;
        crossingCount = 0;
        sampleCount = 0;
    }

private:
    std::size_t windowSize;
    float prevSample;
    std::size_t crossingCount;
    std::size_t sampleCount;
};

} // namespace flues::pm
