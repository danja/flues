// Envelope Module - Attack/Release Envelope for Voice Shaping
// Ported from lv2/chatterbox/src/modules/EnvelopeModule.hpp
// Uses struct definition from dsp_modules.h

#include "dsp_modules.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <math.h>

#define MIN_ATTACK 0.001f
#define MAX_ATTACK 1.0f
#define MIN_RELEASE 0.01f
#define MAX_RELEASE 3.0f

EnvelopeModule* envelope_create(float sample_rate) {
    EnvelopeModule* envelope = (EnvelopeModule*)calloc(1, sizeof(EnvelopeModule));
    if (!envelope) return NULL;

    envelope->sample_rate = sample_rate;
    envelope->attack_samples = 0.01f * sample_rate;  // Default 10ms attack
    envelope->release_samples = 0.05f * sample_rate;  // Default 50ms release
    envelope->current_level = 0.0f;
    envelope->gate = false;
    envelope->state = ENVELOPE_IDLE;

    return envelope;
}

void envelope_destroy(EnvelopeModule* envelope) {
    free(envelope);
}

void envelope_set_attack(EnvelopeModule* envelope, float normalized) {
    // Exponential mapping: 1ms to 1000ms
    float attack_seconds = exp_map(normalized, MIN_ATTACK, MAX_ATTACK);
    envelope->attack_samples = attack_seconds * envelope->sample_rate;
}

void envelope_set_release(EnvelopeModule* envelope, float normalized) {
    // Exponential mapping: 10ms to 3000ms
    float release_seconds = exp_map(normalized, MIN_RELEASE, MAX_RELEASE);
    envelope->release_samples = release_seconds * envelope->sample_rate;
}

void envelope_set_gate(EnvelopeModule* envelope, bool gate) {
    envelope->gate = gate;
    if (gate && envelope->state == ENVELOPE_IDLE) {
        envelope->state = ENVELOPE_ATTACK;
    } else if (!gate && (envelope->state == ENVELOPE_ATTACK || envelope->state == ENVELOPE_SUSTAIN)) {
        envelope->state = ENVELOPE_RELEASE;
    }
}

float envelope_process(EnvelopeModule* envelope) {
    switch (envelope->state) {
        case ENVELOPE_IDLE:
            return 0.0f;

        case ENVELOPE_ATTACK: {
            const float attack_rate = 1.0f / envelope->attack_samples;
            envelope->current_level += attack_rate;

            if (envelope->current_level >= 1.0f) {
                envelope->current_level = 1.0f;
                envelope->state = ENVELOPE_SUSTAIN;
            }
            break;
        }

        case ENVELOPE_SUSTAIN:
            // Hold at 1.0
            if (!envelope->gate) {
                envelope->state = ENVELOPE_RELEASE;
            }
            break;

        case ENVELOPE_RELEASE: {
            const float release_rate = 1.0f / envelope->release_samples;
            envelope->current_level -= release_rate;

            if (envelope->current_level <= 0.0f) {
                envelope->current_level = 0.0f;
                envelope->state = ENVELOPE_IDLE;
            }
            break;
        }
    }

    return envelope->current_level;
}

bool envelope_is_active(EnvelopeModule* envelope) {
    return envelope->state != ENVELOPE_IDLE;
}

void envelope_reset(EnvelopeModule* envelope) {
    envelope->current_level = 0.0f;
    envelope->gate = false;
    envelope->state = ENVELOPE_IDLE;
}
