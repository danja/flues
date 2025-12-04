// brass_strategy.c
// Brass interface: Asymmetric lip model with different positive/negative slopes
// Translated from reference/modules/interface/strategies/BrassStrategy.js

#include "interface_strategy.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <math.h>

// BrassStrategy is stateless, so no impl_data needed

static float brass_process(InterfaceStrategy* self, float input) {
    const float drive = 1.5f + self->intensity * 5.0f;
    float shaped;

    if (input >= 0.0f) {
        const float lifted = input * drive + (0.2f + self->intensity * 0.35f);
        shaped = fast_tanh(fmaxf(lifted, 0.0f));
    } else {
        const float compressed = -input * (drive * (0.4f + self->intensity * 0.4f));
        shaped = -powf(fminf(compressed, 1.5f), 1.3f) * (0.35f + (1.0f - self->intensity) * 0.25f);
    }

    const float buzz = fast_tanh(shaped * (1.2f + self->intensity * 1.5f));
    return fmaxf(-1.0f, fminf(1.0f, buzz + self->intensity * 0.05f));
}

static void brass_reset(InterfaceStrategy* self) {
    // Stateless, nothing to reset
    (void)self;
}

static void brass_set_intensity(InterfaceStrategy* self, float intensity) {
    self->intensity = intensity;
}

static void brass_set_gate(InterfaceStrategy* self, bool gate) {
    // No gate behavior for brass
    (void)self;
    (void)gate;
}

static void brass_destroy(InterfaceStrategy* self) {
    if (self) {
        free(self);
    }
}

static InterfaceStrategyVTable brass_vtable = {
    .process = brass_process,
    .reset = brass_reset,
    .set_intensity = brass_set_intensity,
    .set_gate = brass_set_gate,
    .destroy = brass_destroy
};

InterfaceStrategy* brass_strategy_create(float sample_rate) {
    (void)sample_rate;  // Unused
    InterfaceStrategy* strategy = (InterfaceStrategy*)calloc(1, sizeof(InterfaceStrategy));
    if (!strategy) return NULL;

    strategy->vtable = &brass_vtable;
    strategy->impl_data = NULL;  // Stateless
    strategy->intensity = 0.5f;

    return strategy;
}
