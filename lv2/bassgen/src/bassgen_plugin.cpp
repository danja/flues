#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/state/state.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#define BASSGEN_URI "https://danja.github.io/flues/plugins/bassgen"

static constexpr int kMinLengthBeats = 8;
static constexpr int kMaxLengthBeats = 32;
static constexpr int kMaxPatternSteps = 192;
static constexpr int kMaxEvents = 192;
static constexpr int kSafetyGapSamples = 1;

enum PortIndex {
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
    PORT_TOTAL_COUNT
};

enum ScaleId {
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
};

enum GenreId {
    GENRE_TECHNO = 0,
    GENRE_ACID,
    GENRE_HOUSE,
    GENRE_ELECTRO,
    GENRE_DUB,
    GENRE_AMBIENT,
    GENRE_FUNK,
    GENRE_SABBATH,
    GENRE_COUNT
};

enum SubdivisionId {
    SUBDIV_8TH = 0,
    SUBDIV_16TH,
    SUBDIV_16T,
    SUBDIV_COUNT
};

struct Rng {
    uint32_t state = 0x12345678u;

    void seed(uint32_t seed_value) {
        state = seed_value ? seed_value : 0x12345678u;
    }

    uint32_t next_u32() {
        state = state * 1664525u + 1013904223u;
        return state;
    }

    float next_float() {
        return (float)(next_u32() & 0x00FFFFFFu) / 16777215.0f;
    }

    int next_int(int min_value, int max_value) {
        if (max_value <= min_value) {
            return min_value;
        }
        const uint32_t span = (uint32_t)(max_value - min_value + 1);
        return min_value + (int)(next_u32() % span);
    }
};

struct TimeInfo {
    bool valid = false;
    bool playing = false;
    double bar = 0.0;
    double barBeat = 0.0;
    double beatsPerBar = 4.0;
    double bpm = 120.0;
};

struct ControlSnapshot {
    int root_note = 36;
    int scale = SCALE_MINOR;
    int genre = GENRE_TECHNO;
    int channel = 1;
    int length_beats = 16;
    int subdivision = SUBDIV_16TH;
    float density = 0.45f;
    int reg = 1;
    float hold = 0.35f;
    float accent = 0.45f;
    uint32_t seed = 1;
    int action_new = 0;
    int action_notes = 0;
    int action_rhythm = 0;
};

struct NoteEventData {
    int32_t start_step = 0;
    int32_t duration_steps = 1;
    int32_t note = 36;
    int32_t velocity = 96;
};

struct PatternStateBlob {
    int32_t version = 1;
    int32_t pattern_steps = 0;
    int32_t steps_per_beat = 4;
    int32_t event_count = 0;
    int32_t generation_serial = 0;
    NoteEventData events[kMaxEvents];
};

struct ScaleDef {
    const int* intervals;
    int count;
};

static const int kScaleMinor[] = {0, 2, 3, 5, 7, 8, 10};
static const int kScaleMajor[] = {0, 2, 4, 5, 7, 9, 11};
static const int kScaleDorian[] = {0, 2, 3, 5, 7, 9, 10};
static const int kScalePhrygian[] = {0, 1, 3, 5, 7, 8, 10};
static const int kScalePentMinor[] = {0, 3, 5, 7, 10};
static const int kScaleBlues[] = {0, 3, 5, 6, 7, 10};
static const int kScaleMixolydian[] = {0, 2, 4, 5, 7, 9, 10};
static const int kScaleHarmonicMinor[] = {0, 2, 3, 5, 7, 8, 11};
static const int kScalePentMajor[] = {0, 2, 4, 7, 9};
static const int kScaleLocrian[] = {0, 1, 3, 5, 6, 8, 10};
static const int kScalePhrygianDominant[] = {0, 1, 4, 5, 7, 8, 10};

static const ScaleDef kScales[SCALE_COUNT] = {
    {kScaleMinor, 7},
    {kScaleMajor, 7},
    {kScaleDorian, 7},
    {kScalePhrygian, 7},
    {kScalePentMinor, 5},
    {kScaleBlues, 6},
    {kScaleMixolydian, 7},
    {kScaleHarmonicMinor, 7},
    {kScalePentMajor, 5},
    {kScaleLocrian, 7},
    {kScalePhrygianDominant, 7}
};

struct BassGenURIDs {
    LV2_URID atom_Object = 0;
    LV2_URID atom_Blank = 0;
    LV2_URID atom_Float = 0;
    LV2_URID atom_Double = 0;
    LV2_URID atom_Int = 0;
    LV2_URID atom_Long = 0;
    LV2_URID atom_Chunk = 0;
    LV2_URID atom_Sequence = 0;
    LV2_URID midi_Event = 0;
    LV2_URID time_Position = 0;
    LV2_URID time_speed = 0;
    LV2_URID time_bar = 0;
    LV2_URID time_barBeat = 0;
    LV2_URID time_beatsPerBar = 0;
    LV2_URID time_beatsPerMinute = 0;

