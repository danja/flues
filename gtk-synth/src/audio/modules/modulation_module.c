// modulation_module.c
// LFO modulation (bipolar AM<->FM)
// Translated from experiments/pm-synth/src/audio/modules/ModulationModule.js

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

float modulation_process(ModulationModule* mod, float input) {
    // Generate LFO (sine wave)
    float lfo = sinf(2.0f * M_PI * mod->phase);

    // Bipolar depth: -1 = full AM, 0 = no mod, +1 = full FM
    float output;
    if (mod->depth < 0.0f) {
        // Amplitude modulation
        float am_amount = -mod->depth;
        float am = 1.0f + lfo * am_amount * 0.5f;
        output = input * am;
    } else if (mod->depth > 0.0f) {
        // Frequency modulation (phase modulation approximation)
        // This is a simplified FM - true FM would modulate the delay line length
        float fm_amount = mod->depth;
        float phase_mod = lfo * fm_amount * 0.1f;
        output = input * (1.0f + phase_mod);
    } else {
        output = input;
    }

    // Advance LFO phase
    mod->phase += mod->frequency / mod->sample_rate;
    if (mod->phase >= 1.0f) {
        mod->phase -= 1.0f;
    }

    return output;
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
