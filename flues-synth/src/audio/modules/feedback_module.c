// feedback_module.c
// Feedback mixer for delay lines and filter
// Translated from experiments/pm-synth/src/audio/modules/FeedbackModule.js

#include "dsp_modules.h"
#include "dsp_utils.h"
#include <stdlib.h>

// Softly limit feedback to prevent runaway energy while keeping normal range linear
static inline float feedback_saturate(float x) {
    // Leave generous headroom before the soft clip engages
    const float trimmed = clamp(x, -2.0f, 2.0f);
    return fast_tanh(trimmed);  // drive=1 keeps small signals essentially linear
}

FeedbackModule* feedback_create(void) {
    FeedbackModule* fb = (FeedbackModule*)calloc(1, sizeof(FeedbackModule));
    if (!fb) return NULL;

    // Defaults match JavaScript: 0.95, 0.95, 0.0
    fb->delay1_amount = 0.95f;
    fb->delay2_amount = 0.95f;
    fb->filter_amount = 0.0f;

    return fb;
}

void feedback_destroy(FeedbackModule* fb) {
    free(fb);
}

float feedback_process(FeedbackModule* fb, float delay1, float delay2, float filter) {
    const float mix = delay1 * fb->delay1_amount +
                      delay2 * fb->delay2_amount +
                      filter * fb->filter_amount;

    const float safe_mix = sanitize_sample(mix);
    return feedback_saturate(safe_mix);
}

void feedback_set_delay1(FeedbackModule* fb, float amount) {
    fb->delay1_amount = amount;
}

void feedback_set_delay2(FeedbackModule* fb, float amount) {
    fb->delay2_amount = amount;
}

void feedback_set_filter(FeedbackModule* fb, float amount) {
    fb->filter_amount = amount;
}
