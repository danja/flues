// interface_strategy.h
// Interface strategy pattern for physical modeling behaviors
// Translated from experiments/pm-synth/src/audio/modules/interface

#ifndef INTERFACE_STRATEGY_H
#define INTERFACE_STRATEGY_H

#include <stdbool.h>

// Forward declaration
typedef struct InterfaceStrategy InterfaceStrategy;

// Virtual function table for strategy pattern
typedef struct {
    float (*process)(InterfaceStrategy* self, float input);
    void (*reset)(InterfaceStrategy* self);
    void (*set_intensity)(InterfaceStrategy* self, float intensity);
    void (*set_gate)(InterfaceStrategy* self, bool gate);
    void (*destroy)(InterfaceStrategy* self);
} InterfaceStrategyVTable;

// Base strategy structure
struct InterfaceStrategy {
    const InterfaceStrategyVTable* vtable;
    float sample_rate;
    float intensity;
    bool gate;
    bool previous_gate;
    void* impl_data; // Points to concrete implementation data
};

// Factory function
InterfaceStrategy* interface_strategy_create(int type, float sample_rate);

// Base functions
void interface_strategy_destroy(InterfaceStrategy* strategy);
float interface_strategy_process(InterfaceStrategy* strategy, float input);
void interface_strategy_reset(InterfaceStrategy* strategy);
void interface_strategy_set_intensity(InterfaceStrategy* strategy, float intensity);
void interface_strategy_set_gate(InterfaceStrategy* strategy, bool gate);

// Concrete strategy creators (internal use)
InterfaceStrategy* pluck_strategy_create(float sample_rate);
InterfaceStrategy* hit_strategy_create(float sample_rate);
InterfaceStrategy* reed_strategy_create(float sample_rate);
InterfaceStrategy* flute_strategy_create(float sample_rate);
InterfaceStrategy* brass_strategy_create(float sample_rate);
InterfaceStrategy* bow_strategy_create(float sample_rate);
InterfaceStrategy* bell_strategy_create(float sample_rate);
InterfaceStrategy* drum_strategy_create(float sample_rate);
InterfaceStrategy* crystal_strategy_create(float sample_rate);
InterfaceStrategy* vapor_strategy_create(float sample_rate);
InterfaceStrategy* quantum_strategy_create(float sample_rate);
InterfaceStrategy* plasma_strategy_create(float sample_rate);

#endif // INTERFACE_STRATEGY_H
