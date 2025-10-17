// dsp_modules.h
// DSP module interfaces for physical modeling synthesizer
// Translated from experiments/pm-synth/src/audio/modules

#ifndef DSP_MODULES_H
#define DSP_MODULES_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Sources Module
// ============================================================================

typedef struct {
    float sample_rate;
    float dc_level;
    float noise_level;
    float tone_level;
    float tone_phase;
    float tone_frequency;
} SourcesModule;

SourcesModule* sources_create(float sample_rate);
void sources_destroy(SourcesModule* sources);
float sources_process(SourcesModule* sources);
void sources_set_dc_level(SourcesModule* sources, float level);
void sources_set_noise_level(SourcesModule* sources, float level);
void sources_set_tone_level(SourcesModule* sources, float level);
void sources_set_tone_frequency(SourcesModule* sources, float frequency);

// ============================================================================
// Envelope Module
// ============================================================================

typedef struct {
    float sample_rate;
    float attack_time;
    float release_time;
    float envelope;
    bool gate;
    bool previous_gate;
} EnvelopeModule;

EnvelopeModule* envelope_create(float sample_rate);
void envelope_destroy(EnvelopeModule* env);
float envelope_process(EnvelopeModule* env, float input);
void envelope_set_gate(EnvelopeModule* env, bool gate);
void envelope_set_attack(EnvelopeModule* env, float attack);
void envelope_set_release(EnvelopeModule* env, float release);

// ============================================================================
// Interface Module (Strategy Pattern)
// ============================================================================

typedef struct InterfaceStrategy InterfaceStrategy;

typedef struct {
    float sample_rate;
    int current_type;
    InterfaceStrategy* strategy;
} InterfaceModule;

InterfaceModule* interface_create(float sample_rate);
void interface_destroy(InterfaceModule* iface);
float interface_process(InterfaceModule* iface, float input);
void interface_set_type(InterfaceModule* iface, int type);
void interface_set_intensity(InterfaceModule* iface, float intensity);
void interface_set_gate(InterfaceModule* iface, bool gate);

// ============================================================================
// Delay Lines Module
// ============================================================================

typedef struct {
    float sample_rate;
    float* buffer1;
    float* buffer2;
    int buffer_size;
    float write_pos1;
    float write_pos2;
    float base_delay_samples;
    float tuning_offset;
    float ratio;
} DelayLinesModule;

DelayLinesModule* delay_lines_create(float sample_rate);
void delay_lines_destroy(DelayLinesModule* delays);
void delay_lines_process(DelayLinesModule* delays, float input, float* out1, float* out2);
void delay_lines_set_frequency(DelayLinesModule* delays, float frequency);
void delay_lines_set_tuning(DelayLinesModule* delays, float tuning);
void delay_lines_set_ratio(DelayLinesModule* delays, float ratio);

// ============================================================================
// Feedback Module
// ============================================================================

typedef struct {
    float delay1_amount;
    float delay2_amount;
    float filter_amount;
} FeedbackModule;

FeedbackModule* feedback_create(void);
void feedback_destroy(FeedbackModule* fb);
float feedback_process(FeedbackModule* fb, float delay1, float delay2, float filter);
void feedback_set_delay1(FeedbackModule* fb, float amount);
void feedback_set_delay2(FeedbackModule* fb, float amount);
void feedback_set_filter(FeedbackModule* fb, float amount);

// ============================================================================
// Filter Module (State Variable Filter)
// ============================================================================

typedef struct {
    float sample_rate;
    float frequency;
    float q;
    float shape;
    float low;
    float band;
    float high;
} FilterModule;

FilterModule* filter_create(float sample_rate);
void filter_destroy(FilterModule* filter);
float filter_process(FilterModule* filter, float input);
void filter_set_frequency(FilterModule* filter, float frequency);
void filter_set_q(FilterModule* filter, float q);
void filter_set_shape(FilterModule* filter, float shape);

// ============================================================================
// Modulation Module (LFO)
// ============================================================================

typedef struct {
    float sample_rate;
    float frequency;
    float depth;
    float phase;
} ModulationModule;

ModulationModule* modulation_create(float sample_rate);
void modulation_destroy(ModulationModule* mod);
float modulation_process(ModulationModule* mod, float input);
void modulation_set_frequency(ModulationModule* mod, float frequency);
void modulation_set_depth(ModulationModule* mod, float depth);

// ============================================================================
// Reverb Module (Schroeder)
// ============================================================================

typedef struct {
    float sample_rate;
    float size;
    float wet_level;
    // Delay line buffers
    float* comb1;
    float* comb2;
    float* comb3;
    float* comb4;
    float* allpass1;
    float* allpass2;
    int comb_size1, comb_size2, comb_size3, comb_size4;
    int allpass_size1, allpass_size2;
    int pos1, pos2, pos3, pos4, ap_pos1, ap_pos2;
} ReverbModule;

ReverbModule* reverb_create(float sample_rate);
void reverb_destroy(ReverbModule* reverb);
float reverb_process(ReverbModule* reverb, float input);
void reverb_set_size(ReverbModule* reverb, float size);
void reverb_set_level(ReverbModule* reverb, float level);

#endif // DSP_MODULES_H
