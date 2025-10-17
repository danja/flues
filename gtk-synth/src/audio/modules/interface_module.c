// interface_module.c
// Interface module using Strategy pattern
// Translated from experiments/pm-synth/src/audio/modules/InterfaceModule.js

#include "dsp_modules.h"
#include "interface_strategy.h"
#include <stdlib.h>

InterfaceModule* interface_create(float sample_rate) {
    InterfaceModule* iface = (InterfaceModule*)calloc(1, sizeof(InterfaceModule));
    if (!iface) return NULL;

    iface->sample_rate = sample_rate;
    iface->current_type = 2; // Reed default
    iface->strategy = interface_strategy_create(iface->current_type, sample_rate);

    return iface;
}

void interface_destroy(InterfaceModule* iface) {
    if (!iface) return;
    if (iface->strategy) {
        interface_strategy_destroy(iface->strategy);
    }
    free(iface);
}

float interface_process(InterfaceModule* iface, float input) {
    return interface_strategy_process(iface->strategy, input);
}

void interface_set_type(InterfaceModule* iface, int type) {
    if (type != iface->current_type) {
        float old_intensity = iface->strategy->intensity;
        bool old_gate = iface->strategy->gate;

        interface_strategy_destroy(iface->strategy);
        iface->strategy = interface_strategy_create(type, iface->sample_rate);
        iface->current_type = type;

        // Restore parameters
        interface_strategy_set_intensity(iface->strategy, old_intensity);
        interface_strategy_set_gate(iface->strategy, old_gate);
    }
}

void interface_set_intensity(InterfaceModule* iface, float intensity) {
    interface_strategy_set_intensity(iface->strategy, intensity);
}

void interface_set_gate(InterfaceModule* iface, bool gate) {
    interface_strategy_set_gate(iface->strategy, gate);
}
