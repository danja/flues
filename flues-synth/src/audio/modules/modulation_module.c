// modulation_module.c
// LFO modulation (bipolar AM<->FM)
// Modified for flues-synth two-step processing

#include "dsp_modules.h"
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ModulationModule* modulation_create(float sample_rate) {
    ModulationModule* mod = (ModulationModule*)calloc(1, sizeof(ModulationModule));
    if (!mod) return NULL;

    mod->sample_rate = sample_rate;
    mod->frequency = 5.0f;
    mod->depth = 0.0f;  // Center = no modulation
    mod->phase = 0.0f;

    return mod;
}

void modulation_destroy(ModulationModule* mod) {
    free(mod);
}

void modulation_process(ModulationModule* mod) {
    // Advance LFO phase
    mod->phase += mod->frequency / mod->sample_rate;
    if (mod->phase >= 1.0f) {
        mod->phase -= 1.0f;
    }
}

float modulation_get_am_scale(ModulationModule* mod) {
    // Generate LFO (sine wave)
    float lfo = sinf(2.0f * M_PI * mod->phase);

    // For AM: depth < 0, scale amplitude
    // For FM: depth > 0, would affect frequency (not used in AM scale)
    if (mod->depth < 0.0f) {
        float am_amount = -mod->depth;
        return 1.0f + lfo * am_amount * 0.5f;
    }

    return 1.0f;  // No AM modulation
}

void modulation_set_frequency(ModulationModule* mod, float frequency) {
    mod->frequency = frequency;
}

void modulation_set_depth(ModulationModule* mod, float depth) {
    // Depth range: -1 (AM) to +1 (FM)
    if (depth < -1.0f) depth = -1.0f;
    if (depth > 1.0f) depth = 1.0f;
    mod->depth = depth;
}
