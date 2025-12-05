// brass_strategy.c
// Brass interface: Asymmetric lip model with different positive/negative slopes
// Translated from reference/modules/interface/strategies/BrassStrategy.js

#include "interface_strategy.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <math.h>

// BrassStrategy is stateless, so no impl_data needed

static float brass_process(InterfaceStrategy* self, float input) {
    // Reduced drive to prevent excessive output
    const float drive = 0.8f + self->intensity * 2.0f;
    float shaped;

    if (input >= 0.0f) {
        // Removed DC lift - asymmetry from processing difference only
        const float driven = input * drive;
        shaped = fast_tanh(driven);
    } else {
        // Asymmetric negative processing (compressed, shaped)
        const float compressed = -input * (drive * (0.4f + self->intensity * 0.4f));
        shaped = -powf(fminf(compressed, 1.5f), 1.3f) * (0.35f + (1.0f - self->intensity) * 0.25f);
    }

    // Reduced final drive and removed DC offset addition
    const float buzz = fast_tanh(shaped * (0.8f + self->intensity * 0.8f));
    // Apply DC correction to compensate for asymmetric processing
    const float output = buzz * 0.35f - 0.02f;  // Small DC offset from asymmetry
    return fmaxf(-1.0f, fminf(1.0f, output));
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
