// hit_strategy.c
// Hit interface: Sharp waveshaper with adjustable hardness
// Translated from reference/modules/interface/strategies/HitStrategy.js

#include "interface_strategy.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <math.h>

// HitStrategy is stateless, so no impl_data needed

static float hit_process(InterfaceStrategy* self, float input) {
    const float drive = 2.0f + self->intensity * 8.0f;
    const float folded = sine_fold(input, drive);
    const float hardness = 0.35f + self->intensity * 0.55f;
    const float shaped = copysignf(powf(fabsf(folded), hardness), folded);
    return fmaxf(-1.0f, fminf(1.0f, shaped));
}

static void hit_reset(InterfaceStrategy* self) {
    // Stateless, nothing to reset
    (void)self;
}

static void hit_set_intensity(InterfaceStrategy* self, float intensity) {
    self->intensity = intensity;
}

static void hit_set_gate(InterfaceStrategy* self, bool gate) {
    // No gate behavior for hit
    (void)self;
    (void)gate;
}

static void hit_destroy(InterfaceStrategy* self) {
    if (self) {
        free(self);
    }
}

static InterfaceStrategyVTable hit_vtable = {
    .process = hit_process,
    .reset = hit_reset,
    .set_intensity = hit_set_intensity,
    .set_gate = hit_set_gate,
    .destroy = hit_destroy
};

InterfaceStrategy* hit_strategy_create(float sample_rate) {
    (void)sample_rate;  // Unused
    InterfaceStrategy* strategy = (InterfaceStrategy*)calloc(1, sizeof(InterfaceStrategy));
    if (!strategy) return NULL;

    strategy->vtable = &hit_vtable;
    strategy->impl_data = NULL;  // Stateless
    strategy->intensity = 0.5f;

    return strategy;
}
