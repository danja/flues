// crystal_strategy.c
// Crystal interface: Idealized inharmonic resonator with cross-coupling
// Translated from reference/modules/interface/strategies/CrystalStrategy.js

#include "interface_strategy.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <math.h>

typedef struct {
    float phase1;
    float phase2;
    float phase3;
} CrystalImpl;

static float crystal_process(InterfaceStrategy* self, float input) {
    CrystalImpl* impl = (CrystalImpl*)self->impl_data;

    // Store input in phase accumulators (acting as delay taps)
    // Each "phase" is actually tracking the signal at different time scales
    impl->phase1 = impl->phase1 * 0.98f + input;
    impl->phase2 = impl->phase2 * 0.95f + input * PHI;
    impl->phase3 = impl->phase3 * 0.92f + input * PHI2;

    // Create inharmonic partials by modulating input with delayed versions
    const float p1 = input * (1.0f + impl->phase1 * 0.3f);
    const float p2 = input * (1.0f + impl->phase2 * 0.3f);
    const float p3 = input * (1.0f + impl->phase3 * 0.3f);

    // Cross-coupling creates sum/difference tones (ring modulation)
    const float cross_coupling = self->intensity * 0.3f;
    const float coupled = (p1 + p2 + p3) * 0.33f +
                         cross_coupling * (p1 * p2 + p2 * p3 + p1 * p3) * 0.1f;

    // Apply nonlinearity for additional harmonics
    const float output = cubic_waveshaper(coupled, self->intensity * 0.2f);

    return fmaxf(-1.0f, fminf(1.0f, output));
}

static void crystal_reset(InterfaceStrategy* self) {
    CrystalImpl* impl = (CrystalImpl*)self->impl_data;
    impl->phase1 = 0.0f;
    impl->phase2 = 0.0f;
    impl->phase3 = 0.0f;
}

static void crystal_set_intensity(InterfaceStrategy* self, float intensity) {
    self->intensity = intensity;
}

static void crystal_set_gate(InterfaceStrategy* self, bool gate) {
    if (gate) {
        crystal_reset(self);
    }
}

static void crystal_destroy(InterfaceStrategy* self) {
    if (self) {
        free(self->impl_data);
        free(self);
    }
}

static InterfaceStrategyVTable crystal_vtable = {
    .process = crystal_process,
    .reset = crystal_reset,
    .set_intensity = crystal_set_intensity,
    .set_gate = crystal_set_gate,
    .destroy = crystal_destroy
};

InterfaceStrategy* crystal_strategy_create(float sample_rate) {
    (void)sample_rate;  // Unused
    InterfaceStrategy* strategy = (InterfaceStrategy*)calloc(1, sizeof(InterfaceStrategy));
    if (!strategy) return NULL;

    CrystalImpl* impl = (CrystalImpl*)calloc(1, sizeof(CrystalImpl));
    if (!impl) {
        free(strategy);
        return NULL;
    }

    impl->phase1 = 0.0f;
    impl->phase2 = 0.0f;
    impl->phase3 = 0.0f;

    strategy->vtable = &crystal_vtable;
    strategy->impl_data = impl;
    strategy->intensity = 0.5f;

    return strategy;
}
