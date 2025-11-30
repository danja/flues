// FormantModule.hpp
// Resonant bandpass filter for vocal formants
// Ported from experiments/chatterbox/src/audio/modules/FormantModule.js

#ifndef FORMANT_MODULE_HPP
#define FORMANT_MODULE_HPP

#include <cmath>
#include <algorithm>

class FormantModule {
private:
    float sampleRate;

    // Filter parameters
    float frequency;   // Center frequency in Hz
    float bandwidth;   // Bandwidth in Hz

    // Biquad filter coefficients
    float a0, a1, a2;
    float b1, b2;

    // Filter state
    float x1, x2;
    float y1, y2;

    static constexpr float TWO_PI = 6.28318530718f;

    /**
     * Update biquad filter coefficients for bandpass filter
     * Using Q-based parameterization for stability
     */
    void updateCoefficients() {
        // Calculate Q from bandwidth: Q = f0 / BW
        const float Q = std::max(0.5f, frequency / bandwidth);

        // Normalize frequency
        const float omega = TWO_PI * frequency / sampleRate;
        const float sn = std::sin(omega);
        const float cs = std::cos(omega);
        const float alpha = sn / (2.0f * Q);

        // Bandpass filter coefficients (constant 0 dB peak gain)
        const float a0_raw = 1.0f + alpha;
        const float a1_raw = -2.0f * cs;
        const float a2_raw = 1.0f - alpha;
        const float b0 = Q * alpha;
        const float b1_raw = 0.0f;
        const float b2_raw = -Q * alpha;

        // Normalize
        a0 = b0 / a0_raw;
        a1 = b1_raw / a0_raw;
        a2 = b2_raw / a0_raw;
        b1 = a1_raw / a0_raw;
        b2 = a2_raw / a0_raw;
    }

public:
    FormantModule(float sampleRate)
        : sampleRate(sampleRate)
        , frequency(500.0f)   // Center frequency in Hz
        , bandwidth(100.0f)   // Bandwidth in Hz
        , a0(1.0f), a1(0.0f), a2(0.0f)
        , b1(0.0f), b2(0.0f)
        , x1(0.0f), x2(0.0f)
        , y1(0.0f), y2(0.0f)
    {
        updateCoefficients();
    }

    /**
     * Set formant center frequency
     * @param freq - Frequency in Hz
     */
    void setFrequency(float freq) {
        frequency = std::clamp(freq, 20.0f, sampleRate / 2.0f);
        updateCoefficients();
    }

    /**
     * Set formant bandwidth
     * @param bw - Bandwidth in Hz
     */
    void setBandwidth(float bw) {
        bandwidth = std::clamp(bw, 10.0f, 5000.0f);
        updateCoefficients();
    }

    /**
     * Set formant Q (quality factor)
     * @param q - Q value (higher = narrower bandwidth)
     */
    void setQ(float q) {
        // Convert Q to bandwidth: BW = f0 / Q
        bandwidth = frequency / std::max(0.5f, q);
        updateCoefficients();
    }

    /**
     * Process one sample through the formant filter
     * @param input - Input sample
     * @return Filtered output
     */
    float process(float input) {
        // Biquad difference equation:
        // y[n] = a0*x[n] + a1*x[n-1] + a2*x[n-2] - b1*y[n-1] - b2*y[n-2]
        const float output = a0 * input + a1 * x1 + a2 * x2
                           - b1 * y1 - b2 * y2;

        // Update state
        x2 = x1;
        x1 = input;
        y2 = y1;
        y1 = output;

        // Stability check
        if (!std::isfinite(output)) {
            reset();
            return 0.0f;
        }

        return output;
    }

    /**
     * Reset filter state
     */
    void reset() {
        x1 = 0.0f;
        x2 = 0.0f;
        y1 = 0.0f;
        y2 = 0.0f;
    }
};

#endif // FORMANT_MODULE_HPP
