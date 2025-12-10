// Bitcrusher.hpp
// Bit depth reduction for lo-fi industrial character
// Reduces 16-bit to 12/8/4-bit quantization

#ifndef BITCRUSHER_HPP
#define BITCRUSHER_HPP

#include <algorithm>
#include <cmath>

namespace flues::drumkit {

class Bitcrusher {
private:
    float amount;        // Crush amount [0, 1]

public:
    Bitcrusher()
        : amount(0.0f)
    {}

    /**
     * Set bit crush amount
     * @param amt - 0.0 = no crushing (16-bit), 1.0 = maximum crushing (4-bit)
     */
    void setAmount(float amt) {
        amount = std::clamp(amt, 0.0f, 1.0f);
    }

    /**
     * Process one sample through bit reduction
     */
    float process(float input) {
        if (amount < 0.01f) {
            return input;  // Bypass if amount is negligible
        }

        // Map amount to bit depth: 0.0→16-bit, 0.33→12-bit, 0.66→8-bit, 1.0→4-bit
        const float bitDepth = 16.0f - (amount * 12.0f);  // 16 to 4 bits
        const float levels = std::pow(2.0f, bitDepth);    // Quantization levels

        // Quantize the signal (handles negative values correctly)
        // Scale to [0, 1], quantize, then scale back to [-1, 1]
        const float normalized = (input + 1.0f) * 0.5f;  // -1..1 → 0..1
        const float quantized = std::floor(normalized * levels + 0.5f) / levels;
        const float output = quantized * 2.0f - 1.0f;  // 0..1 → -1..1

        return std::clamp(output, -1.0f, 1.0f);
    }
};

} // namespace flues::drumkit

#endif // BITCRUSHER_HPP
