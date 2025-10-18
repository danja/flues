// pluck_strategy.c
// Pluck interface: One-way damping with transient brightening
// Translated from reference/modules/interface/strategies/PluckStrategy.js

#include "interface_strategy.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <math.h>

typedef struct {
    float last_peak;
    float peak_decay;
    float prev_input;
} PluckImpl;

static float pluck_process(InterfaceStrategy* self, float input) {
    PluckImpl* impl = (PluckImpl*)self->impl_data;

    const float brightness = 0.2f + self->intensity * 0.45f;
    float response;

    if (fabsf(input) > fabsf(impl->last_peak)) {
        // Let the first spike through but brighten it slightly
        impl->last_peak = input;
        response = input;
    } else {
        impl->last_peak *= impl->peak_decay;
        const float transient = (input - impl->prev_input) * brightness;
        const float damp = 0.35f + (1.0f - self->intensity) * 0.45f;
        response = input * damp + transient;
    }

    impl->prev_input = input;
    return fmaxf(-1.0f, fminf(1.0f, response));
}

static void pluck_reset(InterfaceStrategy* self) {
    PluckImpl* impl = (PluckImpl*)self->impl_data;
    impl->last_peak = 0.0f;
    impl->prev_input = 0.0f;
}

static void pluck_set_intensity(InterfaceStrategy* self, float intensity) {
    self->intensity = intensity;
}

static void pluck_set_gate(InterfaceStrategy* self, bool gate) {
    if (gate) {
        pluck_reset(self);
    }
}

static void pluck_destroy(InterfaceStrategy* self) {
    if (self) {
        free(self->impl_data);
        free(self);
    }
}

static InterfaceStrategyVTable pluck_vtable = {
    .process = pluck_process,
    .reset = pluck_reset,
    .set_intensity = pluck_set_intensity,
    .set_gate = pluck_set_gate,
    .destroy = pluck_destroy
};

InterfaceStrategy* pluck_strategy_create(float sample_rate) {
    (void)sample_rate;  // Unused

    InterfaceStrategy* strategy = (InterfaceStrategy*)calloc(1, sizeof(InterfaceStrategy));
    if (!strategy) return NULL;

    PluckImpl* impl = (PluckImpl*)calloc(1, sizeof(PluckImpl));
    if (!impl) {
        free(strategy);
        return NULL;
    }

    impl->last_peak = 0.0f;
    impl->peak_decay = 0.999f;
    impl->prev_input = 0.0f;

    strategy->vtable = &pluck_vtable;
    strategy->impl_data = impl;
    strategy->intensity = 0.5f;

    return strategy;
}