    LV2_URID state_controls = 0;
    LV2_URID state_pattern = 0;
};

struct BassGen {
    const LV2_Atom_Sequence* control = nullptr;
    LV2_Atom_Sequence* midi_out = nullptr;

    const float* root_note_port = nullptr;
    const float* scale_port = nullptr;
    const float* genre_port = nullptr;
    const float* channel_port = nullptr;
    const float* length_beats_port = nullptr;
    const float* subdivision_port = nullptr;
    const float* density_port = nullptr;
    const float* register_port = nullptr;
    const float* hold_port = nullptr;
    const float* accent_port = nullptr;
    const float* seed_port = nullptr;
    const float* action_new_port = nullptr;
    const float* action_notes_port = nullptr;
    const float* action_rhythm_port = nullptr;

    LV2_URID_Map* map = nullptr;
    BassGenURIDs urids{};
    double sample_rate = 48000.0;

    ControlSnapshot controls{};
    ControlSnapshot previous_controls{};

    PatternStateBlob pattern{};
    bool pattern_valid = false;

    int active_note = -1;
    int64_t last_transport_step = -1;
    bool was_playing = false;
};

static inline float clampf(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static inline int clampi(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static bool atom_to_double(const LV2_Atom* atom, const BassGenURIDs& urids, double* out) {
    if (!atom || !out) {
        return false;
    }
    if (atom->type == urids.atom_Float) {
        *out = ((const LV2_Atom_Float*)atom)->body;
        return true;
    }
    if (atom->type == urids.atom_Double) {
        *out = ((const LV2_Atom_Double*)atom)->body;
        return true;
    }
    if (atom->type == urids.atom_Int) {
        *out = ((const LV2_Atom_Int*)atom)->body;
        return true;
    }
    if (atom->type == urids.atom_Long) {
        *out = (double)((const LV2_Atom_Long*)atom)->body;
        return true;
    }
    return false;
}

static bool read_time_info(const LV2_Atom_Sequence* control, const BassGenURIDs& urids, TimeInfo* info) {
    if (!control || !info) {
        return false;
    }

    bool found = false;
    TimeInfo local = *info;

    LV2_ATOM_SEQUENCE_FOREACH(control, ev) {
        const LV2_Atom_Object* obj = nullptr;
        if (ev->body.type == urids.time_Position) {
            obj = (const LV2_Atom_Object*)&ev->body;
        } else if (ev->body.type == urids.atom_Object || ev->body.type == urids.atom_Blank) {
            const LV2_Atom_Object* cand = (const LV2_Atom_Object*)&ev->body;
            if (cand->body.otype == urids.time_Position) {
                obj = cand;
            }
        }

        if (!obj) {
            continue;
        }

        found = true;
        const LV2_Atom* bar_atom = nullptr;
        const LV2_Atom* bar_beat_atom = nullptr;
        const LV2_Atom* beats_per_bar_atom = nullptr;
        const LV2_Atom* bpm_atom = nullptr;
        const LV2_Atom* speed_atom = nullptr;

        lv2_atom_object_get(obj,
                            urids.time_bar, &bar_atom,
                            urids.time_barBeat, &bar_beat_atom,
                            urids.time_beatsPerBar, &beats_per_bar_atom,
                            urids.time_beatsPerMinute, &bpm_atom,
                            urids.time_speed, &speed_atom,
                            0);

        double value = 0.0;
        if (atom_to_double(bar_atom, urids, &value)) {
            local.bar = value;
        }
        if (atom_to_double(bar_beat_atom, urids, &value)) {
            local.barBeat = value;
        }
        if (atom_to_double(beats_per_bar_atom, urids, &value) && value > 0.0) {
            local.beatsPerBar = value;
        }
        if (atom_to_double(bpm_atom, urids, &value) && value > 0.0) {
            local.bpm = value;
        }
        if (atom_to_double(speed_atom, urids, &value)) {
            local.playing = value > 0.0;
        }
    }

    if (found) {
        local.valid = true;
        *info = local;
    }

    return found;
}

static int steps_per_beat_for_subdivision(int subdivision) {
    switch (subdivision) {
        case SUBDIV_8TH: return 2;
        case SUBDIV_16TH: return 4;
        case SUBDIV_16T: return 6;
        default: return 4;
    }
}

static int register_offset(int reg) {
    switch (reg) {
        case 0: return -12;
        case 1: return 0;
        case 2: return 12;
        case 3: return 24;
        default: return 0;
    }
}

static void emit_midi_3(BassGen* self, uint32_t frame, uint8_t status, uint8_t data1, uint8_t data2) {
    LV2_Atom_Event ev;
    ev.time.frames = frame;
    ev.body.type = self->urids.midi_Event;
    ev.body.size = 3;

    LV2_Atom_Event* out_ev = lv2_atom_sequence_append_event(self->midi_out, 8192, &ev);
    if (!out_ev) {
        return;
    }

    uint8_t* out = (uint8_t*)(out_ev + 1);
    out[0] = status;
    out[1] = data1;
    out[2] = data2;
}

static void emit_note_off(BassGen* self, uint32_t frame, int note) {
    if (note < 0) {
        return;
    }
    const uint8_t status = (uint8_t)(0x80 | clampi(self->controls.channel - 1, 0, 15));
    emit_midi_3(self, frame, status, (uint8_t)clampi(note, 0, 127), 0);
}

static void emit_note_on(BassGen* self, uint32_t frame, int note, int velocity) {
    const uint8_t status = (uint8_t)(0x90 | clampi(self->controls.channel - 1, 0, 15));
    emit_midi_3(self, frame, status, (uint8_t)clampi(note, 0, 127), (uint8_t)clampi(velocity, 1, 127));
}

static ControlSnapshot read_controls(BassGen* self) {
    ControlSnapshot c = self->controls;
    c.root_note = clampi((int)lroundf(self->root_note_port ? *self->root_note_port : 36.0f), 0, 127);
    c.scale = clampi((int)lroundf(self->scale_port ? *self->scale_port : 0.0f), 0, SCALE_COUNT - 1);
    c.genre = clampi((int)lroundf(self->genre_port ? *self->genre_port : 0.0f), 0, GENRE_COUNT - 1);
    c.channel = clampi((int)lroundf(self->channel_port ? *self->channel_port : 1.0f), 1, 16);
    c.length_beats = clampi((int)lroundf(self->length_beats_port ? *self->length_beats_port : 16.0f), kMinLengthBeats, kMaxLengthBeats);
    c.subdivision = clampi((int)lroundf(self->subdivision_port ? *self->subdivision_port : 1.0f), 0, SUBDIV_COUNT - 1);
    c.density = clampf(self->density_port ? *self->density_port : 0.45f, 0.0f, 1.0f);
    c.reg = clampi((int)lroundf(self->register_port ? *self->register_port : 1.0f), 0, 3);
    c.hold = clampf(self->hold_port ? *self->hold_port : 0.35f, 0.0f, 1.0f);
    c.accent = clampf(self->accent_port ? *self->accent_port : 0.45f, 0.0f, 1.0f);
    c.seed = (uint32_t)clampi((int)lroundf(self->seed_port ? *self->seed_port : 1.0f), 0, 65535);
    c.action_new = clampi((int)lroundf(self->action_new_port ? *self->action_new_port : 0.0f), 0, 1048576);
    c.action_notes = clampi((int)lroundf(self->action_notes_port ? *self->action_notes_port : 0.0f), 0, 1048576);
    c.action_rhythm = clampi((int)lroundf(self->action_rhythm_port ? *self->action_rhythm_port : 0.0f), 0, 1048576);
    return c;
}

static bool structural_controls_changed(const ControlSnapshot& a, const ControlSnapshot& b) {
    return a.root_note != b.root_note ||
           a.scale != b.scale ||
           a.genre != b.genre ||
           a.length_beats != b.length_beats ||
           a.subdivision != b.subdivision ||
           fabsf(a.density - b.density) > 0.0001f ||
           a.reg != b.reg ||
           fabsf(a.hold - b.hold) > 0.0001f ||
           fabsf(a.accent - b.accent) > 0.0001f ||
           a.seed != b.seed;
}

static bool event_active_at(const NoteEventData& ev, double local_step) {
    const double start = (double)ev.start_step;
    const double end = (double)(ev.start_step + ev.duration_steps);
    return local_step >= start && local_step < end;
}

static const NoteEventData* find_active_event(const PatternStateBlob& pattern, double local_step) {
    for (int i = 0; i < pattern.event_count; ++i) {
        if (event_active_at(pattern.events[i], local_step)) {
            return &pattern.events[i];
        }
    }
    return nullptr;
}

static int choose_degree(Rng* rng, int genre, bool strong_beat, int prev_degree) {
    const float roll = rng->next_float();
    if (strong_beat) {
        if (genre == GENRE_FUNK) {
            if (roll < 0.35f) return 0;
            if (roll < 0.58f) return 4;
            if (roll < 0.76f) return 7;
            return 2;
        }
        if (genre == GENRE_SABBATH) {
            if (roll < 0.52f) return 0;
            if (roll < 0.76f) return 4;
            if (roll < 0.90f) return 6;
            return 1;
        }
        if (roll < 0.45f) return 0;
        if (roll < 0.70f) return 4;
        if (roll < 0.82f) return 2;
    }

    switch (genre) {
        case GENRE_FUNK:
            if (roll < 0.24f) return prev_degree;
            if (roll < 0.45f) return (prev_degree < 7) ? prev_degree + 1 : 4;
            if (roll < 0.61f) return (prev_degree > 0) ? prev_degree - 1 : 0;
            if (roll < 0.78f) return 7;
            return (rng->next_float() < 0.5f) ? 0 : 4;
        case GENRE_SABBATH:
            if (roll < 0.36f) return 0;
            if (roll < 0.58f) return 4;
            if (roll < 0.74f) return 6;
            if (roll < 0.86f) return 1;
            return (roll < 0.93f) ? prev_degree : 3;
        case GENRE_ACID:
            if (roll < 0.25f) return prev_degree;
            if (roll < 0.55f) return clampi(prev_degree + rng->next_int(-1, 1), 0, 6);
            return rng->next_int(0, 6);
        case GENRE_DUB:
            if (roll < 0.55f) return 0;
            if (roll < 0.75f) return 4;
            return clampi(prev_degree + rng->next_int(-1, 1), 0, 6);
        case GENRE_AMBIENT:
            if (roll < 0.40f) return prev_degree;
            return clampi(prev_degree + rng->next_int(-1, 1), 0, 6);
        default:
            if (roll < 0.30f) return 0;
            if (roll < 0.50f) return 4;
            return clampi(prev_degree + rng->next_int(-1, 1), 0, 6);
    }
}

static int note_from_degree(const ControlSnapshot& controls, int degree_index) {
    const ScaleDef& scale = kScales[controls.scale];
    const int octave = degree_index / scale.count;
    const int degree = degree_index % scale.count;
    const int interval = scale.intervals[degree] + 12 * octave;
    const int base = controls.root_note + register_offset(controls.reg);
    return clampi(base + interval, 0, 127);
}

static float genre_density_bias(int genre, bool strong) {
    switch (genre) {
        case GENRE_TECHNO: return strong ? 1.15f : 0.78f;
        case GENRE_ACID: return strong ? 0.95f : 1.10f;
        case GENRE_HOUSE: return strong ? 0.80f : 1.20f;
        case GENRE_ELECTRO: return strong ? 1.00f : 1.05f;
        case GENRE_DUB: return strong ? 1.20f : 0.58f;
        case GENRE_AMBIENT: return strong ? 0.90f : 0.45f;
        case GENRE_FUNK: return strong ? 0.84f : 1.28f;
        case GENRE_SABBATH: return strong ? 1.22f : 0.52f;
        default: return 1.0f;
    }
}

static int next_onset_step(const bool* onset, int pattern_steps, int step) {
    for (int j = step + 1; j < pattern_steps; ++j) {
        if (onset[j]) {
            return j;
        }
    }
    return pattern_steps;
}

static int choose_duration_steps(const ControlSnapshot& controls, Rng* rng, int available_steps) {
    if (available_steps <= 1) {
        return 1;
    }
    float hold_bias = controls.hold;
    switch (controls.genre) {
        case GENRE_FUNK:
            hold_bias *= 0.72f;
            break;
        case GENRE_SABBATH:
            hold_bias = clampf(hold_bias * 1.35f + 0.12f, 0.0f, 1.0f);
            break;
        default:
            break;
    }
    const int max_hold = clampi((int)floorf(1.0f + hold_bias * (float)(available_steps - 1)), 1, available_steps);
    return clampi(1 + rng->next_int(0, max_hold - 1), 1, available_steps);
}

static int sabbath_cell_length(const PatternStateBlob* pattern) {
    if (pattern->event_count >= 8) return 4;
    if (pattern->event_count >= 4) return 3;
    return 2;
}

static void build_sabbath_degree_cell(int* cell, int cell_len, Rng* rng) {
    if (cell_len <= 0) {
        return;
    }
    cell[0] = 0;
    if (cell_len > 1) {
        const float roll = rng->next_float();
        cell[1] = (roll < 0.45f) ? 4 : ((roll < 0.78f) ? 6 : 1);
    }
    if (cell_len > 2) {
        const float roll = rng->next_float();
        cell[2] = (roll < 0.40f) ? 0 : ((roll < 0.68f) ? 6 : ((roll < 0.88f) ? 1 : 3));
    }
    if (cell_len > 3) {
        const float roll = rng->next_float();
        cell[3] = (roll < 0.50f) ? 4 : ((roll < 0.78f) ? 0 : 6);
    }
}

static int sabbath_cell_degree(const PatternStateBlob* pattern, Rng* rng, const int* cell, int cell_len, int event_index) {
    const int degree = cell[event_index % cell_len];
    const bool phrase_restart = (event_index % cell_len) == 0;
    const bool late_phrase = event_index >= cell_len && pattern->event_count > cell_len;
    if (!late_phrase || phrase_restart || rng->next_float() < 0.70f) {
        return degree;
    }

    const float roll = rng->next_float();
    if (roll < 0.45f) return degree;
    if (roll < 0.72f) return 0;
    if (roll < 0.86f) return 4;
    return (degree == 6) ? 1 : 6;
}

static void ensure_first_event(PatternStateBlob* pattern, const ControlSnapshot& controls) {
    if (pattern->event_count > 0) {
        return;
    }
    pattern->event_count = 1;
    pattern->events[0].start_step = 0;
    pattern->events[0].duration_steps = pattern->steps_per_beat;
    pattern->events[0].note = clampi(controls.root_note + register_offset(controls.reg), 0, 127);
    pattern->events[0].velocity = 96;
}

static void generate_rhythm(PatternStateBlob* pattern, const ControlSnapshot& controls, Rng* rng) {
    pattern->event_count = 0;
    pattern->steps_per_beat = steps_per_beat_for_subdivision(controls.subdivision);
    pattern->pattern_steps = clampi(controls.length_beats * pattern->steps_per_beat, 1, kMaxPatternSteps);
    pattern->generation_serial += 1;

    bool onset[kMaxPatternSteps];
    memset(onset, 0, sizeof(onset));

    onset[0] = true;
    int cooldown = 0;

    for (int step = 1; step < pattern->pattern_steps; ++step) {
        const bool strong = (step % pattern->steps_per_beat) == 0;
        float probability = controls.density * genre_density_bias(controls.genre, strong);

        if (cooldown > 0) {
            probability *= 0.30f;
            cooldown -= 1;
        }

        if (rng->next_float() < clampf(probability, 0.02f, 0.95f)) {
            onset[step] = true;
            cooldown = 1;
        }
    }

    for (int step = 0; step < pattern->pattern_steps && pattern->event_count < kMaxEvents; ++step) {
        if (!onset[step]) {
            continue;
        }

        const int next_step = next_onset_step(onset, pattern->pattern_steps, step);
        const int available = clampi(next_step - step, 1, pattern->pattern_steps);
        const int duration = choose_duration_steps(controls, rng, available);

        pattern->events[pattern->event_count].start_step = step;
        pattern->events[pattern->event_count].duration_steps = duration;
        pattern->events[pattern->event_count].note = controls.root_note;
        pattern->events[pattern->event_count].velocity = 96;
        pattern->event_count += 1;
    }

    ensure_first_event(pattern, controls);
}

static void generate_notes(PatternStateBlob* pattern, const ControlSnapshot& controls, Rng* rng) {
    int prev_degree = 0;
    int sabbath_cell[4] = {0, 4, 6, 0};
    int sabbath_cell_len = 0;
    if (controls.genre == GENRE_SABBATH) {
        sabbath_cell_len = sabbath_cell_length(pattern);
        build_sabbath_degree_cell(sabbath_cell, sabbath_cell_len, rng);
    }

    for (int i = 0; i < pattern->event_count; ++i) {
        NoteEventData* ev = &pattern->events[i];
        const bool strong = (ev->start_step % pattern->steps_per_beat) == 0;
        const int degree = (controls.genre == GENRE_SABBATH)
            ? sabbath_cell_degree(pattern, rng, sabbath_cell, sabbath_cell_len, i)
            : choose_degree(rng, controls.genre, strong, prev_degree);
        prev_degree = degree;
        ev->note = note_from_degree(controls, degree);

        const int base_velocity = 86;
        const int accent_boost = strong ? (int)lroundf(controls.accent * 28.0f) : 0;
        const int random_boost = rng->next_int(0, 10);
        ev->velocity = clampi(base_velocity + accent_boost + random_boost, 1, 127);
    }
}

static void sort_events(PatternStateBlob* pattern) {
    for (int i = 0; i < pattern->event_count - 1; ++i) {
        for (int j = i + 1; j < pattern->event_count; ++j) {
            if (pattern->events[j].start_step < pattern->events[i].start_step) {
                const NoteEventData tmp = pattern->events[i];
                pattern->events[i] = pattern->events[j];
                pattern->events[j] = tmp;
            }
        }
    }
}

static void copy_note_content_from_pattern(PatternStateBlob* target, const PatternStateBlob* source) {
    if (!target || !source || source->event_count <= 0) {
        return;
    }

    for (int i = 0; i < target->event_count; ++i) {
        const NoteEventData* src = &source->events[i % source->event_count];
        target->events[i].note = src->note;
        target->events[i].velocity = src->velocity;
    }
}

static void regenerate_pattern(BassGen* self, bool regen_rhythm, bool regen_notes) {
    const uint32_t seed_mix = self->controls.seed ^ (uint32_t)(self->pattern.generation_serial * 2654435761u);
    Rng rng;
    rng.seed(seed_mix);
    PatternStateBlob previous_pattern = self->pattern;
    const bool had_previous_pattern = self->pattern_valid && previous_pattern.event_count > 0;

    if (!self->pattern_valid || regen_rhythm) {
        generate_rhythm(&self->pattern, self->controls, &rng);
    }

    if (regen_rhythm && !regen_notes && had_previous_pattern) {
        copy_note_content_from_pattern(&self->pattern, &previous_pattern);
    }

    if (!self->pattern_valid || regen_notes) {
        if (regen_rhythm) {
            rng.seed(seed_mix ^ 0xA5A5A5A5u);
        }
        generate_notes(&self->pattern, self->controls, &rng);
    }

    sort_events(&self->pattern);
    self->pattern_valid = true;
}

static void update_pattern_if_needed(BassGen* self, const ControlSnapshot& fresh) {
    const bool params_changed = structural_controls_changed(fresh, self->previous_controls);
    const bool trigger_new = fresh.action_new != self->previous_controls.action_new;
    const bool trigger_notes = fresh.action_notes != self->previous_controls.action_notes;
    const bool trigger_rhythm = fresh.action_rhythm != self->previous_controls.action_rhythm;

    self->controls = fresh;

    if (!self->pattern_valid || params_changed || trigger_new) {
        regenerate_pattern(self, true, true);
    } else if (trigger_rhythm) {
        regenerate_pattern(self, true, false);
    } else if (trigger_notes) {
        regenerate_pattern(self, false, true);
    }

    self->previous_controls = fresh;
}

static void clear_active_note(BassGen* self, uint32_t frame) {
    if (self->active_note >= 0) {
        emit_note_off(self, frame, self->active_note);
        self->active_note = -1;
    }
}

static void sync_note_state_to_position(BassGen* self, double local_step_pos) {
    const NoteEventData* ev = self->pattern_valid ? find_active_event(self->pattern, local_step_pos) : nullptr;
    const int should_note = ev ? ev->note : -1;
    if (self->active_note >= 0 && self->active_note != should_note) {
        emit_note_off(self, 0, self->active_note);
        self->active_note = -1;
    }
    if (should_note >= 0 && self->active_note < 0) {
        emit_note_on(self, 0, should_note, ev->velocity);
        self->active_note = should_note;
    }
}

static void reset_transport_state(BassGen* self) {
    self->was_playing = false;
    self->last_transport_step = -1;
}

static void handle_stopped_transport(BassGen* self) {
    clear_active_note(self, 0);
    reset_transport_state(self);
}

static bool transport_restart_detected(const BassGen* self, int64_t start_step_floor) {
    return !self->was_playing || (self->last_transport_step >= 0 && start_step_floor < self->last_transport_step);
}

static void handle_transport_restart(BassGen* self, double abs_steps_start) {
    clear_active_note(self, 0);
    const double local_step = fmod(abs_steps_start, (double)self->pattern.pattern_steps);
    sync_note_state_to_position(self, local_step < 0.0 ? local_step + self->pattern.pattern_steps : local_step);
}

static void prepare_midi_output(BassGen* self) {
    self->midi_out->atom.type = self->urids.atom_Sequence;
    self->midi_out->atom.size = sizeof(LV2_Atom_Sequence_Body);
    self->midi_out->body.unit = 0;
    self->midi_out->body.pad = 0;
}

static const NoteEventData* find_event_starting_at(const PatternStateBlob& pattern, int local_step) {
    for (int i = 0; i < pattern.event_count; ++i) {
        if (pattern.events[i].start_step == local_step) {
            return &pattern.events[i];
        }
    }
    return nullptr;
}

static bool any_event_ends_at(const PatternStateBlob& pattern, int local_step) {
    for (int i = 0; i < pattern.event_count; ++i) {
        const int end_step = (pattern.events[i].start_step + pattern.events[i].duration_steps) % pattern.pattern_steps;
        if (pattern.events[i].duration_steps < pattern.pattern_steps && end_step == local_step) {
            return true;
        }
    }
    return false;
}

static uint32_t frame_for_boundary(double abs_steps_start, double abs_steps_end, uint32_t nframes, int64_t boundary) {
    const double rel_steps = (double)boundary - abs_steps_start;
    const double t = rel_steps / (abs_steps_end - abs_steps_start + 1e-12);
    return (uint32_t)clampi((int)floor(t * (double)nframes), 0, (int)nframes - 1);
}

static int local_step_for_boundary(const PatternStateBlob& pattern, int64_t boundary) {
    const double local_step_d = fmod((double)boundary, (double)pattern.pattern_steps);
    return (int)(local_step_d < 0.0 ? local_step_d + pattern.pattern_steps : local_step_d);
}

static void process_boundary(BassGen* self, uint32_t nframes, double abs_steps_start, double abs_steps_end, int64_t boundary) {
    const uint32_t frame = frame_for_boundary(abs_steps_start, abs_steps_end, nframes, boundary);
    const int local_step = local_step_for_boundary(self->pattern, boundary);
    const bool ending_here = any_event_ends_at(self->pattern, local_step);
    const NoteEventData* start_event = find_event_starting_at(self->pattern, local_step);

    if (ending_here) {
        clear_active_note(self, frame);
    }

    if (start_event) {
        const uint32_t on_frame = (uint32_t)clampi((int)frame + kSafetyGapSamples, 0, (int)nframes - 1);
        clear_active_note(self, frame);
        emit_note_on(self, on_frame, start_event->note, start_event->velocity);
        self->active_note = start_event->note;
    }
}

static LV2_State_Status bassgen_state_save(LV2_Handle instance,
                                           LV2_State_Store_Function store,
                                           LV2_State_Handle handle,
                                           uint32_t,
                                           const LV2_Feature* const*) {
    BassGen* self = (BassGen*)instance;
    if (!self || !store) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    const uint32_t flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;
    store(handle, self->urids.state_controls,
          &self->controls, sizeof(ControlSnapshot), self->urids.atom_Chunk, flags);
    store(handle, self->urids.state_pattern,
          &self->pattern, sizeof(PatternStateBlob), self->urids.atom_Chunk, flags);
    return LV2_STATE_SUCCESS;
}

static LV2_State_Status bassgen_state_restore(LV2_Handle instance,
                                              LV2_State_Retrieve_Function retrieve,
                                              LV2_State_Handle handle,
                                              uint32_t,
                                              const LV2_Feature* const*) {
    BassGen* self = (BassGen*)instance;
    if (!self || !retrieve) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    size_t size = 0;
    uint32_t type = 0;
    uint32_t flags = 0;

    const void* controls_data = retrieve(handle, self->urids.state_controls, &size, &type, &flags);
    if (controls_data && size == sizeof(ControlSnapshot) && type == self->urids.atom_Chunk) {
        memcpy(&self->controls, controls_data, sizeof(ControlSnapshot));
        self->previous_controls = self->controls;
    }

    const void* pattern_data = retrieve(handle, self->urids.state_pattern, &size, &type, &flags);
    if (pattern_data && size == sizeof(PatternStateBlob) && type == self->urids.atom_Chunk) {
        memcpy(&self->pattern, pattern_data, sizeof(PatternStateBlob));
        self->pattern.event_count = clampi(self->pattern.event_count, 0, kMaxEvents);
        self->pattern.pattern_steps = clampi(self->pattern.pattern_steps, 1, kMaxPatternSteps);
        self->pattern.steps_per_beat = clampi(self->pattern.steps_per_beat, 1, 6);
        self->pattern_valid = true;
    }

    return LV2_STATE_SUCCESS;
}

static const LV2_State_Interface bassgen_state_interface = {
    bassgen_state_save,
    bassgen_state_restore
};

static LV2_Handle instantiate(const LV2_Descriptor*,
                              double rate,
                              const char*,
                              const LV2_Feature* const* features) {
    BassGen* self = new BassGen();
    if (!self) {
        return nullptr;
    }

    self->sample_rate = rate;
    for (int i = 0; features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_URID__map)) {
            self->map = (LV2_URID_Map*)features[i]->data;
        }
    }

    if (!self->map) {
        delete self;
        return nullptr;
    }

    self->urids.atom_Object = self->map->map(self->map->handle, LV2_ATOM__Object);
    self->urids.atom_Blank = self->map->map(self->map->handle, LV2_ATOM__Blank);
    self->urids.atom_Float = self->map->map(self->map->handle, LV2_ATOM__Float);
    self->urids.atom_Double = self->map->map(self->map->handle, LV2_ATOM__Double);
    self->urids.atom_Int = self->map->map(self->map->handle, LV2_ATOM__Int);
    self->urids.atom_Long = self->map->map(self->map->handle, LV2_ATOM__Long);
    self->urids.atom_Chunk = self->map->map(self->map->handle, LV2_ATOM__Chunk);
    self->urids.atom_Sequence = self->map->map(self->map->handle, LV2_ATOM__Sequence);
    self->urids.midi_Event = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);
    self->urids.time_Position = self->map->map(self->map->handle, LV2_TIME__Position);
    self->urids.time_speed = self->map->map(self->map->handle, LV2_TIME__speed);
    self->urids.time_bar = self->map->map(self->map->handle, LV2_TIME__bar);
    self->urids.time_barBeat = self->map->map(self->map->handle, LV2_TIME__barBeat);
    self->urids.time_beatsPerBar = self->map->map(self->map->handle, LV2_TIME__beatsPerBar);
    self->urids.time_beatsPerMinute = self->map->map(self->map->handle, LV2_TIME__beatsPerMinute);
    self->urids.state_controls = self->map->map(self->map->handle, BASSGEN_URI "#controls");
    self->urids.state_pattern = self->map->map(self->map->handle, BASSGEN_URI "#pattern");

    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    BassGen* self = (BassGen*)instance;
    switch (port) {
        case PORT_CONTROL: self->control = (const LV2_Atom_Sequence*)data; break;
        case PORT_MIDI_OUT: self->midi_out = (LV2_Atom_Sequence*)data; break;
        case PORT_ROOT_NOTE: self->root_note_port = (const float*)data; break;
        case PORT_SCALE: self->scale_port = (const float*)data; break;
        case PORT_GENRE: self->genre_port = (const float*)data; break;
        case PORT_CHANNEL: self->channel_port = (const float*)data; break;
        case PORT_LENGTH_BEATS: self->length_beats_port = (const float*)data; break;
        case PORT_SUBDIVISION: self->subdivision_port = (const float*)data; break;
        case PORT_DENSITY: self->density_port = (const float*)data; break;
        case PORT_REGISTER: self->register_port = (const float*)data; break;
        case PORT_HOLD: self->hold_port = (const float*)data; break;
        case PORT_ACCENT: self->accent_port = (const float*)data; break;
        case PORT_SEED: self->seed_port = (const float*)data; break;
        case PORT_ACTION_NEW: self->action_new_port = (const float*)data; break;
        case PORT_ACTION_NOTES: self->action_notes_port = (const float*)data; break;
        case PORT_ACTION_RHYTHM: self->action_rhythm_port = (const float*)data; break;
        default: break;
    }
}

