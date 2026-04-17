#include "bassgen_pattern.hpp"

#include "bassgen_rng.hpp"

#include <climits>
#include <cmath>
#include <cstring>
#include <utility>

namespace {

struct ScaleDef {
    const int* intervals;
    int count;
};

const int kScaleMinor[] = {0, 2, 3, 5, 7, 8, 10};
const int kScaleMajor[] = {0, 2, 4, 5, 7, 9, 11};
const int kScaleDorian[] = {0, 2, 3, 5, 7, 9, 10};
const int kScalePhrygian[] = {0, 1, 3, 5, 7, 8, 10};
const int kScalePentMinor[] = {0, 3, 5, 7, 10};
const int kScaleBlues[] = {0, 3, 5, 6, 7, 10};
const int kScaleMixolydian[] = {0, 2, 4, 5, 7, 9, 10};
const int kScaleHarmonicMinor[] = {0, 2, 3, 5, 7, 8, 11};
const int kScalePentMajor[] = {0, 2, 4, 7, 9};
const int kScaleLocrian[] = {0, 1, 3, 5, 6, 8, 10};
const int kScalePhrygianDominant[] = {0, 1, 4, 5, 7, 8, 10};

const ScaleDef kScales[SCALE_COUNT] = {
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

inline float clampf(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

inline int clampi(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

int next_generation_serial(int32_t current_serial) {
    return current_serial >= INT32_MAX ? 1 : current_serial + 1;
}

uint32_t seed_mix_for_serial(const ControlSnapshot& controls, int32_t generation_serial) {
    return controls.seed ^ ((uint32_t)generation_serial * 2654435761u);
}

int choose_degree(BassGenRng* rng, int genre, bool strong_beat, int prev_degree) {
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

int note_from_degree(const ControlSnapshot& controls, int degree_index) {
    const ScaleDef& scale = kScales[controls.scale];
    const int octave = degree_index / scale.count;
    const int degree = degree_index % scale.count;
    const int interval = scale.intervals[degree] + 12 * octave;
    const int base = controls.root_note + bassgen_register_offset(controls.reg);
    return clampi(base + interval, 0, 127);
}

float genre_density_bias(int genre, bool strong) {
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

int next_onset_step(const bool* onset, int pattern_steps, int step) {
    for (int j = step + 1; j < pattern_steps; ++j) {
        if (onset[j]) {
            return j;
        }
    }
    return pattern_steps;
}

int choose_duration_steps(const ControlSnapshot& controls, BassGenRng* rng, int available_steps) {
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

int sabbath_cell_length(const PatternStateBlob* pattern) {
    if (pattern->event_count >= 8) return 4;
    if (pattern->event_count >= 4) return 3;
    return 2;
}

void build_sabbath_degree_cell(int* cell, int cell_len, BassGenRng* rng) {
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

int sabbath_cell_degree(const PatternStateBlob* pattern,
                        BassGenRng* rng,
                        const int* cell,
                        int cell_len,
                        int event_index) {
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

void ensure_first_event(PatternStateBlob* pattern, const ControlSnapshot& controls) {
    if (pattern->event_count > 0) {
        return;
    }

    pattern->event_count = 1;
    pattern->events[0].start_step = 0;
    pattern->events[0].duration_steps = pattern->steps_per_beat;
    pattern->events[0].note = clampi(controls.root_note + bassgen_register_offset(controls.reg), 0, 127);
    pattern->events[0].velocity = 96;
}

void generate_rhythm(PatternStateBlob* pattern, const ControlSnapshot& controls, BassGenRng* rng) {
    pattern->event_count = 0;
    pattern->steps_per_beat = bassgen_steps_per_beat_for_subdivision(controls.subdivision);
    pattern->pattern_steps = clampi(controls.length_beats * pattern->steps_per_beat, 1, BASSGEN_MAX_PATTERN_STEPS);

    bool onset[BASSGEN_MAX_PATTERN_STEPS];
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

    for (int step = 0; step < pattern->pattern_steps && pattern->event_count < BASSGEN_MAX_EVENTS; ++step) {
        if (!onset[step]) {
            continue;
        }

        const int next_step = next_onset_step(onset, pattern->pattern_steps, step);
        const int available = clampi(next_step - step, 1, pattern->pattern_steps);
        const int duration = choose_duration_steps(controls, rng, available);

        NoteEventData* ev = &pattern->events[pattern->event_count++];
        ev->start_step = step;
        ev->duration_steps = duration;
        ev->note = controls.root_note;
        ev->velocity = 96;
    }

    ensure_first_event(pattern, controls);
}

void generate_notes(PatternStateBlob* pattern, const ControlSnapshot& controls, BassGenRng* rng) {
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

void sort_events(PatternStateBlob* pattern) {
    for (int i = 0; i < pattern->event_count - 1; ++i) {
        for (int j = i + 1; j < pattern->event_count; ++j) {
            if (pattern->events[j].start_step < pattern->events[i].start_step) {
                std::swap(pattern->events[i], pattern->events[j]);
            }
        }
    }
}

void copy_note_content_from_pattern(PatternStateBlob* target, const PatternStateBlob* source) {
    if (!target || !source || source->event_count <= 0) {
        return;
    }

    for (int i = 0; i < target->event_count; ++i) {
        const NoteEventData* src = &source->events[i % source->event_count];
        target->events[i].note = src->note;
        target->events[i].velocity = src->velocity;
    }
}

}  // namespace

ControlSnapshot bassgen_clamp_controls(const ControlSnapshot& raw) {
    ControlSnapshot controls = raw;
    controls.root_note = clampi(controls.root_note, 0, 127);
    controls.scale = clampi(controls.scale, 0, SCALE_COUNT - 1);
    controls.genre = clampi(controls.genre, 0, GENRE_COUNT - 1);
    controls.channel = clampi(controls.channel, 1, 16);
    controls.length_beats = clampi(controls.length_beats, BASSGEN_MIN_LENGTH_BEATS, BASSGEN_MAX_LENGTH_BEATS);
    controls.subdivision = clampi(controls.subdivision, 0, SUBDIV_COUNT - 1);
    controls.density = clampf(controls.density, 0.0f, 1.0f);
    controls.reg = clampi(controls.reg, 0, 3);
    controls.hold = clampf(controls.hold, 0.0f, 1.0f);
    controls.accent = clampf(controls.accent, 0.0f, 1.0f);
    controls.vary = clampf(controls.vary, 0.0f, 1.0f);
    controls.action_new = clampi(controls.action_new, 0, 1048576);
    controls.action_notes = clampi(controls.action_notes, 0, 1048576);
    controls.action_rhythm = clampi(controls.action_rhythm, 0, 1048576);
    return controls;
}

bool bassgen_structural_controls_changed(const ControlSnapshot& a, const ControlSnapshot& b) {
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

int bassgen_steps_per_beat_for_subdivision(int subdivision) {
    switch (subdivision) {
        case SUBDIV_8TH: return 2;
        case SUBDIV_16TH: return 4;
        case SUBDIV_16T: return 6;
        default: return 4;
    }
}

int bassgen_register_offset(int reg) {
    switch (reg) {
        case 0: return -12;
        case 1: return 0;
        case 2: return 12;
        case 3: return 24;
        default: return 0;
    }
}

void bassgen_regenerate_pattern(PatternStateBlob* pattern,
                                const ControlSnapshot& controls,
                                bool regen_rhythm,
                                bool regen_notes) {
    if (!pattern) {
        return;
    }

    if (!regen_rhythm && pattern->event_count <= 0) {
        regen_rhythm = true;
        regen_notes = true;
    }

    const int32_t next_serial = next_generation_serial(pattern->generation_serial);
    const uint32_t seed_mix = seed_mix_for_serial(controls, next_serial);
    const PatternStateBlob previous_pattern = *pattern;
    const bool had_previous_pattern = previous_pattern.event_count > 0;

    BassGenRng rng;
    rng.seed(seed_mix);

    if (regen_rhythm || pattern->pattern_steps <= 0 || pattern->steps_per_beat <= 0) {
        generate_rhythm(pattern, controls, &rng);
    }

    if (regen_rhythm && !regen_notes && had_previous_pattern) {
        copy_note_content_from_pattern(pattern, &previous_pattern);
    }

    if (regen_notes || pattern->event_count <= 0) {
        if (regen_rhythm) {
            rng.seed(seed_mix ^ 0xA5A5A5A5u);
        }
        generate_notes(pattern, controls, &rng);
    }

    sort_events(pattern);
    pattern->version = BASSGEN_PATTERN_STATE_VERSION;
    pattern->generation_serial = next_serial;
}

void bassgen_partial_note_mutation(PatternStateBlob* pattern,
                                   const ControlSnapshot& controls,
                                   float strength) {
    if (!pattern || pattern->event_count <= 0) {
        bassgen_regenerate_pattern(pattern, controls, true, true);
        return;
    }

    const int32_t next_serial = next_generation_serial(pattern->generation_serial);
    const uint32_t seed_mix = seed_mix_for_serial(controls, next_serial);
    PatternStateBlob mutated = *pattern;

    BassGenRng note_rng;
    note_rng.seed(seed_mix ^ 0xA5A5A5A5u);
    generate_notes(&mutated, controls, &note_rng);

    BassGenRng select_rng;
    select_rng.seed(seed_mix ^ 0x5A5A5A5Au);

    strength = clampf(strength, 0.05f, 1.0f);
    const int event_count = clampi(pattern->event_count, 1, BASSGEN_MAX_EVENTS);
    const int mutation_count = clampi((int)lroundf(1.0f + strength * (float)(event_count - 1)), 1, event_count);

    int indices[BASSGEN_MAX_EVENTS];
    for (int i = 0; i < event_count; ++i) {
        indices[i] = i;
    }

    for (int i = 0; i < mutation_count; ++i) {
        const int swap_index = i + select_rng.next_int(0, event_count - i - 1);
        std::swap(indices[i], indices[swap_index]);

        const int event_index = indices[i];
        pattern->events[event_index].note = mutated.events[event_index].note;
        pattern->events[event_index].velocity = mutated.events[event_index].velocity;
    }

    pattern->version = BASSGEN_PATTERN_STATE_VERSION;
    pattern->generation_serial = next_serial;
}
