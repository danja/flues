// sources_module.c
// Excitation sources: DC, noise, and tone
// Translated from experiments/pm-synth/src/audio/modules/SourcesModule.js

#include "dsp_modules.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <math.h>

SourcesModule* sources_create(float sample_rate) {
    SourcesModule* sources = (SourcesModule*)calloc(1, sizeof(SourcesModule));
    if (!sources) return NULL;

    sources->sample_rate = sample_rate;
    sources->dc_level = 0.0f;
    sources->noise_level = 0.1f;
    sources->tone_level = 0.0f;
    sources->tone_phase = 0.0f;
    sources->tone_frequency = 440.0f;

    return sources;
}

void sources_destroy(SourcesModule* sources) {
    free(sources);
}

float sources_process(SourcesModule* sources) {
    // DC component
    float dc = sources->dc_level;

    // White noise component
    float noise = white_noise() * sources->noise_level;

    // Sawtooth tone component
    float tone = 0.0f;
    if (sources->tone_level > 0.0f) {
        tone = (sources->tone_phase * 2.0f - 1.0f) * sources->tone_level;
        sources->tone_phase += sources->tone_frequency / sources->sample_rate;
        if (sources->tone_phase >= 1.0f) {
            sources->tone_phase -= 1.0f;
        }
    }

    return dc + noise + tone;
}

void sources_set_dc_level(SourcesModule* sources, float level) {
    sources->dc_level = level;
}

void sources_set_noise_level(SourcesModule* sources, float level) {
    sources->noise_level = level;
}

void sources_set_tone_level(SourcesModule* sources, float level) {
    sources->tone_level = level;
}

void sources_set_tone_frequency(SourcesModule* sources, float frequency) {
    sources->tone_frequency = frequency;
}
