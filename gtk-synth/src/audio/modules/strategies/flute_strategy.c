// flute_strategy.c
// Flute interface: Soft symmetric nonlinearity with breath noise
// Translated from reference/modules/interface/strategies/FluteStrategy.js

#include "interface_strategy.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <math.h>

// FluteStrategy is stateless, so no impl_data needed

static float flute_process(InterfaceStrategy* self, float input) {
    const float softness = 0.45f + self->intensity * 0.4f;
    const float breath = white_noise() * self->intensity * 0.04f;
    const float mixed = (input + breath) * softness;
    const float shaped = mixed - (mixed * mixed * mixed) * 0.35f;
    return fmaxf(-0.49f, fminf(0.49f, shaped));
}

static void flute_reset(InterfaceStrategy* self) {
    // Stateless, nothing to reset
    (void)self;
}

static void flute_set_intensity(InterfaceStrategy* self, float intensity) {
    self->intensity = intensity;
}

static void flute_set_gate(InterfaceStrategy* self, bool gate) {
    // No gate behavior for flute
    (void)self;
    (void)gate;
}

static void flute_destroy(InterfaceStrategy* self) {
    if (self) {
        free(self);
    }
}

static InterfaceStrategyVTable flute_vtable = {
    .process = flute_process,
    .reset = flute_reset,
    .set_intensity = flute_set_intensity,
    .set_gate = flute_set_gate,
    .destroy = flute_destroy
};

InterfaceStrategy* flute_strategy_create(float sample_rate) {
    (void)sample_rate;  // Unused
    InterfaceStrategy* strategy = (InterfaceStrategy*)calloc(1, sizeof(InterfaceStrategy));
    if (!strategy) return NULL;

    strategy->vtable = &flute_vtable;
    strategy->impl_data = NULL;  // Stateless
    strategy->intensity = 0.5f;

    return strategy;
}
