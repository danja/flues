// Formant Module - Resonant Bandpass Filter for Vocal Formants
// Ported from lv2/chatterbox/src/modules/FormantModule.hpp
// Uses struct definition from dsp_modules.h

#include "dsp_modules.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TWO_PI (2.0f * M_PI)

// Update biquad filter coefficients for bandpass filter
// Using Q-based parameterization for stability
static void update_coefficients(FormantModule* formant) {
    // Calculate Q from bandwidth: Q = f0 / BW
    float Q = (formant->frequency / formant->bandwidth);
    if (Q < 0.5f) Q = 0.5f;
    formant->q = Q;

    // Normalize frequency
    const float omega = TWO_PI * formant->frequency / formant->sample_rate;
    const float sn = sinf(omega);
    const float cs = cosf(omega);
    const float alpha = sn / (2.0f * Q);

    // Bandpass filter coefficients (constant 0 dB peak gain)
    const float a0_raw = 1.0f + alpha;
    const float a1_raw = -2.0f * cs;
    const float a2_raw = 1.0f - alpha;
    const float b0 = Q * alpha;
    const float b1_raw = 0.0f;
    const float b2_raw = -Q * alpha;

    // Normalize
    formant->a0 = b0 / a0_raw;
    formant->a1 = b1_raw / a0_raw;
    formant->a2 = b2_raw / a0_raw;
    formant->b1 = a1_raw / a0_raw;
    formant->b2 = a2_raw / a0_raw;
}

FormantModule* formant_create(float sample_rate) {
    FormantModule* formant = (FormantModule*)calloc(1, sizeof(FormantModule));
    if (!formant) return NULL;

    formant->sample_rate = sample_rate;
    formant->frequency = 500.0f;  // Default center frequency
    formant->bandwidth = 80.0f;   // Default bandwidth
    formant->q = 1.0f;

    // Initialize coefficients
    update_coefficients(formant);

    return formant;
}

void formant_destroy(FormantModule* formant) {
    free(formant);
}

void formant_set_frequency(FormantModule* formant, float frequency, float bandwidth) {
    // Clamp to valid range
    if (frequency < 20.0f) frequency = 20.0f;
    if (frequency > formant->sample_rate / 2.0f) frequency = formant->sample_rate / 2.0f;

    formant->frequency = frequency;
    formant->bandwidth = bandwidth;
    update_coefficients(formant);
}

float formant_process(FormantModule* formant, float input) {
    // Biquad difference equation:
    // y[n] = a0*x[n] + a1*x[n-1] + a2*x[n-2] - b1*y[n-1] - b2*y[n-2]
    const float output = formant->a0 * input + formant->a1 * formant->x1 + formant->a2 * formant->x2
                       - formant->b1 * formant->y1 - formant->b2 * formant->y2;

    // Update state
    formant->x2 = formant->x1;
    formant->x1 = input;
    formant->y2 = formant->y1;
    formant->y1 = output;

    // Stability check (avoid exploding values)
    if (!isfinite(output)) {
        formant->x1 = 0.0f;
        formant->x2 = 0.0f;
        formant->y1 = 0.0f;
        formant->y2 = 0.0f;
        return 0.0f;
    }

    return output;
}
