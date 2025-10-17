#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "../Random.hpp"

namespace flues::pm {

class DelayLinesModule {
public:
    explicit DelayLinesModule(float sampleRate = 44100.0f)
        : sampleRate(sampleRate),
          maxDelayLength(static_cast<std::size_t>(sampleRate / 20.0f)),
          delayLine1(maxDelayLength, 0.0f),
          delayLine2(maxDelayLength, 0.0f),
          writePos1(0),
          writePos2(0),
          tuningSemitones(0.0f),
          ratio(1.0f),
          delayLength1(1000.0f),
          delayLength2(1000.0f),
          frequency(440.0f) {}

    void setTuning(float value) {
        tuningSemitones = (std::clamp(value, 0.0f, 1.0f) - 0.5f) * 24.0f;
        if (frequency > 0.0f) {
            updateDelayLengths(frequency);
        }
    }

    void setRatio(float value) {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        if (clamped < 0.5f) {
            ratio = 0.5f + clamped;
        } else {
            ratio = 1.0f + (clamped - 0.5f) * 2.0f;
        }
        if (frequency > 0.0f) {
            updateDelayLengths(frequency);
        }
    }

    void updateDelayLengths(float cv) {
        frequency = cv;
        const float tuningFactor = std::pow(2.0f, tuningSemitones / 12.0f);
        const float tunedFrequency = cv * tuningFactor;

        delayLength1 = std::clamp(sampleRate / tunedFrequency, 2.0f, static_cast<float>(maxDelayLength - 1));
        delayLength2 = std::clamp(delayLength1 * ratio, 2.0f, static_cast<float>(maxDelayLength - 1));
    }

    struct DelayOutputs {
        float delay1;
        float delay2;
    };

    DelayOutputs process(float input, float cv) {
        if (cv != frequency) {
            updateDelayLengths(cv);
        }

        const float o1 = readDelay(delayLine1, writePos1, delayLength1);
        const float o2 = readDelay(delayLine2, writePos2, delayLength2);

        delayLine1[writePos1] = input;
        delayLine2[writePos2] = input;

        writePos1 = (writePos1 + 1) % maxDelayLength;
        writePos2 = (writePos2 + 1) % maxDelayLength;

        return {o1, o2};
    }

    void reset() {
        std::fill(delayLine1.begin(), delayLine1.end(), 0.0f);
        std::fill(delayLine2.begin(), delayLine2.end(), 0.0f);
        writePos1 = 0;
        writePos2 = 0;

        const std::size_t limit = std::min<std::size_t>(100, maxDelayLength);
        for (std::size_t i = 0; i < limit; ++i) {
            delayLine1[i] = rng.uniformSignedFloat() * 0.01f;
            delayLine2[i] = rng.uniformSignedFloat() * 0.01f;
        }
    }

private:
    float readDelay(const std::vector<float>& buffer, std::size_t writePos, float delayLength) const {
        const float readPosFloat = static_cast<float>(writePos) - delayLength;
        float readPosWrapped = std::fmod(readPosFloat + static_cast<float>(maxDelayLength), static_cast<float>(maxDelayLength));
        if (readPosWrapped < 0.0f) {
            readPosWrapped += static_cast<float>(maxDelayLength);
        }

        const std::size_t readPosInt = static_cast<std::size_t>(std::floor(readPosWrapped));
        const float frac = readPosWrapped - static_cast<float>(readPosInt);
        const std::size_t nextPos = (readPosInt + 1) % maxDelayLength;

        return buffer[readPosInt] * (1.0f - frac) + buffer[nextPos] * frac;
    }

    float sampleRate;
    std::size_t maxDelayLength;
    std::vector<float> delayLine1;
    std::vector<float> delayLine2;
    std::size_t writePos1;
    std::size_t writePos2;
    float tuningSemitones;
    float ratio;
    float delayLength1;
    float delayLength2;
    float frequency;
    Random rng;
};

} // namespace flues::pm
