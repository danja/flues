// all_strategies_stub.c
// Stub implementations of remaining interface strategies
// TODO: Translate full implementations from experiments/pm-synth/src/audio/modules/interface/strategies/

#include "interface_strategy.h"
#include "dsp_utils.h"
#include <stdlib.h>

// Generic stub vtable and implementation
typedef struct { float state; } StubImpl;

static float stub_process(InterfaceStrategy* self, float input) {
    return fast_tanh(input * (0.5f + self->intensity * 0.5f));
}

static void stub_reset(InterfaceStrategy* self) {
    StubImpl* impl = (StubImpl*)self->impl_data;
    impl->state = 0.0f;
}

static void stub_set_intensity(InterfaceStrategy* self, float intensity) {
    self->intensity = intensity;
}

static void stub_set_gate(InterfaceStrategy* self, bool gate) {
    self->previous_gate = self->gate;
    self->gate = gate;
}

static void stub_destroy(InterfaceStrategy* self) {
    if (self->impl_data) free(self->impl_data);
    free(self);
}

static const InterfaceStrategyVTable stub_vtable = {
    .process = stub_process, .reset = stub_reset,
    .set_intensity = stub_set_intensity, .set_gate = stub_set_gate,
    .destroy = stub_destroy
};

static InterfaceStrategy* create_stub_strategy(float sample_rate) {
    InterfaceStrategy* s = (InterfaceStrategy*)calloc(1, sizeof(InterfaceStrategy));
    StubImpl* impl = (StubImpl*)calloc(1, sizeof(StubImpl));
    s->vtable = &stub_vtable;
    s->sample_rate = sample_rate;
    s->intensity = 0.5f;
    s->impl_data = impl;
    return s;
}

// Stub creators for all strategies (except Reed which has full implementation)
InterfaceStrategy* pluck_strategy_create(float sample_rate) { return create_stub_strategy(sample_rate); }
InterfaceStrategy* hit_strategy_create(float sample_rate) { return create_stub_strategy(sample_rate); }
InterfaceStrategy* flute_strategy_create(float sample_rate) { return create_stub_strategy(sample_rate); }
InterfaceStrategy* brass_strategy_create(float sample_rate) { return create_stub_strategy(sample_rate); }
InterfaceStrategy* bow_strategy_create(float sample_rate) { return create_stub_strategy(sample_rate); }
InterfaceStrategy* bell_strategy_create(float sample_rate) { return create_stub_strategy(sample_rate); }
InterfaceStrategy* drum_strategy_create(float sample_rate) { return create_stub_strategy(sample_rate); }
InterfaceStrategy* crystal_strategy_create(float sample_rate) { return create_stub_strategy(sample_rate); }
InterfaceStrategy* vapor_strategy_create(float sample_rate) { return create_stub_strategy(sample_rate); }
InterfaceStrategy* quantum_strategy_create(float sample_rate) { return create_stub_strategy(sample_rate); }
InterfaceStrategy* plasma_strategy_create(float sample_rate) { return create_stub_strategy(sample_rate); }
