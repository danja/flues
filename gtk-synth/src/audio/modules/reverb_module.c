// reverb_module.c
// Schroeder reverb (comb filters + allpass)
// Translated from experiments/pm-synth/src/audio/modules/ReverbModule.js

#include "dsp_modules.h"
#include <stdlib.h>
#include <string.h>

ReverbModule* reverb_create(float sample_rate) {
    ReverbModule* reverb = (ReverbModule*)calloc(1, sizeof(ReverbModule));
    if (!reverb) return NULL;

    reverb->sample_rate = sample_rate;
    reverb->size = 0.5f;
    reverb->wet_level = 0.3f;

    // Schroeder's delay line lengths (scaled by sample rate)
    float scale = sample_rate / 44100.0f;
    reverb->comb_size1 = (int)(1557 * scale);
    reverb->comb_size2 = (int)(1617 * scale);
    reverb->comb_size3 = (int)(1491 * scale);
    reverb->comb_size4 = (int)(1422 * scale);
    reverb->allpass_size1 = (int)(225 * scale);
    reverb->allpass_size2 = (int)(556 * scale);

    // Allocate delay line buffers
    reverb->comb1 = (float*)calloc(reverb->comb_size1, sizeof(float));
    reverb->comb2 = (float*)calloc(reverb->comb_size2, sizeof(float));
    reverb->comb3 = (float*)calloc(reverb->comb_size3, sizeof(float));
    reverb->comb4 = (float*)calloc(reverb->comb_size4, sizeof(float));
    reverb->allpass1 = (float*)calloc(reverb->allpass_size1, sizeof(float));
    reverb->allpass2 = (float*)calloc(reverb->allpass_size2, sizeof(float));

    reverb->pos1 = reverb->pos2 = reverb->pos3 = reverb->pos4 = 0;
    reverb->ap_pos1 = reverb->ap_pos2 = 0;

    return reverb;
}

void reverb_destroy(ReverbModule* reverb) {
    if (!reverb) return;
    free(reverb->comb1);
    free(reverb->comb2);
    free(reverb->comb3);
    free(reverb->comb4);
    free(reverb->allpass1);
    free(reverb->allpass2);
    free(reverb);
}

float reverb_process(ReverbModule* reverb, float input) {
    // Feedback coefficient based on room size
    const float feedback = 0.5f + reverb->size * 0.45f;

    // Process through 4 parallel comb filters
    float comb_out = 0.0f;

    // Comb 1
    float delayed1 = reverb->comb1[reverb->pos1];
    reverb->comb1[reverb->pos1] = input + delayed1 * feedback;
    comb_out += delayed1;
    reverb->pos1 = (reverb->pos1 + 1) % reverb->comb_size1;

    // Comb 2
    float delayed2 = reverb->comb2[reverb->pos2];
    reverb->comb2[reverb->pos2] = input + delayed2 * feedback;
    comb_out += delayed2;
    reverb->pos2 = (reverb->pos2 + 1) % reverb->comb_size2;

    // Comb 3
    float delayed3 = reverb->comb3[reverb->pos3];
    reverb->comb3[reverb->pos3] = input + delayed3 * feedback;
    comb_out += delayed3;
    reverb->pos3 = (reverb->pos3 + 1) % reverb->comb_size3;

    // Comb 4
    float delayed4 = reverb->comb4[reverb->pos4];
    reverb->comb4[reverb->pos4] = input + delayed4 * feedback;
    comb_out += delayed4;
    reverb->pos4 = (reverb->pos4 + 1) % reverb->comb_size4;

    comb_out *= 0.25f; // Average the 4 combs

    // Process through 2 series allpass filters
    const float allpass_feedback = 0.5f;

    // Allpass 1
    float ap1_delayed = reverb->allpass1[reverb->ap_pos1];
    float ap1_in = comb_out + ap1_delayed * allpass_feedback;
    float ap1_out = ap1_delayed - ap1_in * allpass_feedback;
    reverb->allpass1[reverb->ap_pos1] = ap1_in;
    reverb->ap_pos1 = (reverb->ap_pos1 + 1) % reverb->allpass_size1;

    // Allpass 2
    float ap2_delayed = reverb->allpass2[reverb->ap_pos2];
    float ap2_in = ap1_out + ap2_delayed * allpass_feedback;
    float ap2_out = ap2_delayed - ap2_in * allpass_feedback;
    reverb->allpass2[reverb->ap_pos2] = ap2_in;
    reverb->ap_pos2 = (reverb->ap_pos2 + 1) % reverb->allpass_size2;

    // Mix dry/wet
    return input * (1.0f - reverb->wet_level) + ap2_out * reverb->wet_level;
}

void reverb_set_size(ReverbModule* reverb, float size) {
    reverb->size = size;
}

void reverb_set_level(ReverbModule* reverb, float level) {
    reverb->wet_level = level;
}
