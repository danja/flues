// bow_strategy.c
// Bow interface: Stick-slip friction with controllable bite and noise
// Translated from reference/modules/interface/strategies/BowStrategy.js

#include "interface_strategy.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <math.h>

typedef struct {
    float bow_state;
} BowImpl;

static float bow_process(InterfaceStrategy* self, float input) {
    BowImpl* impl = (BowImpl*)self->impl_data;

    const float bow_velocity = self->intensity * 0.9f + 0.2f;
    const float slip = input - impl->bow_state;
    const float friction = fast_tanh(slip * (6.0f + self->intensity * 12.0f));
    const float grit = white_noise() * self->intensity * 0.012f;
    const float output = friction * (0.55f + self->intensity * 0.35f) + slip * 0.25f + grit;
    const float stick = 0.8f - self->intensity * 0.25f;
    impl->bow_state = impl->bow_state * stick + (input + friction * bow_velocity * 0.05f) * (1.0f - stick);

    return fmaxf(-1.0f, fminf(1.0f, output));
}

static void bow_reset(InterfaceStrategy* self) {
    BowImpl* impl = (BowImpl*)self->impl_data;
    impl->bow_state = 0.0f;
}

static void bow_set_intensity(InterfaceStrategy* self, float intensity) {
    self->intensity = intensity;
}

static void bow_set_gate(InterfaceStrategy* self, bool gate) {
    if (gate) {
        bow_reset(self);
    }
}

static void bow_destroy(InterfaceStrategy* self) {
    if (self) {
        free(self->impl_data);
        free(self);
    }
}

static InterfaceStrategyVTable bow_vtable = {
    .process = bow_process,
    .reset = bow_reset,
    .set_intensity = bow_set_intensity,
    .set_gate = bow_set_gate,
    .destroy = bow_destroy
};

InterfaceStrategy* bow_strategy_create(float sample_rate) {
    (void)sample_rate;  // Unused
    InterfaceStrategy* strategy = (InterfaceStrategy*)calloc(1, sizeof(InterfaceStrategy));
    if (!strategy) return NULL;

    BowImpl* impl = (BowImpl*)calloc(1, sizeof(BowImpl));
    if (!impl) {
        free(strategy);
        return NULL;
    }

    impl->bow_state = 0.0f;

    strategy->vtable = &bow_vtable;
    strategy->impl_data = impl;
    strategy->intensity = 0.5f;

    return strategy;
}
