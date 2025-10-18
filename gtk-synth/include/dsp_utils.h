// dsp_utils.h
// DSP utility functions for physical modeling
// Translated from experiments/pm-synth/src/audio/modules/interface/utils

#ifndef DSP_UTILS_H
#define DSP_UTILS_H

#include <math.h>
#include <stdlib.h>

// ============================================================================
// Mathematical Constants
// ============================================================================

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_E
#define M_E 2.71828182845904523536
#endif

// Golden ratio and powers
#define PHI 1.618033988749895f
#define PHI2 2.618033988749895f

// ============================================================================
// Nonlinearity Functions
// ============================================================================

static inline float fast_tanh(float x) {
    const float TANH_CLIP_THRESHOLD = 3.0f;
    const float TANH_NUMERATOR_CONSTANT = 27.0f;
    const float TANH_DENOMINATOR_SCALE = 9.0f;

    if (x > TANH_CLIP_THRESHOLD) return 1.0f;
    if (x < -TANH_CLIP_THRESHOLD) return -1.0f;

    const float x2 = x * x;
    return x * (TANH_NUMERATOR_CONSTANT + x2) /
           (TANH_NUMERATOR_CONSTANT + TANH_DENOMINATOR_SCALE * x2);
}

static inline float hard_clip(float x) {
    if (x > 1.0f) return 1.0f;
    if (x < -1.0f) return -1.0f;
    return x;
}

static inline float soft_clip(float x) {
    if (x > 1.0f) return 1.0f;
    if (x < -1.0f) return -1.0f;
    return 1.5f * x - 0.5f * x * x * x;
}

static inline float cubic_waveshaper(float x, float alpha) {
    const float x3 = x * x * x;
    return x - alpha * x3;
}

static inline float sine_fold(float x, float drive) {
    return sinf(x * drive * M_PI * 0.5f);
}

static inline float soft_clip_drive(float x, float drive) {
    return fast_tanh(x * drive);
}

static inline float power_function(float x, float exponent) {
    return (x >= 0.0f) ? powf(x, exponent) : -powf(-x, exponent);
}

// ============================================================================
// Interpolation Functions
// ============================================================================

static inline float linear_interpolate(float y0, float y1, float frac) {
    return y0 + frac * (y1 - y0);
}

static inline float cubic_interpolate(float y0, float y1, float y2, float y3, float frac) {
    const float a0 = y3 - y2 - y0 + y1;
    const float a1 = y0 - y1 - a0;
    const float a2 = y2 - y0;
    const float a3 = y1;

    const float frac2 = frac * frac;
    return a0 * frac * frac2 + a1 * frac2 + a2 * frac + a3;
}

static inline float hermite_interpolate(float y0, float y1, float y2, float y3, float frac) {
    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

// ============================================================================
// Delay Line Utilities
// ============================================================================

// Read from circular buffer with interpolation
static inline float delay_read_interpolated(const float* buffer, int buffer_size,
                                            float read_pos) {
    const int i0 = (int)read_pos;
    const int i1 = (i0 + 1) % buffer_size;
    const float frac = read_pos - (float)i0;
    return linear_interpolate(buffer[i0], buffer[i1], frac);
}

// Write to circular buffer
static inline void delay_write(float* buffer, int buffer_size, int write_pos, float value) {
    buffer[write_pos % buffer_size] = value;
}

// ============================================================================
// White Noise Generator
// ============================================================================

static inline float white_noise(void) {
    return ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
}

// ============================================================================
// DC Blocker
// ============================================================================

typedef struct {
    float x1;
    float y1;
} DCBlocker;

static inline void dc_blocker_init(DCBlocker* blocker) {
    blocker->x1 = 0.0f;
    blocker->y1 = 0.0f;
}

static inline float dc_blocker_process(DCBlocker* blocker, float x) {
    const float R = 0.995f;
    const float y = x - blocker->x1 + R * blocker->y1;
    blocker->x1 = x;
    blocker->y1 = y;
    return y;
}

// ============================================================================
// Chaotic Oscillator (Logistic Map)
// ============================================================================

typedef struct {
    float r;  // Chaos parameter (3.57+ = chaotic)
    float x;
} ChaoticOscillator;

static inline void chaotic_oscillator_init(ChaoticOscillator* osc, float r) {
    osc->r = r;
    osc->x = 0.5f;
}

static inline void chaotic_oscillator_set_r(ChaoticOscillator* osc, float r) {
    osc->r = fmaxf(2.5f, fminf(4.0f, r));
}

static inline float chaotic_oscillator_process(ChaoticOscillator* osc, float amplitude) {
    osc->x = osc->r * osc->x * (1.0f - osc->x);
    // Map from [0, 1] to [-1, 1]
    return (osc->x * 2.0f - 1.0f) * amplitude;
}

static inline void chaotic_oscillator_reset(ChaoticOscillator* osc) {
    osc->x = 0.5f;
}

// ============================================================================
// Amplitude Tracker
// ============================================================================

typedef struct {
    float amplitude;
    float coefficient;
} AmplitudeTracker;

static inline void amplitude_tracker_init(AmplitudeTracker* tracker, float smoothing_time, float sample_rate) {
    tracker->amplitude = 0.0f;
    if (smoothing_time <= 0.0f) {
        tracker->coefficient = 0.0f;
    } else {
        tracker->coefficient = expf(-1.0f / (smoothing_time * sample_rate));
    }
}

static inline float amplitude_tracker_process(AmplitudeTracker* tracker, float sample) {
    const float instant = fabsf(sample);

    if (tracker->coefficient == 0.0f) {
        tracker->amplitude = instant;
    } else {
        tracker->amplitude = tracker->amplitude * tracker->coefficient + instant * (1.0f - tracker->coefficient);
    }

    return tracker->amplitude;
}

static inline void amplitude_tracker_reset(AmplitudeTracker* tracker) {
    tracker->amplitude = 0.0f;
}

#endif // DSP_UTILS_H
