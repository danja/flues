#pragma once

#include <stdint.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CADENCE_MAX_SEGMENTS = 32,
    CADENCE_MAX_CHORD_NOTES = 4,
    CADENCE_MAX_CANDIDATES = 108,
    CADENCE_TIMING_BINS = 8,
    CADENCE_MAX_COMP_HITS = 4,
    CADENCE_PROGRESSION_STATE_VERSION = 1,
    CADENCE_VARIATION_STATE_VERSION = 1
};

#define CADENCE_DEFAULT_KEY 0
#define CADENCE_DEFAULT_SCALE SCALE_NAT_MINOR
#define CADENCE_DEFAULT_CYCLE_BARS 2
#define CADENCE_DEFAULT_GRANULARITY GRANULARITY_HALF_BAR
#define CADENCE_DEFAULT_COMPLEXITY 0.45f
#define CADENCE_DEFAULT_MOVEMENT 0.65f
#define CADENCE_DEFAULT_CHORD_SIZE CHORD_SIZE_TRIADS
#define CADENCE_DEFAULT_NOTE_LENGTH 1.0f
#define CADENCE_DEFAULT_REGISTER REGISTER_MID
#define CADENCE_DEFAULT_SPREAD SPREAD_CLOSE
#define CADENCE_DEFAULT_PASS_INPUT true
#define CADENCE_DEFAULT_OUTPUT_CHANNEL 0
#define CADENCE_DEFAULT_VARY 0.0f
#define CADENCE_DEFAULT_COMP 0.0f

typedef enum {
    PORT_CONTROL = 0,
    PORT_MIDI_IN,
    PORT_MIDI_OUT,
    PORT_KEY,
    PORT_SCALE,
    PORT_CYCLE_BARS,
    PORT_GRANULARITY,
    PORT_COMPLEXITY,
    PORT_MOVEMENT,
    PORT_CHORD_SIZE,
    PORT_NOTE_LENGTH,
    PORT_REGISTER,
    PORT_SPREAD,
    PORT_PASS_INPUT,
    PORT_OUTPUT_CHANNEL,
    PORT_ACTION_LEARN,
    PORT_STATUS_READY,
    PORT_VARY,
    PORT_COMP
} PortIndex;

typedef enum {
    SCALE_CHROMATIC = 0,
    SCALE_MAJOR,
    SCALE_NAT_MINOR,
    SCALE_HARM_MINOR,
    SCALE_PENT_MAJOR,
    SCALE_PENT_MINOR,
    SCALE_BLUES,
    SCALE_DORIAN,
    SCALE_MIXOLYDIAN,
    SCALE_PHRYGIAN,
    SCALE_LOCRIAN,
    SCALE_PHRYGIAN_DOMINANT,
    SCALE_COUNT
} ScaleId;

typedef enum {
    GRANULARITY_BEAT = 0,
    GRANULARITY_HALF_BAR,
    GRANULARITY_BAR
} GranularityMode;

typedef enum {
    CHORD_SIZE_TRIADS = 0,
    CHORD_SIZE_SEVENTHS
} ChordSizeMode;

typedef enum {
    REGISTER_LOW = 0,
    REGISTER_MID,
    REGISTER_HIGH
} RegisterMode;

typedef enum {
    SPREAD_CLOSE = 0,
    SPREAD_OPEN,
    SPREAD_DROP2
} SpreadMode;

typedef enum {
    QUALITY_POWER = 0,
    QUALITY_MAJOR,
    QUALITY_MINOR,
    QUALITY_SUS2,
    QUALITY_SUS4,
    QUALITY_DIM,
    QUALITY_DOM7,
    QUALITY_MAJ7,
    QUALITY_MIN7
} QualityId;

typedef struct {
    int key;
    int scale;
    int cycle_bars;
    int granularity;
    float complexity;
    float movement;
    int chord_size;
    float note_length;
    int reg;
    int spread;
    bool pass_input;
    int output_channel;
    int action_learn;
    float vary;
    float comp;
} ControlSnapshot;

typedef struct {
    double duration[12];
    double onset[12];
    double timing_bins[CADENCE_TIMING_BINS];
    double onset_total;
} SegmentCapture;

typedef struct {
    bool valid;
    uint8_t root_pc;
    uint8_t quality;
    uint8_t note_count;
    uint8_t velocity;
    uint8_t notes[CADENCE_MAX_CHORD_NOTES];
} ChordSlot;

typedef struct {
    int32_t valid;
    int32_t root_pc;
    int32_t quality;
    int32_t note_count;
    int32_t velocity;
    int32_t notes[CADENCE_MAX_CHORD_NOTES];
} SavedChordSlot;

typedef struct {
    int32_t version;
    int32_t segment_count;
    int32_t ready;
    SavedChordSlot slots[CADENCE_MAX_SEGMENTS];
} ProgressionStateBlob;

typedef struct {
    int32_t version;
    int64_t completed_cycles;
    int64_t last_mutation_cycle;
    int32_t mutation_serial;
} VariationStateBlob;

static inline ControlSnapshot cadence_default_controls(void) {
    ControlSnapshot controls = {0};
    controls.key = CADENCE_DEFAULT_KEY;
    controls.scale = CADENCE_DEFAULT_SCALE;
    controls.cycle_bars = CADENCE_DEFAULT_CYCLE_BARS;
    controls.granularity = CADENCE_DEFAULT_GRANULARITY;
    controls.complexity = CADENCE_DEFAULT_COMPLEXITY;
    controls.movement = CADENCE_DEFAULT_MOVEMENT;
    controls.chord_size = CADENCE_DEFAULT_CHORD_SIZE;
    controls.note_length = CADENCE_DEFAULT_NOTE_LENGTH;
    controls.reg = CADENCE_DEFAULT_REGISTER;
    controls.spread = CADENCE_DEFAULT_SPREAD;
    controls.pass_input = CADENCE_DEFAULT_PASS_INPUT;
    controls.output_channel = CADENCE_DEFAULT_OUTPUT_CHANNEL;
    controls.action_learn = 0;
    controls.vary = CADENCE_DEFAULT_VARY;
    controls.comp = CADENCE_DEFAULT_COMP;
    return controls;
}

static inline VariationStateBlob cadence_default_variation_state(void) {
    VariationStateBlob variation = {0};
    variation.version = CADENCE_VARIATION_STATE_VERSION;
    return variation;
}

#ifdef __cplusplus
}
#endif
