#ifndef FLUES_SYNTH_DSP_UTILS_H
#define FLUES_SYNTH_DSP_UTILS_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

// Fast tanh approximation using rational polynomial
// Matches the approximation used in gtk-synth and LV2 plugins
static inline float fast_tanh(float x) {
    if (x < -3.0f) return -1.0f;
    if (x > 3.0f) return 1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Golden ratio and powers
#define PHI 1.618033988749895f
#define PHI2 2.618033988749895f

// Cubic waveshaper for nonlinear distortion
static inline float cubic_waveshaper(float x, float alpha) {
    const float x3 = x * x * x;
    return x - alpha * x3;
}

// Sine wavefolder for distortion effects
static inline float sine_fold(float x, float drive) {
    return sinf(x * drive * M_PI * 0.5f);
}

// Soft clipping with drive
static inline float soft_clip_drive(float x, float drive) {
    return fast_tanh(x * drive);
}

// Sanitize potentially bad samples (NaN/Inf) and hard-limit extreme magnitudes
static inline float sanitize_sample(float x) {
    if (!isfinite(x)) return 0.0f;
    if (x > 8.0f) return 8.0f;
    if (x < -8.0f) return -8.0f;
    return x;
}

// Linear interpolation
static inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

// Alias for compatibility with gtk-synth code
static inline float linear_interpolate(float a, float b, float t) {
    return lerp(a, b, t);
}

// Read from circular buffer with linear interpolation
static inline float delay_read_interpolated(const float* buffer, int buffer_size,
                                            float read_pos) {
    const int i0 = (int)read_pos;
    const int i1 = (i0 + 1) % buffer_size;
    const float frac = read_pos - (float)i0;
    return linear_interpolate(buffer[i0], buffer[i1], frac);
}

// MIDI note to frequency conversion
static inline float midi_note_to_frequency(int note) {
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

// Exponential parameter mapping (0-1 → min to max)
static inline float exp_map(float normalized, float min, float max) {
    return min * powf(max / min, normalized);
}

// Random number generator (LCG)
typedef struct {
    uint32_t state;
} Random;

static inline void random_init(Random* rng, uint32_t seed) {
    rng->state = seed;
}

static inline float random_uniform(Random* rng) {
    rng->state = rng->state * 1103515245 + 12345;
    return (float)(rng->state >> 16) / 32768.0f;
}

static inline float random_uniform_signed(Random* rng) {
    return random_uniform(rng) * 2.0f - 1.0f;
}

// Global white noise generator (for simple use cases)
static Random g_noise_rng = {1234567};

static inline float white_noise(void) {
    return random_uniform_signed(&g_noise_rng);
}

// DC blocker (first-order high-pass filter)
typedef struct {
    float x1;
    float y1;
    float r;  // Coefficient (typically 0.995)
} DCBlocker;

static inline void dc_blocker_init(DCBlocker* dc, float r) {
    dc->x1 = 0.0f;
    dc->y1 = 0.0f;
    dc->r = r;
}

static inline float dc_blocker_process(DCBlocker* dc, float input) {
    float output = input - dc->x1 + dc->r * dc->y1;
    dc->x1 = input;
    dc->y1 = output;
    return output;
}

// Clamp value to range
static inline float clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
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

#endif // FLUES_SYNTH_DSP_UTILS_H
