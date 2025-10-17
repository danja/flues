// interface_strategy.c
// Base strategy implementation and factory
// Translated from experiments/pm-synth/src/audio/modules/interface

#include "interface_strategy.h"
#include <stdlib.h>

// Factory function - creates appropriate strategy based on type
InterfaceStrategy* interface_strategy_create(int type, float sample_rate) {
    switch (type) {
        case 0: return pluck_strategy_create(sample_rate);
        case 1: return hit_strategy_create(sample_rate);
        case 2: return reed_strategy_create(sample_rate);
        case 3: return flute_strategy_create(sample_rate);
        case 4: return brass_strategy_create(sample_rate);
        case 5: return bow_strategy_create(sample_rate);
        case 6: return bell_strategy_create(sample_rate);
        case 7: return drum_strategy_create(sample_rate);
        case 8: return crystal_strategy_create(sample_rate);
        case 9: return vapor_strategy_create(sample_rate);
        case 10: return quantum_strategy_create(sample_rate);
        case 11: return plasma_strategy_create(sample_rate);
        default: return reed_strategy_create(sample_rate); // Fallback
    }
}

// Base interface functions that call through vtable
void interface_strategy_destroy(InterfaceStrategy* strategy) {
    if (strategy && strategy->vtable && strategy->vtable->destroy) {
        strategy->vtable->destroy(strategy);
    }
}

float interface_strategy_process(InterfaceStrategy* strategy, float input) {
    if (strategy && strategy->vtable && strategy->vtable->process) {
        return strategy->vtable->process(strategy, input);
    }
    return input;
}

void interface_strategy_reset(InterfaceStrategy* strategy) {
    if (strategy && strategy->vtable && strategy->vtable->reset) {
        strategy->vtable->reset(strategy);
    }
}

void interface_strategy_set_intensity(InterfaceStrategy* strategy, float intensity) {
    if (strategy && strategy->vtable && strategy->vtable->set_intensity) {
        strategy->vtable->set_intensity(strategy, intensity);
    }
}

void interface_strategy_set_gate(InterfaceStrategy* strategy, bool gate) {
    if (strategy && strategy->vtable && strategy->vtable->set_gate) {
        strategy->vtable->set_gate(strategy, gate);
    }
}