static void activate(LV2_Handle instance) {
    BassGen* self = (BassGen*)instance;
    self->active_note = -1;
    reset_transport_state(self);
    self->controls = read_controls(self);
    self->previous_controls = self->controls;
    if (!self->pattern_valid) {
        regenerate_pattern(self, true, true);
    }
}

static void run(LV2_Handle instance, uint32_t nframes) {
    BassGen* self = (BassGen*)instance;
    if (!self || !self->midi_out) {
        return;
    }

    prepare_midi_output(self);

    update_pattern_if_needed(self, read_controls(self));

    TimeInfo info;
    info.bpm = 120.0;
    info.beatsPerBar = 4.0;
    info.playing = false;
    info.bar = 0.0;
    info.barBeat = 0.0;
    info.valid = false;
    read_time_info(self->control, self->urids, &info);

    const bool playing = info.valid && info.playing && info.bpm > 0.0 && info.beatsPerBar > 0.0;
    if (!playing || !self->pattern_valid) {
        handle_stopped_transport(self);
        return;
    }

    const int spb = self->pattern.steps_per_beat;
    const double abs_beats_start = info.bar * info.beatsPerBar + info.barBeat;
    const double abs_beats_step = ((double)nframes * info.bpm) / (60.0 * self->sample_rate);
    const double abs_steps_start = abs_beats_start * (double)spb;
    const double abs_steps_end = (abs_beats_start + abs_beats_step) * (double)spb;
    const int64_t start_step_floor = (int64_t)floor(abs_steps_start + 1e-9);

    if (transport_restart_detected(self, start_step_floor)) {
        handle_transport_restart(self, abs_steps_start);
    }

    self->was_playing = true;
    self->last_transport_step = start_step_floor;

    int64_t boundary = (int64_t)floor(abs_steps_start) + 1;
    const int64_t boundary_end = (int64_t)floor(abs_steps_end + 1e-9);

    while (boundary <= boundary_end) {
        process_boundary(self, nframes, abs_steps_start, abs_steps_end, boundary);
        boundary += 1;
    }
}

static void deactivate(LV2_Handle instance) {
    BassGen* self = (BassGen*)instance;
    self->active_note = -1;
    reset_transport_state(self);
}

static void cleanup(LV2_Handle instance) {
    BassGen* self = (BassGen*)instance;
    delete self;
}

static const void* extension_data(const char* uri) {
    if (!strcmp(uri, LV2_STATE__interface)) {
        return &bassgen_state_interface;
    }
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    BASSGEN_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : nullptr;
}
