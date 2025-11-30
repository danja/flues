// AspiratorModule.hpp
// White noise generator for unvoiced/aspirated sounds
// Ported from experiments/chatterbox/src/audio/modules/AspiratorModule.js

#ifndef ASPIRATOR_MODULE_HPP
#define ASPIRATOR_MODULE_HPP

#include <algorithm>
#include <cstdint>

class AspiratorModule {
private:
    float level;
    bool enabled;

    // Simple LCG state for fast white noise
    uint32_t noiseState;

    /**
     * Generate white noise using linear congruential generator
     * @return Random value in range [-1, 1]
     */
    float generateNoise() {
        // LCG: Xn+1 = (a * Xn + c) mod m
        noiseState = noiseState * 1103515245u + 12345u;
        // Convert to float in [-1, 1]
        return (static_cast<int32_t>(noiseState) / 2147483648.0f);
    }

public:
    AspiratorModule(float sampleRate)
        : level(0.0f)
        , enabled(false)
        , noiseState(123456789u)  // Seed
    {
        (void)sampleRate;  // Unused but kept for API consistency
    }

    /**
     * Set the noise level
     * @param lvl - Amplitude 0-1
     */
    void setLevel(float lvl) {
        level = std::clamp(lvl, 0.0f, 1.0f);
    }

    /**
     * Enable or disable aspirated noise
     * @param enable - True to enable noise output
     */
    void setAspirated(bool enable) {
        enabled = enable;
    }

    /**
     * Process one sample
     * @return Output sample
     */
    float process() {
        if (!enabled || level <= 0.0f) {
            return 0.0f;
        }

        // Generate white noise
        const float noise = generateNoise();

        return noise * level;
    }
};

#endif // ASPIRATOR_MODULE_HPP
