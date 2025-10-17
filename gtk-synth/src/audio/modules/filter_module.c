// filter_module.c
// State variable filter (LP/BP/HP morphing)
// Translated from experiments/pm-synth/src/audio/modules/FilterModule.js

#include "dsp_modules.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <math.h>

FilterModule* filter_create(float sample_rate) {
    FilterModule* filter = (FilterModule*)calloc(1, sizeof(FilterModule));
    if (!filter) return NULL;

    filter->sample_rate = sample_rate;
    filter->frequency = 1000.0f;
    filter->q = 1.0f;
    filter->shape = 0.0f; // Lowpass
    filter->low = 0.0f;
    filter->band = 0.0f;
    filter->high = 0.0f;

    return filter;
}

void filter_destroy(FilterModule* filter) {
    free(filter);
}

float filter_process(FilterModule* filter, float input) {
    // State variable filter implementation
    float f = 2.0f * sinf(M_PI * filter->frequency / filter->sample_rate);
    float q = 1.0f / filter->q;

    // Clamp to stable range
    if (f > 1.0f) f = 1.0f;
    if (q < 0.01f) q = 0.01f;

    // Update filter states
    filter->low += f * filter->band;
    filter->high = input - filter->low - q * filter->band;
    filter->band += f * filter->high;

    // Morph between LP, BP, HP based on shape (0=LP, 0.5=BP, 1=HP)
    float output;
    if (filter->shape < 0.5f) {
        // Morph from LP to BP
        float mix = filter->shape * 2.0f;
        output = filter->low * (1.0f - mix) + filter->band * mix;
    } else {
        // Morph from BP to HP
        float mix = (filter->shape - 0.5f) * 2.0f;
        output = filter->band * (1.0f - mix) + filter->high * mix;
    }

    return output;
}

void filter_set_frequency(FilterModule* filter, float frequency) {
    filter->frequency = frequency;
}

void filter_set_q(FilterModule* filter, float q) {
    filter->q = q;
}

void filter_set_shape(FilterModule* filter, float shape) {
    filter->shape = shape;
}
