// envelope_module.c
// Attack/Release envelope with gate
// Translated from experiments/pm-synth/src/audio/modules/EnvelopeModule.js

#include "dsp_modules.h"
#include <stdlib.h>
#include <math.h>

EnvelopeModule* envelope_create(float sample_rate) {
    EnvelopeModule* env = (EnvelopeModule*)calloc(1, sizeof(EnvelopeModule));
    if (!env) return NULL;

    env->sample_rate = sample_rate;
    env->attack_time = 0.01f;  // 10ms default
    env->release_time = 0.1f;   // 100ms default
    env->envelope = 0.0f;
    env->gate = false;
    env->previous_gate = false;

    return env;
}

void envelope_destroy(EnvelopeModule* env) {
    free(env);
}

float envelope_process(EnvelopeModule* env, float input) {
    // Calculate attack/release coefficients
    float attack_coeff = expf(-1.0f / (env->attack_time * env->sample_rate));
    float release_coeff = expf(-1.0f / (env->release_time * env->sample_rate));

    // Update envelope based on gate
    if (env->gate) {
        // Attack phase
        env->envelope += (1.0f - env->envelope) * (1.0f - attack_coeff);
    } else {
        // Release phase
        env->envelope *= release_coeff;
    }

    env->previous_gate = env->gate;

    return input * env->envelope;
}

void envelope_set_gate(EnvelopeModule* env, bool gate) {
    env->gate = gate;
}

void envelope_set_attack(EnvelopeModule* env, float attack) {
    // Map 0-1 to 1ms-1000ms exponentially
    env->attack_time = 0.001f * powf(1000.0f, attack);
}

void envelope_set_release(EnvelopeModule* env, float release) {
    // Map 0-1 to 10ms-5000ms exponentially
    env->release_time = 0.01f * powf(500.0f, release);
}
