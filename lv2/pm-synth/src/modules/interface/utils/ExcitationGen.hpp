#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Random.hpp"

namespace flues::pm {

inline std::vector<float> generateTriangularProfile(std::size_t length,
                                                    float pickPosition = 0.5f,
                                                    float amplitude = 1.0f) {
    std::vector<float> buffer(length, 0.0f);
    const std::size_t pickSample = std::min<std::size_t>(length - 1, static_cast<std::size_t>(pickPosition * length));

    for (std::size_t i = 0; i < pickSample; ++i) {
        buffer[i] = amplitude * (static_cast<float>(i) / static_cast<float>(pickSample));
    }
    for (std::size_t i = pickSample; i < length; ++i) {
        buffer[i] = amplitude * (1.0f - static_cast<float>(i - pickSample) / std::max<std::size_t>(1, length - pickSample));
    }

    return buffer;
}

inline std::vector<float> generateNoiseBurst(std::size_t length,
                                             float amplitude = 1.0f,
                                             float decay = 0.95f,
                                             Random* rng = nullptr) {
    Random local;
    Random& random = rng ? *rng : local;
    std::vector<float> buffer(length, 0.0f);
    float envelope = 1.0f;

    for (std::size_t i = 0; i < length; ++i) {
        const float noise = random.uniformSignedFloat();
        buffer[i] = noise * amplitude * envelope;
        envelope *= decay;
    }

    return buffer;
}

inline std::vector<float> generateImpulse(std::size_t length,
                                          std::size_t position = 0,
                                          float amplitude = 1.0f) {
    std::vector<float> buffer(length, 0.0f);
    if (position < length) {
        buffer[position] = amplitude;
    }
    return buffer;
}

inline std::vector<float> generateVelocityBurst(std::size_t length,
                                                float amplitude = 1.0f,
                                                std::size_t width = 10) {
    std::vector<float> buffer(length, 0.0f);
    const float halfWidth = static_cast<float>(width) * 0.5f;

    for (std::size_t i = 0; i < std::min<std::size_t>(width, length); ++i) {
        const float t = (static_cast<float>(i) - halfWidth) / std::max(halfWidth, 1.0f);
        buffer[i] = amplitude * std::exp(-t * t * 4.0f);
    }

    return buffer;
}

inline float whiteNoise(float amplitude = 1.0f, Random* rng = nullptr) {
    Random local;
    Random& random = rng ? *rng : local;
    return random.uniformSignedFloat() * amplitude;
}

class PinkNoiseGenerator {
public:
    PinkNoiseGenerator()
        : b0(0), b1(0), b2(0), b3(0), b4(0), b5(0), b6(0) {}

    float process(float amplitude = 1.0f, Random* rng = nullptr) {
        Random local;
        Random& random = rng ? *rng : local;
        const float white = random.uniformSignedFloat();

        b0 = 0.99886f * b0 + white * 0.0555179f;
        b1 = 0.99332f * b1 + white * 0.0750759f;
        b2 = 0.96900f * b2 + white * 0.1538520f;
        b3 = 0.86650f * b3 + white * 0.3104856f;
        b4 = 0.55000f * b4 + white * 0.5329522f;
        b5 = -0.7616f * b5 - white * 0.0168980f;
        const float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
        b6 = white * 0.115926f;

        return pink * 0.11f * amplitude;
    }

    void reset() {
        b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
    }

private:
    float b0, b1, b2, b3, b4, b5, b6;
};

class ChaoticOscillator {
public:
    explicit ChaoticOscillator(float r = 3.8f)
        : r_(r), x_(0.5f) {}

    void setR(float r) {
        r_ = std::clamp(r, 2.5f, 4.0f);
    }

    float process(float amplitude = 1.0f) {
        x_ = r_ * x_ * (1.0f - x_);
        return (x_ * 2.0f - 1.0f) * amplitude;
    }

    void reset() {
        x_ = 0.5f;
    }

private:
    float r_;
    float x_;
};

inline float gaussianNoise(float mean = 0.0f, float stdDev = 1.0f, Random* rng = nullptr) {
    Random local;
    Random& random = rng ? *rng : local;
    return random.normal(mean, stdDev);
}

} // namespace flues::pm
