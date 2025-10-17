#pragma once

#include <algorithm>
#include <cmath>
#include <vector>
#include <limits>

namespace flues::pm {

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float hermiteInterpolate(float xm1, float x0, float x1, float x2, float frac) {
    const float c0 = x0;
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

inline float cubicInterpolate(float xm1, float x0, float x1, float x2, float frac) {
    const float a0 = x2 - x1 - xm1 + x0;
    const float a1 = xm1 - x0 - a0;
    const float a2 = x1 - xm1;
    const float a3 = x0;
    return ((a0 * frac + a1) * frac + a2) * frac + a3;
}

inline float allpassCoefficient(float delay) {
    return (1.0f - delay) / (1.0f + delay);
}

class AllpassDelay {
public:
    AllpassDelay()
        : x1(0.0f), y1(0.0f) {}

    float process(float input, float coefficient) {
        const float output = coefficient * input + x1 - coefficient * y1;
        x1 = input;
        y1 = output;
        return output;
    }

    void reset() {
        x1 = 0.0f;
        y1 = 0.0f;
    }

private:
    float x1;
    float y1;
};

class FractionalDelayLine {
public:
    explicit FractionalDelayLine(std::size_t maxLength)
        : buffer(maxLength, 0.0f),
          maxLength(maxLength),
          writePos(0) {}

    void write(float sample) {
        buffer[writePos] = sample;
        writePos = (writePos + 1) % maxLength;
    }

    float readLinear(float delayLength) const {
        const float readPosFloat = static_cast<float>(writePos) - delayLength;
        float readPosWrapped = std::fmod(readPosFloat + static_cast<float>(maxLength), static_cast<float>(maxLength));
        if (readPosWrapped < 0.0f) {
            readPosWrapped += static_cast<float>(maxLength);
        }
        const std::size_t readPosInt = static_cast<std::size_t>(std::floor(readPosWrapped));
        const float frac = readPosWrapped - static_cast<float>(readPosInt);
        const std::size_t nextPos = (readPosInt + 1) % maxLength;
        return lerp(buffer[readPosInt], buffer[nextPos], frac);
    }

    float readHermite(float delayLength) const {
        const float readPosFloat = static_cast<float>(writePos) - delayLength;
        float readPosWrapped = std::fmod(readPosFloat + static_cast<float>(maxLength), static_cast<float>(maxLength));
        if (readPosWrapped < 0.0f) {
            readPosWrapped += static_cast<float>(maxLength);
        }
        const std::size_t readPosInt = static_cast<std::size_t>(std::floor(readPosWrapped));
        const float frac = readPosWrapped - static_cast<float>(readPosInt);

        const std::size_t pos0 = readPosInt % maxLength;
        const std::size_t posm1 = (pos0 + maxLength - 1) % maxLength;
        const std::size_t pos1 = (pos0 + 1) % maxLength;
        const std::size_t pos2 = (pos0 + 2) % maxLength;

        return hermiteInterpolate(buffer[posm1], buffer[pos0], buffer[pos1], buffer[pos2], frac);
    }

    void reset() {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    std::size_t size() const {
        return maxLength;
    }

private:
    std::vector<float> buffer;
    std::size_t maxLength;
    std::size_t writePos;
};

inline std::pair<float, float> calculateSafeDelayRange(float baseDelay, float modAmount,
                                                      float minDelay = 2.0f,
                                                      float maxDelay = std::numeric_limits<float>::infinity()) {
    const float minVal = std::max(minDelay, baseDelay - modAmount);
    const float maxVal = std::min(maxDelay, baseDelay + modAmount);
    return {minVal, maxVal};
}

class DelayLengthSmoother {
public:
    DelayLengthSmoother(float smoothingTime = 0.01f, float sampleRate = 44100.0f)
        : current(0.0f) {
        setSmoothing(smoothingTime, sampleRate);
    }

    void setSmoothing(float time, float sampleRate) {
        coefficient = std::exp(-1.0f / (time * sampleRate));
    }

    float process(float target) {
        current = current * coefficient + target * (1.0f - coefficient);
        return current;
    }

    void reset(float value = 0.0f) {
        current = value;
    }

private:
    float current;
    float coefficient;
};

} // namespace flues::pm
