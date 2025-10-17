#pragma once

#include <algorithm>
#include <cmath>

namespace flues::pm {

inline float fastTanh(float x) {
    constexpr float kClip = 3.0f;
    constexpr float kNum = 27.0f;
    constexpr float kDenScale = 9.0f;

    if (x > kClip) {
        return 1.0f;
    }
    if (x < -kClip) {
        return -1.0f;
    }

    const float x2 = x * x;
    return x * (kNum + x2) / (kNum + kDenScale * x2);
}

inline float hardClip(float x, float threshold = 1.0f) {
    return std::clamp(x, -threshold, threshold);
}

inline float softClip(float x, float drive = 1.0f) {
    return fastTanh(x * drive);
}

inline float powerFunction(float x, float alpha) {
    if (x <= 0.0f) {
        return 0.0f;
    }

    if (alpha == 1.0f) {
        return x;
    }
    if (alpha == 2.0f) {
        return x * x;
    }
    if (alpha == 3.0f) {
        return x * x * x;
    }
    if (alpha == 0.5f) {
        return std::sqrt(x);
    }

    return std::exp(alpha * std::log(x));
}

inline float cubicWaveshaper(float x, float alpha = 0.33f) {
    return x - alpha * x * x * x;
}

inline float polynomialWaveshaper(float x, float a1 = 1.0f, float a3 = -0.33f, float a5 = 0.1f) {
    const float x2 = x * x;
    const float x3 = x2 * x;
    const float x5 = x3 * x2;
    return a1 * x + a3 * x3 + a5 * x5;
}

inline float frictionCurve(float velocity, float normalForce = 1.0f) {
    constexpr float kMuStatic = 0.8f;
    constexpr float kMuDynamic = 0.6f;
    constexpr float kMuViscous = 0.05f;

    const float vStr = 0.01f + normalForce * 0.09f;
    const float absV = std::abs(velocity);
    const float mu = kMuStatic + (kMuDynamic - kMuStatic) * std::exp(-absV / vStr)
                     + kMuViscous * absV;

    return normalForce * mu * (velocity >= 0 ? 1.0f : -1.0f);
}

inline float sigmoid(float x, float steepness = 1.0f) {
    return 1.0f / (1.0f + std::exp(-steepness * x));
}

inline float sineFold(float x, float drive = 1.0f) {
    return std::sin(x * drive * static_cast<float>(M_PI) * 0.5f);
}

inline float asymmetricShape(float x, float posGain = 1.0f, float negGain = 1.0f) {
    if (x >= 0.0f) {
        return fastTanh(x * posGain);
    }
    return fastTanh(x * negGain);
}

} // namespace flues::pm
