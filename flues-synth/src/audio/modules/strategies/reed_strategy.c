// reed_strategy.c
// Reed interface strategy - nonlinear reed model
// Translated from experiments/pm-synth/src/audio/modules/interface/strategies/ReedStrategy.js

#include "interface_strategy.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <math.h>

// Reed implementation data
typedef struct {
    float reed_state;
} ReedImpl;

// Forward declarations
static float reed_process(InterfaceStrategy* self, float input);
static void reed_reset(InterfaceStrategy* self);
static void reed_set_intensity(InterfaceStrategy* self, float intensity);
static void reed_set_gate(InterfaceStrategy* self, bool gate);
static void reed_destroy(InterfaceStrategy* self);

// VTable for reed strategy
static const InterfaceStrategyVTable reed_vtable = {
    .process = reed_process,
    .reset = reed_reset,
    .set_intensity = reed_set_intensity,
    .set_gate = reed_set_gate,
    .destroy = reed_destroy
};

InterfaceStrategy* reed_strategy_create(float sample_rate) {
    InterfaceStrategy* strategy = (InterfaceStrategy*)calloc(1, sizeof(InterfaceStrategy));
    if (!strategy) return NULL;

    ReedImpl* impl = (ReedImpl*)calloc(1, sizeof(ReedImpl));
    if (!impl) {
        free(strategy);
        return NULL;
    }

    strategy->vtable = &reed_vtable;
    strategy->sample_rate = sample_rate;
    strategy->intensity = 0.5f;
    strategy->gate = false;
    strategy->previous_gate = false;
    strategy->impl_data = impl;

    impl->reed_state = 0.0f;

    return strategy;
}

static float reed_process(InterfaceStrategy* self, float input) {
    ReedImpl* impl = (ReedImpl*)self->impl_data;

    // Reed model: pressure difference causes nonlinear flow
    // input = pressure (from excitation + feedback)
    // reed_state = reed opening (filtered response)

    // Calculate pressure differential
    const float pressure = input * 0.5f;

    // Reed opening follows pressure with smoothing
    const float reed_coeff = 0.95f;
    impl->reed_state = impl->reed_state * reed_coeff +
                       pressure * (1.0f - reed_coeff);

    // Flow through reed (nonlinear)
    const float opening = 1.0f - self->intensity * 0.7f;
    const float flow_area = opening + impl->reed_state;

    // Nonlinear flow (Bernoulli's principle approximation)
    float flow;
    if (flow_area > 0.0f) {
        flow = pressure * sqrtf(fabsf(flow_area));
    } else {
        flow = 0.0f; // Reed closed
    }

    // Apply waveshaping for richer harmonics
    const float shaped = fast_tanh(flow * (1.0f + self->intensity));

    return shaped;
}

static void reed_reset(InterfaceStrategy* self) {
    ReedImpl* impl = (ReedImpl*)self->impl_data;
    impl->reed_state = 0.0f;
    self->previous_gate = false;
}

static void reed_set_intensity(InterfaceStrategy* self, float intensity) {
    self->intensity = intensity;
}

static void reed_set_gate(InterfaceStrategy* self, bool gate) {
    self->previous_gate = self->gate;
    self->gate = gate;
}

static void reed_destroy(InterfaceStrategy* self) {
    if (self->impl_data) {
        free(self->impl_data);
    }
    free(self);
}
