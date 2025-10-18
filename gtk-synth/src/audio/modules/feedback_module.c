// feedback_module.c
// Feedback mixer for delay lines and filter
// Translated from experiments/pm-synth/src/audio/modules/FeedbackModule.js

#include "dsp_modules.h"
#include <stdlib.h>

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
    return delay1 * fb->delay1_amount +
           delay2 * fb->delay2_amount +
           filter * fb->filter_amount;
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
