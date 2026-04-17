#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DRUMGEN_MIN_BARS = 1,
    DRUMGEN_MAX_BARS = 4,
    DRUMGEN_LANE_COUNT = 11,
    DRUMGEN_MAX_PATTERN_STEPS = 64,
    DRUMGEN_MAX_PENDING_NOTE_OFFS = 96,
    DRUMGEN_SAFETY_GAP_SAMPLES = 1,
    DRUMGEN_PATTERN_STATE_VERSION = 1,
    DRUMGEN_VARIATION_STATE_VERSION = 1
};

#define DRUMGEN_DEFAULT_GENRE 0
#define DRUMGEN_DEFAULT_CHANNEL 10
#define DRUMGEN_DEFAULT_KIT_MAP 0
#define DRUMGEN_DEFAULT_BARS 2
#define DRUMGEN_DEFAULT_RESOLUTION 1
#define DRUMGEN_DEFAULT_DENSITY 0.58f
#define DRUMGEN_DEFAULT_VARIATION 0.35f
#define DRUMGEN_DEFAULT_FILL 0.30f
#define DRUMGEN_DEFAULT_VARY 0.0f
#define DRUMGEN_DEFAULT_SEED 1u
#define DRUMGEN_DEFAULT_KICK_AMT 0.78f
#define DRUMGEN_DEFAULT_BACKBEAT_AMT 0.76f
#define DRUMGEN_DEFAULT_HAT_AMT 0.82f
#define DRUMGEN_DEFAULT_AUX_AMT 0.28f
#define DRUMGEN_DEFAULT_TOM_AMT 0.30f
#define DRUMGEN_DEFAULT_METAL_AMT 0.26f

typedef enum {
    PORT_CONTROL = 0,
    PORT_MIDI_OUT,
    PORT_GENRE,
    PORT_CHANNEL,
    PORT_KIT_MAP,
    PORT_BARS,
    PORT_RESOLUTION,
    PORT_DENSITY,
    PORT_VARIATION,
    PORT_FILL,
    PORT_SEED,
    PORT_KICK_AMT,
    PORT_BACKBEAT_AMT,
    PORT_HAT_AMT,
    PORT_AUX_AMT,
    PORT_ACTION_NEW,
    PORT_ACTION_MUTATE,
    PORT_ACTION_FILL,
    PORT_TOM_AMT,
    PORT_METAL_AMT,
    PORT_VARY,
    PORT_TOTAL_COUNT
} PortIndex;

typedef enum {
    GENRE_ROCK = 0,
    GENRE_DISCO,
    GENRE_SHUFFLE,
    GENRE_ELECTRO,
    GENRE_DUB,
    GENRE_MOTORIK,
    GENRE_BOSSA,
    GENRE_AFRO,
    GENRE_COUNT
} GenreId;

typedef enum {
    RESOLUTION_8TH = 0,
    RESOLUTION_16TH,
    RESOLUTION_16T,
    RESOLUTION_COUNT
} ResolutionId;

typedef enum {
    KIT_MAP_FLUES_DRUMKIT = 0,
    KIT_MAP_GM,
    KIT_MAP_COUNT
} KitMapId;

typedef enum {
    LANE_KICK = 0,
    LANE_CLAP,
    LANE_SNARE,
    LANE_CRASH,
    LANE_CLOSED_HAT,
    LANE_LOW_TOM,
    LANE_OPEN_HAT,
    LANE_HIGH_TOM,
    LANE_BASH,
    LANE_COWBELL,
    LANE_CLAVE
} LaneId;

typedef enum {
    STEP_FLAG_ACCENT = 1 << 0,
    STEP_FLAG_FILL = 1 << 1
} StepFlags;

typedef struct {
    int genre;
    int channel;
    int kit_map;
    int bars;
    int resolution;
    float density;
    float variation;
    float fill;
    float vary;
    uint32_t seed;
    float kick_amt;
    float backbeat_amt;
    float hat_amt;
    float aux_amt;
    float tom_amt;
    float metal_amt;
    int action_new;
    int action_mutate;
    int action_fill;
} ControlSnapshot;

typedef struct {
    uint8_t velocity;
    uint8_t flags;
} DrumStepCell;

typedef struct {
    int32_t midi_note;
    DrumStepCell steps[DRUMGEN_MAX_PATTERN_STEPS];
} DrumLaneState;

typedef struct {
    int32_t version;
    int32_t bars;
    int32_t steps_per_beat;
    int32_t steps_per_bar;
    int32_t total_steps;
    int32_t generation_serial;
    DrumLaneState lanes[DRUMGEN_LANE_COUNT];
} PatternStateBlob;

typedef struct {
    int32_t version;
    int64_t completed_loops;
    int64_t last_mutation_loop;
} VariationStateBlob;

static inline ControlSnapshot drumgen_default_controls(void) {
    ControlSnapshot controls = {0};
    controls.genre = DRUMGEN_DEFAULT_GENRE;
    controls.channel = DRUMGEN_DEFAULT_CHANNEL;
    controls.kit_map = DRUMGEN_DEFAULT_KIT_MAP;
    controls.bars = DRUMGEN_DEFAULT_BARS;
    controls.resolution = DRUMGEN_DEFAULT_RESOLUTION;
    controls.density = DRUMGEN_DEFAULT_DENSITY;
    controls.variation = DRUMGEN_DEFAULT_VARIATION;
    controls.fill = DRUMGEN_DEFAULT_FILL;
    controls.vary = DRUMGEN_DEFAULT_VARY;
    controls.seed = DRUMGEN_DEFAULT_SEED;
    controls.kick_amt = DRUMGEN_DEFAULT_KICK_AMT;
    controls.backbeat_amt = DRUMGEN_DEFAULT_BACKBEAT_AMT;
    controls.hat_amt = DRUMGEN_DEFAULT_HAT_AMT;
    controls.aux_amt = DRUMGEN_DEFAULT_AUX_AMT;
    controls.tom_amt = DRUMGEN_DEFAULT_TOM_AMT;
    controls.metal_amt = DRUMGEN_DEFAULT_METAL_AMT;
    return controls;
}

static inline PatternStateBlob drumgen_default_pattern_state(void) {
    PatternStateBlob pattern = {0};
    pattern.version = DRUMGEN_PATTERN_STATE_VERSION;
    pattern.bars = DRUMGEN_DEFAULT_BARS;
    pattern.steps_per_beat = 4;
    pattern.steps_per_bar = 16;
    return pattern;
}

static inline VariationStateBlob drumgen_default_variation_state(void) {
    VariationStateBlob variation = {0};
    variation.version = DRUMGEN_VARIATION_STATE_VERSION;
    return variation;
}

#ifdef __cplusplus
}
#endif
