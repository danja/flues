#pragma once

#include <random>

namespace flues::pm {

/**
 * Shared random number generator wrapper.
 * Provides thread-safe (per-instance) uniform and normal variates.
 */
class Random {
public:
    Random()
        : engine(std::random_device{}()),
          uniform01(0.0f, 1.0f),
          uniformSigned(-1.0f, 1.0f),
          normalDist(0.0f, 1.0f) {}

    float uniform() {
        return uniform01(engine);
    }

    float uniformSignedFloat() {
        return uniformSigned(engine);
    }

    float normal(float mean = 0.0f, float stddev = 1.0f) {
        return static_cast<float>(normalDist(engine) * stddev + mean);
    }

private:
    std::mt19937 engine;
    std::uniform_real_distribution<float> uniform01;
    std::uniform_real_distribution<float> uniformSigned;
    std::normal_distribution<float> normalDist;
};

} // namespace flues::pm
