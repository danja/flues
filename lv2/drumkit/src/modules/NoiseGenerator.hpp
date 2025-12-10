// NoiseGenerator.hpp
// White noise generator using Linear Congruential Generator
// Used for snare, clap, hi-hat, crash synthesis

#ifndef NOISE_GENERATOR_HPP
#define NOISE_GENERATOR_HPP

#include <cstdint>

namespace flues::drumkit {

class NoiseGenerator {
private:
    uint32_t state;  // LCG state

public:
    explicit NoiseGenerator(uint32_t seed = 123456789u)
        : state(seed) {}

    /**
     * Generate one sample of white noise
     * @return Random value in range [-1, 1]
     */
    float process() {
        // Linear Congruential Generator: Xn+1 = (a * Xn + c) mod m
        // Using Numerical Recipes constants
        state = state * 1103515245u + 12345u;

        // Convert unsigned to signed and normalize to [-1, 1]
        return static_cast<int32_t>(state) / 2147483648.0f;
    }

    /**
     * Reset the noise generator with a new seed
     */
    void reset(uint32_t seed = 123456789u) {
        state = seed;
    }
};

} // namespace flues::drumkit

#endif // NOISE_GENERATOR_HPP
