#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    BASSGEN_MIN_LENGTH_BEATS = 8,
    BASSGEN_MAX_LENGTH_BEATS = 32,
    BASSGEN_MAX_PATTERN_STEPS = 192,
    BASSGEN_MAX_EVENTS = 192,
    BASSGEN_SAFETY_GAP_SAMPLES = 1,
    BASSGEN_PATTERN_STATE_VERSION = 1,
    BASSGEN_VARIATION_STATE_VERSION = 1
};

#define BASSGEN_DEFAULT_ROOT_NOTE 36
#define BASSGEN_DEFAULT_SCALE 0
#define BASSGEN_DEFAULT_GENRE 0
#define BASSGEN_DEFAULT_CHANNEL 1
#define BASSGEN_DEFAULT_LENGTH_BEATS 16
#define BASSGEN_DEFAULT_SUBDIVISION 1
#define BASSGEN_DEFAULT_DENSITY 0.45f
#define BASSGEN_DEFAULT_REGISTER 1
#define BASSGEN_DEFAULT_HOLD 0.35f
#define BASSGEN_DEFAULT_ACCENT 0.45f
#define BASSGEN_DEFAULT_VARY 0.0f
#define BASSGEN_DEFAULT_SEED 1u

typedef enum {
    PORT_CONTROL = 0,
    PORT_MIDI_OUT,
    PORT_ROOT_NOTE,
    PORT_SCALE,
    PORT_GENRE,
    PORT_CHANNEL,
    PORT_LENGTH_BEATS,
    PORT_SUBDIVISION,
    PORT_DENSITY,
    PORT_REGISTER,
    PORT_HOLD,
    PORT_ACCENT,
    PORT_SEED,
    PORT_ACTION_NEW,
    PORT_ACTION_NOTES,
    PORT_ACTION_RHYTHM,
    PORT_VARY,
    PORT_TOTAL_COUNT
} PortIndex;

typedef enum {
    SCALE_MINOR = 0,
    SCALE_MAJOR,
    SCALE_DORIAN,
    SCALE_PHRYGIAN,
    SCALE_PENT_MINOR,
    SCALE_BLUES,
    SCALE_MIXOLYDIAN,
    SCALE_HARMONIC_MINOR,
    SCALE_PENT_MAJOR,
    SCALE_LOCRIAN,
    SCALE_PHRYGIAN_DOMINANT,
    SCALE_COUNT
} ScaleId;

typedef enum {
    GENRE_TECHNO = 0,
    GENRE_ACID,
    GENRE_HOUSE,
    GENRE_ELECTRO,
    GENRE_DUB,
    GENRE_AMBIENT,
    GENRE_FUNK,
    GENRE_SABBATH,
    GENRE_COUNT
} GenreId;

typedef enum {
    SUBDIV_8TH = 0,
    SUBDIV_16TH,
    SUBDIV_16T,
    SUBDIV_COUNT
} SubdivisionId;

typedef struct {
    int root_note;
    int scale;
    int genre;
    int channel;
    int length_beats;
    int subdivision;
    float density;
    int reg;
    float hold;
    float accent;
    float vary;
    uint32_t seed;
    int action_new;
    int action_notes;
    int action_rhythm;
} ControlSnapshot;

typedef struct {
    int32_t start_step;
    int32_t duration_steps;
    int32_t note;
    int32_t velocity;
} NoteEventData;

typedef struct {
    int32_t version;
    int32_t pattern_steps;
    int32_t steps_per_beat;
    int32_t event_count;
    int32_t generation_serial;
    NoteEventData events[BASSGEN_MAX_EVENTS];
} PatternStateBlob;

typedef struct {
    int32_t version;
    int64_t completed_loops;
    int64_t last_mutation_loop;
} VariationStateBlob;

static inline ControlSnapshot bassgen_default_controls(void) {
    ControlSnapshot controls = {0};
    controls.root_note = BASSGEN_DEFAULT_ROOT_NOTE;
    controls.scale = BASSGEN_DEFAULT_SCALE;
    controls.genre = BASSGEN_DEFAULT_GENRE;
    controls.channel = BASSGEN_DEFAULT_CHANNEL;
    controls.length_beats = BASSGEN_DEFAULT_LENGTH_BEATS;
    controls.subdivision = BASSGEN_DEFAULT_SUBDIVISION;
    controls.density = BASSGEN_DEFAULT_DENSITY;
    controls.reg = BASSGEN_DEFAULT_REGISTER;
    controls.hold = BASSGEN_DEFAULT_HOLD;
    controls.accent = BASSGEN_DEFAULT_ACCENT;
    controls.vary = BASSGEN_DEFAULT_VARY;
    controls.seed = BASSGEN_DEFAULT_SEED;
    return controls;
}

static inline PatternStateBlob bassgen_default_pattern_state(void) {
    PatternStateBlob pattern = {0};
    pattern.version = BASSGEN_PATTERN_STATE_VERSION;
    pattern.steps_per_beat = 4;
    return pattern;
}

static inline VariationStateBlob bassgen_default_variation_state(void) {
    VariationStateBlob variation = {0};
    variation.version = BASSGEN_VARIATION_STATE_VERSION;
    return variation;
}

#ifdef __cplusplus
}
#endif
