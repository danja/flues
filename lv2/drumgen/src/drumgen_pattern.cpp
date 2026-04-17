#include "drumgen_pattern.hpp"

#include "drumgen_rng.hpp"

#include <climits>
#include <cmath>
#include <cstring>

namespace {

const int kFluesDrumkitNotes[DRUMGEN_LANE_COUNT] = {36, 39, 40, 41, 42, 45, 46, 50, 51, 52, 53};
const int kGMNotes[DRUMGEN_LANE_COUNT] = {36, 39, 38, 49, 42, 45, 46, 50, 57, 56, 75};

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

uint32_t base_seed_for_serial(const ControlSnapshot& controls, int32_t generation_serial) {
    return controls.seed ^
           ((uint32_t)controls.genre * 0x45D9F3Bu) ^
           ((uint32_t)generation_serial * 0x9E3779B9u);
}

uint32_t fill_seed_for_serial(const ControlSnapshot& controls, int32_t generation_serial) {
    return controls.seed ^
           ((uint32_t)generation_serial * 0x85EBCA6Bu) ^
           0xA511E9B3u;
}

uint32_t bar_seed_for_serial(const ControlSnapshot& controls, int32_t generation_serial, int bar_index, bool fill_bar) {
    const uint32_t base_seed = base_seed_for_serial(controls, generation_serial);
    const uint32_t fill_seed = fill_seed_for_serial(controls, generation_serial);
    return base_seed ^
           ((uint32_t)(bar_index + 1) * 0x27D4EB2Du) ^
           (fill_bar ? fill_seed : 0u);
}

int note_for_lane(int kit_map, int lane) {
    const int* table = (kit_map == KIT_MAP_GM) ? kGMNotes : kFluesDrumkitNotes;
    return table[clampi(lane, 0, DRUMGEN_LANE_COUNT - 1)];
}

float lane_macro(const ControlSnapshot& controls, int lane) {
    switch (lane) {
        case LANE_KICK: return controls.kick_amt;
        case LANE_CLAP:
        case LANE_SNARE: return controls.backbeat_amt;
        case LANE_CLOSED_HAT:
        case LANE_OPEN_HAT: return controls.hat_amt;
        case LANE_LOW_TOM:
        case LANE_HIGH_TOM: return controls.tom_amt;
        case LANE_CRASH:
        case LANE_BASH: return controls.metal_amt;
        case LANE_COWBELL:
        case LANE_CLAVE:
        default: return controls.aux_amt;
    }
}

bool is_offbeat_step(int step_in_bar, int steps_per_beat) {
    if (steps_per_beat <= 0) {
        return false;
    }
    if (steps_per_beat == 3) {
        return (step_in_bar % steps_per_beat) == 2;
    }
    return (step_in_bar % steps_per_beat) == (steps_per_beat / 2);
}

bool is_fill_zone_step(int step_in_bar, int steps_per_bar, int steps_per_beat, float fill) {
    const int fill_beats = fill > 0.62f ? 2 : 1;
    const int fill_steps = clampi(fill_beats * steps_per_beat, steps_per_beat, steps_per_bar);
    return step_in_bar >= steps_per_bar - fill_steps;
}

bool euclid_hit(int step, int pulses, int offset, int length) {
    if (length <= 0 || pulses <= 0) {
        return false;
    }
    if (pulses >= length) {
        return true;
    }

    const int base_step = (step - offset + length) % length;
    return ((base_step * pulses) % length) < pulses;
}

float anchor_probability(const ControlSnapshot& controls,
                         int lane,
                         int bar_index,
                         int beat_index,
                         int sub_index,
                         int steps_per_beat,
                         bool fill_bar) {
    const bool beat_start = sub_index == 0;
    const bool offbeat = is_offbeat_step(beat_index * steps_per_beat + sub_index, steps_per_beat);
    const bool late_sub = sub_index == steps_per_beat - 1;
    const float kick = controls.kick_amt;
    const float backbeat = controls.backbeat_amt;
    const float hat = controls.hat_amt;
    const float tom = controls.tom_amt;
    const float metal = controls.metal_amt;
    const float perc = controls.aux_amt;
    const float fill = controls.fill;

    switch (lane) {
        case LANE_KICK:
            switch (controls.genre) {
                case GENRE_DISCO:
                case GENRE_MOTORIK:
                    if (beat_start) return 0.86f + 0.12f * kick;
                    if (offbeat && beat_index == 3) return 0.12f + 0.10f * controls.variation;
                    break;
                case GENRE_SHUFFLE:
                    if (beat_index == 0 && beat_start) return 0.96f;
                    if (beat_index == 2 && beat_start) return 0.62f + 0.18f * kick;
                    if (offbeat && (beat_index == 1 || beat_index == 3)) return 0.16f + 0.10f * controls.variation;
                    break;
                case GENRE_ELECTRO:
                    if (beat_index == 0 && beat_start) return 0.94f;
                    if (beat_index == 2 && beat_start) return 0.40f + 0.22f * kick;
                    if (late_sub) return 0.12f + 0.22f * controls.variation;
                    break;
                case GENRE_DUB:
                    if (beat_index == 0 && beat_start) return 0.96f;
                    if (beat_index == 2 && beat_start) return 0.24f + 0.18f * kick;
                    if (offbeat && beat_index == 3) return 0.10f + 0.10f * controls.variation;
                    break;
                case GENRE_BOSSA:
                    if (beat_index == 0 && beat_start) return 0.82f;
                    if (beat_index == 1 && late_sub) return 0.34f;
                    if (beat_index == 2 && beat_start) return 0.56f;
                    if (beat_index == 3 && offbeat) return 0.42f;
                    break;
                case GENRE_AFRO:
                    if (beat_index == 0 && beat_start) return 0.84f;
                    if (beat_index == 1 && offbeat) return 0.34f;
                    if (beat_index == 2 && beat_start) return 0.58f;
                    if (beat_index == 3 && offbeat) return 0.38f;
                    break;
                case GENRE_ROCK:
                default:
                    if (beat_index == 0 && beat_start) return 0.98f;
                    if (beat_index == 2 && beat_start) return 0.68f + 0.18f * kick;
                    if (beat_index == 3 && late_sub) return 0.10f + 0.18f * controls.variation;
                    if (beat_index == 1 && beat_start) return 0.12f + 0.10f * kick;
                    break;
            }
            break;

        case LANE_SNARE:
            if ((beat_index == 1 || beat_index == 3) && beat_start) {
                switch (controls.genre) {
                    case GENRE_DISCO: return 0.76f + 0.18f * backbeat;
                    case GENRE_ELECTRO: return 0.72f + 0.18f * backbeat;
                    case GENRE_DUB: return 0.64f + 0.16f * backbeat;
                    default: return 0.84f + 0.12f * backbeat;
                }
            }
            if (fill_bar && beat_index >= 2 && late_sub) return 0.08f + 0.24f * fill * backbeat;
            if (controls.genre == GENRE_AFRO && offbeat) return 0.08f + 0.10f * controls.variation;
            if (controls.genre == GENRE_SHUFFLE && late_sub) return 0.06f + 0.10f * controls.variation;
            break;

        case LANE_CLAP:
            if ((beat_index == 1 || beat_index == 3) && beat_start) {
                switch (controls.genre) {
                    case GENRE_DISCO: return 0.78f + 0.16f * backbeat;
                    case GENRE_ELECTRO: return 0.52f + 0.20f * backbeat;
                    case GENRE_DUB: return 0.18f + 0.14f * backbeat;
                    default: return 0.18f + 0.34f * backbeat;
                }
            }
            if (controls.genre == GENRE_DISCO && offbeat) return 0.08f + 0.14f * controls.variation;
            if (fill_bar && beat_index == 3 && !beat_start) return 0.06f + 0.18f * fill * backbeat;
            break;

        case LANE_CRASH:
            if (beat_start && beat_index == 0) {
                return (bar_index == 0 ? 0.34f : 0.16f) + 0.18f * metal;
            }
            if (fill_bar && beat_start && beat_index >= 2) {
                return 0.08f + 0.18f * fill * (0.55f + 0.45f * metal);
            }
            break;

        case LANE_CLOSED_HAT:
            if (steps_per_beat == 3) {
                if (sub_index == 0) return 0.70f + 0.18f * hat;
                if (sub_index == 2) return 0.54f + 0.20f * hat;
                return 0.18f + 0.16f * controls.variation;
            }
            if (steps_per_beat == 2) {
                return offbeat ? 0.66f + 0.20f * hat : 0.74f + 0.16f * hat;
            }
            if (sub_index == 0 || sub_index == 2) return 0.74f + 0.18f * hat;
            return 0.18f + 0.28f * hat * controls.density;

        case LANE_OPEN_HAT:
            if (offbeat) {
                switch (controls.genre) {
                    case GENRE_DISCO:
                    case GENRE_MOTORIK: return 0.34f + 0.32f * hat;
                    case GENRE_DUB: return 0.14f + 0.18f * hat;
                    default: return 0.18f + 0.20f * hat;
                }
            }
            if (fill_bar && beat_index == 3 && late_sub) return 0.16f + 0.14f * fill;
            break;

        case LANE_LOW_TOM:
            if (fill_bar && beat_index >= 2 && (offbeat || late_sub)) {
                return 0.08f + 0.28f * fill + 0.12f * tom;
            }
            break;

        case LANE_HIGH_TOM:
            if (fill_bar && beat_index >= 2 && !beat_start) {
                return 0.08f + 0.26f * fill + 0.12f * tom;
            }
            break;

        case LANE_BASH:
            switch (controls.genre) {
                case GENRE_ELECTRO:
                    if (beat_start && beat_index == 0) return 0.14f + 0.16f * metal;
                    if (fill_bar && beat_index >= 2 && (beat_start || late_sub)) return 0.10f + 0.26f * fill * metal;
                    if (late_sub && beat_index == 3) return 0.08f + 0.14f * controls.variation;
                    break;
                case GENRE_MOTORIK:
                    if (beat_start && beat_index == 0) return 0.12f + 0.18f * metal;
                    if (fill_bar && beat_index == 3 && beat_start) return 0.10f + 0.22f * fill * metal;
                    break;
                case GENRE_DUB:
                    if ((offbeat && beat_index == 3) || (late_sub && beat_index == 2)) return 0.10f + 0.18f * metal;
                    if (fill_bar && beat_index == 3) return 0.10f + 0.18f * fill * metal;
                    break;
                default:
                    if (fill_bar && beat_index >= 3 && (offbeat || late_sub)) return 0.06f + 0.18f * fill * metal;
                    break;
            }
            break;

        case LANE_COWBELL:
            switch (controls.genre) {
                case GENRE_DISCO:
                    if (offbeat) return 0.16f + 0.24f * perc;
                    if (beat_start && (beat_index == 1 || beat_index == 3)) return 0.08f + 0.14f * perc;
                    break;
                case GENRE_MOTORIK:
                    if (beat_start && (beat_index == 0 || beat_index == 2)) return 0.10f + 0.18f * perc;
                    if (offbeat) return 0.10f + 0.16f * perc;
                    break;
                case GENRE_BOSSA:
                    if ((beat_index == 0 || beat_index == 2) && late_sub) return 0.16f + 0.18f * perc;
                    if ((beat_index == 1 || beat_index == 3) && offbeat) return 0.16f + 0.20f * perc;
                    break;
                case GENRE_AFRO:
                    if (offbeat || late_sub) return 0.14f + 0.20f * perc;
                    break;
                default:
                    if (fill_bar && beat_index >= 2 && offbeat) return 0.06f + 0.16f * fill * perc;
                    break;
            }
            break;

        case LANE_CLAVE:
            switch (controls.genre) {
                case GENRE_BOSSA:
                    if (beat_index == 0 && beat_start) return 0.26f + 0.16f * perc;
                    if (beat_index == 1 && offbeat) return 0.22f + 0.16f * perc;
                    if (beat_index == 2 && late_sub) return 0.22f + 0.16f * perc;
                    if (beat_index == 3 && beat_start) return 0.24f + 0.16f * perc;
                    break;
                case GENRE_AFRO:
                    if (beat_index == 0 && beat_start) return 0.20f + 0.18f * perc;
                    if (beat_index == 1 && late_sub) return 0.18f + 0.18f * perc;
                    if (beat_index == 2 && offbeat) return 0.22f + 0.18f * perc;
                    if (beat_index == 3 && beat_start) return 0.18f + 0.16f * perc;
                    break;
                case GENRE_SHUFFLE:
                    if (beat_index == 1 && late_sub) return 0.10f + 0.14f * perc;
                    if (beat_index == 3 && offbeat) return 0.10f + 0.14f * perc;
                    break;
                default:
                    if (fill_bar && beat_index == 3 && !beat_start) return 0.06f + 0.14f * fill * perc;
                    break;
            }
            break;

        default:
            break;
    }

    return 0.0f;
}

int euclid_pulses_for_lane(const ControlSnapshot& controls,
                           int lane,
                           int steps_per_bar,
                           bool fill_bar,
                           DrumGenRng* rng) {
    const float density = controls.density;
    const float variation = controls.variation;
    const float fill = controls.fill;
    const float macro = lane_macro(controls, lane);

    float desired_hits = 0.0f;
    switch (lane) {
        case LANE_KICK:
            desired_hits = 1.0f + (controls.genre == GENRE_DISCO || controls.genre == GENRE_MOTORIK ? 3.0f : 1.6f) * density * macro;
            break;
        case LANE_CLAP:
            desired_hits = (controls.genre == GENRE_DISCO || controls.genre == GENRE_ELECTRO ? 1.6f : 0.8f) * density * macro;
            break;
        case LANE_SNARE:
            desired_hits = 1.0f + 1.0f * density * macro * (0.4f + 0.6f * variation);
            break;
        case LANE_CRASH:
            desired_hits = fill_bar ? (0.3f + 1.2f * fill * macro) : (0.15f + 0.35f * macro);
            break;
        case LANE_CLOSED_HAT:
            desired_hits = (steps_per_bar * (0.20f + 0.55f * density * macro)) + (steps_per_bar >= 16 ? 1.5f : 0.0f);
            break;
        case LANE_OPEN_HAT:
            desired_hits = 0.4f + 1.5f * density * macro;
            break;
        case LANE_LOW_TOM:
        case LANE_HIGH_TOM:
            desired_hits = fill_bar ? (0.4f + 2.4f * fill * macro) : 0.0f;
            break;
        case LANE_BASH:
            desired_hits = fill_bar
                ? (0.2f + 1.4f * fill * macro)
                : (((controls.genre == GENRE_ELECTRO) || (controls.genre == GENRE_MOTORIK) || (controls.genre == GENRE_DUB))
                    ? (0.15f + 0.75f * variation * macro)
                    : (0.05f + 0.35f * variation * macro));
            break;
        case LANE_COWBELL:
            if (controls.genre == GENRE_DISCO || controls.genre == GENRE_MOTORIK) {
                desired_hits = 0.8f + 3.0f * density * macro;
            } else if (controls.genre == GENRE_BOSSA || controls.genre == GENRE_AFRO) {
                desired_hits = 0.8f + 2.4f * density * macro;
            } else {
                desired_hits = 0.2f + 1.0f * density * variation * macro;
            }
            break;
        case LANE_CLAVE:
            if (controls.genre == GENRE_BOSSA || controls.genre == GENRE_AFRO) {
                desired_hits = 0.8f + 2.0f * density * macro;
            } else if (controls.genre == GENRE_SHUFFLE) {
                desired_hits = 0.4f + 1.4f * density * macro;
            } else {
                desired_hits = 0.15f + 0.8f * variation * macro;
            }
            break;
        default:
            break;
    }

    const int jitter = (int)lroundf((rng->next_float() * 2.0f - 1.0f) * (variation * 2.0f));
    return clampi((int)lroundf(desired_hits) + jitter, 0, steps_per_bar);
}

float euclid_influence_for_lane(const ControlSnapshot& controls, int lane, bool fill_bar) {
    switch (lane) {
        case LANE_KICK:
            return 0.10f + 0.35f * controls.variation * controls.kick_amt;
        case LANE_CLAP:
        case LANE_SNARE:
            return 0.10f + 0.32f * controls.variation * controls.backbeat_amt;
        case LANE_CRASH:
            return 0.10f + 0.28f * controls.variation * controls.metal_amt + (fill_bar ? 0.16f : 0.0f);
        case LANE_CLOSED_HAT:
            return 0.24f + 0.52f * controls.variation * controls.hat_amt;
        case LANE_OPEN_HAT:
            return 0.16f + 0.42f * controls.variation * controls.hat_amt;
        case LANE_LOW_TOM:
        case LANE_HIGH_TOM:
            return 0.10f + 0.44f * controls.variation * controls.tom_amt + (fill_bar ? 0.24f : 0.0f);
        case LANE_BASH:
            return 0.10f + 0.36f * controls.variation * controls.metal_amt + (fill_bar ? 0.28f : 0.0f);
        case LANE_COWBELL:
            return 0.18f + 0.34f * controls.variation * controls.aux_amt;
        case LANE_CLAVE:
            return 0.16f + 0.32f * controls.variation * controls.aux_amt + (fill_bar ? 0.10f : 0.0f);
        default:
            return 0.20f;
    }
}

int step_velocity(const ControlSnapshot& controls,
                  int lane,
                  int beat_index,
                  int sub_index,
                  int steps_per_beat,
                  bool fill_bar,
                  DrumGenRng* rng,
                  uint8_t* flags_out) {
    int velocity = 90;
    uint8_t flags = 0;

    switch (lane) {
        case LANE_KICK: velocity = 108; break;
        case LANE_CLAP: velocity = 104; break;
        case LANE_SNARE: velocity = 100; break;
        case LANE_CRASH: velocity = 96; break;
        case LANE_CLOSED_HAT: velocity = 82; break;
        case LANE_OPEN_HAT: velocity = 76; break;
        case LANE_LOW_TOM: velocity = 92; break;
        case LANE_HIGH_TOM: velocity = 94; break;
        case LANE_BASH: velocity = 110; break;
        case LANE_COWBELL: velocity = 88; break;
        case LANE_CLAVE: velocity = 90; break;
        default: break;
    }

    if (sub_index == 0) {
        velocity += 8;
    } else {
        velocity -= 4;
    }

    if ((lane == LANE_SNARE || lane == LANE_CLAP) && (beat_index == 1 || beat_index == 3) && sub_index == 0) {
        velocity += 10;
        flags |= STEP_FLAG_ACCENT;
    }
    if (lane == LANE_KICK && beat_index == 0 && sub_index == 0) {
        velocity += 8;
        flags |= STEP_FLAG_ACCENT;
    }
    if ((lane == LANE_COWBELL || lane == LANE_CLAVE) && sub_index != 0) {
        velocity += 4;
    }
    if (lane == LANE_BASH && (beat_index == 0 || fill_bar)) {
        velocity += 6;
        flags |= STEP_FLAG_ACCENT;
    }
    if (fill_bar && (lane == LANE_CRASH || lane == LANE_LOW_TOM || lane == LANE_HIGH_TOM)) {
        velocity += 6;
        flags |= STEP_FLAG_FILL;
    }
    if (fill_bar && (lane == LANE_BASH || lane == LANE_COWBELL || lane == LANE_CLAVE)) {
        velocity += 4;
        flags |= STEP_FLAG_FILL;
    }

    velocity += rng->next_int(-6, 6);
    velocity = clampi(velocity, 1, 127);

    if (flags_out) {
        *flags_out = flags;
    }
    return velocity;
}

void clear_step_hit(PatternStateBlob* pattern, int lane, int step) {
    if (!pattern || lane < 0 || lane >= DRUMGEN_LANE_COUNT || step < 0 || step >= pattern->total_steps) {
        return;
    }
    pattern->lanes[lane].steps[step].velocity = 0;
    pattern->lanes[lane].steps[step].flags = 0;
}

void set_step_hit(PatternStateBlob* pattern, int lane, int step, int velocity, uint8_t flags) {
    if (!pattern || lane < 0 || lane >= DRUMGEN_LANE_COUNT || step < 0 || step >= pattern->total_steps) {
        return;
    }
    DrumStepCell* cell = &pattern->lanes[lane].steps[step];
    if (velocity > cell->velocity) {
        cell->velocity = (uint8_t)clampi(velocity, 1, 127);
    }
    cell->flags |= flags;
}

void clear_bar_hits(PatternStateBlob* pattern, int bar_index) {
    if (!pattern || bar_index < 0 || bar_index >= pattern->bars) {
        return;
    }

    const int bar_start = bar_index * pattern->steps_per_bar;
    const int bar_end = clampi(bar_start + pattern->steps_per_bar, 0, pattern->total_steps);
    for (int lane = 0; lane < DRUMGEN_LANE_COUNT; ++lane) {
        for (int step = bar_start; step < bar_end; ++step) {
            pattern->lanes[lane].steps[step].velocity = 0;
            pattern->lanes[lane].steps[step].flags = 0;
        }
    }
}

void clear_pattern(PatternStateBlob* pattern, const ControlSnapshot& controls) {
    memset(pattern, 0, sizeof(PatternStateBlob));
    pattern->version = DRUMGEN_PATTERN_STATE_VERSION;
    pattern->bars = controls.bars;
    pattern->steps_per_beat = drumgen_steps_per_beat_for_resolution(controls.resolution);
    pattern->steps_per_bar = pattern->steps_per_beat * 4;
    pattern->total_steps = clampi(pattern->bars * pattern->steps_per_bar, 1, DRUMGEN_MAX_PATTERN_STEPS);
    for (int lane = 0; lane < DRUMGEN_LANE_COUNT; ++lane) {
        pattern->lanes[lane].midi_note = note_for_lane(controls.kit_map, lane);
    }
}

void copy_pattern_prefix(PatternStateBlob* target, const PatternStateBlob* source, int prefix_steps) {
    if (!target || !source || prefix_steps <= 0) {
        return;
    }
    for (int lane = 0; lane < DRUMGEN_LANE_COUNT; ++lane) {
        memcpy(target->lanes[lane].steps, source->lanes[lane].steps, (size_t)prefix_steps * sizeof(DrumStepCell));
    }
}

void apply_fill_overlay(PatternStateBlob* pattern, const ControlSnapshot& controls, uint32_t seed_value) {
    if (!pattern || pattern->bars <= 0 || pattern->total_steps <= 0 || controls.fill < 0.08f) {
        return;
    }

    DrumGenRng rng;
    const int steps_per_bar = pattern->steps_per_bar;
    const int steps_per_beat = pattern->steps_per_beat;
    const int last_bar = pattern->bars - 1;
    const int bar_start = last_bar * steps_per_bar;
    const int bar_end = clampi(bar_start + steps_per_bar, 0, pattern->total_steps);
    const int fill_beats = controls.fill > 0.62f ? 2 : 1;
    const int fill_steps = clampi(fill_beats * steps_per_beat, steps_per_beat, steps_per_bar);
    const int zone_start = clampi(bar_end - fill_steps, bar_start, bar_end);
    int motif = 0;

    rng.seed(seed_value ^ 0xC001D00Du);

    switch (controls.genre) {
        case GENRE_ROCK:
        case GENRE_SHUFFLE:
            motif = rng.next_int(0, 1);
            break;
        case GENRE_DISCO:
        case GENRE_MOTORIK:
            motif = 2 + rng.next_int(0, 1);
            break;
        case GENRE_ELECTRO:
        case GENRE_DUB:
            motif = 1 + rng.next_int(0, 2);
            break;
        case GENRE_BOSSA:
        case GENRE_AFRO:
            motif = (rng.next_float() < 0.65f) ? 2 : 3;
            break;
        default:
            motif = rng.next_int(0, 3);
            break;
    }

    for (int step = zone_start; step < bar_end; ++step) {
        clear_step_hit(pattern, LANE_OPEN_HAT, step);
        clear_step_hit(pattern, LANE_CRASH, step);
        clear_step_hit(pattern, LANE_BASH, step);
        if (controls.fill > 0.45f) {
            if ((step - zone_start) % 2 == 1) {
                clear_step_hit(pattern, LANE_CLOSED_HAT, step);
            }
            if ((step % steps_per_beat) != 0 && controls.fill > 0.60f) {
                clear_step_hit(pattern, LANE_KICK, step);
            }
        }
    }

    switch (motif) {
        case 0: {
            int index = 0;
            const int stride = controls.fill > 0.65f ? 1 : 2;
            for (int step = zone_start; step < bar_end; step += stride) {
                const int lane = (index % 2 == 0) ? LANE_LOW_TOM : LANE_HIGH_TOM;
                set_step_hit(pattern, lane, step, 92 + index * 5, STEP_FLAG_FILL | (index > 1 ? STEP_FLAG_ACCENT : 0));
                index += 1;
            }
            set_step_hit(pattern,
                         (controls.genre == GENRE_ELECTRO || controls.genre == GENRE_MOTORIK) ? LANE_BASH : LANE_CRASH,
                         bar_end - 1,
                         114,
                         STEP_FLAG_FILL | STEP_FLAG_ACCENT);
            break;
        }

        case 1: {
            int rise = 0;
            for (int step = zone_start; step < bar_end; ++step) {
                set_step_hit(pattern, LANE_SNARE, step, 88 + rise * 4, STEP_FLAG_FILL);
                if ((step - zone_start) % 2 == 1) {
                    set_step_hit(pattern, LANE_CLAP, step, 84 + rise * 3, STEP_FLAG_FILL);
                }
                rise += 1;
            }
            set_step_hit(pattern, LANE_HIGH_TOM, clampi(bar_end - 2, zone_start, bar_end - 1), 108, STEP_FLAG_FILL | STEP_FLAG_ACCENT);
            if (controls.metal_amt > 0.20f) {
                set_step_hit(pattern, LANE_BASH, bar_end - 1, 116, STEP_FLAG_FILL | STEP_FLAG_ACCENT);
            }
            break;
        }

        case 2: {
            int toggle = 0;
            for (int step = zone_start; step < bar_end; ++step) {
                const int step_in_bar = step - bar_start;
                if (is_offbeat_step(step_in_bar, steps_per_beat) || ((step - zone_start) % 2 == 0)) {
                    set_step_hit(pattern, toggle % 2 == 0 ? LANE_COWBELL : LANE_CLAVE, step, 90 + toggle * 2, STEP_FLAG_FILL);
                    toggle += 1;
                }
            }
            set_step_hit(pattern, LANE_LOW_TOM, zone_start, 96, STEP_FLAG_FILL);
            set_step_hit(pattern, LANE_HIGH_TOM, clampi(bar_end - 2, zone_start, bar_end - 1), 104, STEP_FLAG_FILL | STEP_FLAG_ACCENT);
            break;
        }

        case 3:
        default: {
            const int bash_step = clampi(zone_start + fill_steps / 2, zone_start, bar_end - 1);
            int index = 0;
            set_step_hit(pattern, LANE_BASH, bash_step, 114, STEP_FLAG_FILL | STEP_FLAG_ACCENT);
            for (int step = zone_start; step < bar_end; step += 2) {
                const int lane = (index % 3 == 0) ? LANE_HIGH_TOM : ((index % 3 == 1) ? LANE_COWBELL : LANE_CLAVE);
                set_step_hit(pattern, lane, step, 92 + index * 4, STEP_FLAG_FILL);
                index += 1;
            }
            set_step_hit(pattern, LANE_CLAVE, clampi(bar_end - 2, zone_start, bar_end - 1), 98, STEP_FLAG_FILL);
            set_step_hit(pattern, LANE_HIGH_TOM, bar_end - 1, 108, STEP_FLAG_FILL | STEP_FLAG_ACCENT);
            break;
        }
    }
}

void build_bar(PatternStateBlob* pattern,
               const ControlSnapshot& controls,
               int bar_index,
               bool fill_bar,
               uint32_t seed_value) {
    const int steps_per_bar = pattern->steps_per_bar;
    const int steps_per_beat = pattern->steps_per_beat;
    DrumGenRng rng;
    rng.seed(seed_value);

    int pulses[DRUMGEN_LANE_COUNT];
    int offsets[DRUMGEN_LANE_COUNT];
    for (int lane = 0; lane < DRUMGEN_LANE_COUNT; ++lane) {
        pulses[lane] = euclid_pulses_for_lane(controls, lane, steps_per_bar, fill_bar, &rng);
        offsets[lane] = pulses[lane] > 0 ? rng.next_int(0, steps_per_bar - 1) : 0;
    }

    for (int step = 0; step < steps_per_bar; ++step) {
        const int global_step = bar_index * steps_per_bar + step;
        if (global_step >= pattern->total_steps) {
            break;
        }

        const int beat_index = step / steps_per_beat;
        const int sub_index = step % steps_per_beat;

        for (int lane = 0; lane < DRUMGEN_LANE_COUNT; ++lane) {
            const float anchor_prob = anchor_probability(controls, lane, bar_index, beat_index, sub_index, steps_per_beat, fill_bar);
            const bool anchor = anchor_prob > 0.0f && rng.next_float() < clampf(anchor_prob, 0.0f, 1.0f);
            const bool euclid = euclid_hit(step, pulses[lane], offsets[lane], steps_per_bar);
            bool hit = anchor;

            if (!hit && euclid) {
                const float chance = clampf(euclid_influence_for_lane(controls, lane, fill_bar), 0.0f, 1.0f);
                hit = rng.next_float() < chance;
            }

            if (!hit) {
                continue;
            }

            uint8_t flags = 0;
            const int velocity = step_velocity(controls, lane, beat_index, sub_index, steps_per_beat, fill_bar, &rng, &flags);
            pattern->lanes[lane].steps[global_step].velocity = (uint8_t)velocity;
            pattern->lanes[lane].steps[global_step].flags = flags;
        }
    }
}

void cleanup_pattern(PatternStateBlob* pattern, const ControlSnapshot& controls) {
    if (!pattern || pattern->total_steps <= 0) {
        return;
    }

    const int steps_per_bar = pattern->steps_per_bar;
    const int steps_per_beat = pattern->steps_per_beat;

    for (int step = 0; step < pattern->total_steps; ++step) {
        DrumStepCell& closed = pattern->lanes[LANE_CLOSED_HAT].steps[step];
        DrumStepCell& open = pattern->lanes[LANE_OPEN_HAT].steps[step];
        DrumStepCell& cowbell = pattern->lanes[LANE_COWBELL].steps[step];
        DrumStepCell& clave = pattern->lanes[LANE_CLAVE].steps[step];
        DrumStepCell& clap = pattern->lanes[LANE_CLAP].steps[step];
        DrumStepCell& snare = pattern->lanes[LANE_SNARE].steps[step];

        if (closed.velocity > 0 && open.velocity > 0) {
            if (is_offbeat_step(step % steps_per_bar, steps_per_beat)) {
                closed.velocity = 0;
                closed.flags = 0;
            } else {
                open.velocity = 0;
                open.flags = 0;
            }
        }
        if (cowbell.velocity > 0 && clave.velocity > 0) {
            if (controls.genre == GENRE_BOSSA || controls.genre == GENRE_AFRO) {
                cowbell.velocity = 0;
                cowbell.flags = 0;
            } else {
                clave.velocity = 0;
                clave.flags = 0;
            }
        }
        if (snare.velocity > 0 && clap.velocity > 0 && controls.genre != GENRE_DISCO && controls.genre != GENRE_ELECTRO) {
            clap.velocity = 0;
            clap.flags = 0;
        }
    }

    for (int bar = 0; bar < pattern->bars; ++bar) {
        const int bar_start = bar * steps_per_bar;
        int crash_hits = 0;
        int bash_hits = 0;
        for (int step = 0; step < steps_per_bar && (bar_start + step) < pattern->total_steps; ++step) {
            DrumStepCell& crash = pattern->lanes[LANE_CRASH].steps[bar_start + step];
            DrumStepCell& bash = pattern->lanes[LANE_BASH].steps[bar_start + step];
            if (crash.velocity > 0) {
                crash_hits += 1;
                if (crash_hits > 2) {
                    crash.velocity = 0;
                    crash.flags = 0;
                }
            }
            if (bash.velocity > 0) {
                bash_hits += 1;
                if (bash_hits > 2) {
                    bash.velocity = 0;
                    bash.flags = 0;
                }
            }

            if (is_fill_zone_step(step, steps_per_bar, steps_per_beat, controls.fill)) {
                const bool fill_activity =
                    pattern->lanes[LANE_LOW_TOM].steps[bar_start + step].velocity > 0 ||
                    pattern->lanes[LANE_HIGH_TOM].steps[bar_start + step].velocity > 0 ||
                    pattern->lanes[LANE_BASH].steps[bar_start + step].velocity > 0 ||
                    pattern->lanes[LANE_COWBELL].steps[bar_start + step].velocity > 0 ||
                    pattern->lanes[LANE_CLAVE].steps[bar_start + step].velocity > 0;

                if (fill_activity) {
                    clear_step_hit(pattern, LANE_OPEN_HAT, bar_start + step);
                    if ((step - (steps_per_bar - steps_per_beat)) % 2 != 0) {
                        clear_step_hit(pattern, LANE_CLOSED_HAT, bar_start + step);
                    }
                }
            }
        }

        const int backbeat1 = bar_start + steps_per_beat;
        const int backbeat2 = bar_start + steps_per_beat * 3;
        const bool needs_backbeat = controls.backbeat_amt > 0.30f;
        if (needs_backbeat) {
            if (backbeat1 < pattern->total_steps &&
                pattern->lanes[LANE_SNARE].steps[backbeat1].velocity == 0 &&
                pattern->lanes[LANE_CLAP].steps[backbeat1].velocity == 0) {
                pattern->lanes[LANE_SNARE].steps[backbeat1].velocity = 102;
                pattern->lanes[LANE_SNARE].steps[backbeat1].flags = STEP_FLAG_ACCENT;
            }
            if (backbeat2 < pattern->total_steps &&
                pattern->lanes[LANE_SNARE].steps[backbeat2].velocity == 0 &&
                pattern->lanes[LANE_CLAP].steps[backbeat2].velocity == 0) {
                pattern->lanes[LANE_SNARE].steps[backbeat2].velocity = 104;
                pattern->lanes[LANE_SNARE].steps[backbeat2].flags = STEP_FLAG_ACCENT;
            }
        }
    }
}

}  // namespace

ControlSnapshot drumgen_clamp_controls(const ControlSnapshot& raw) {
    ControlSnapshot controls = raw;
    controls.genre = clampi(controls.genre, 0, GENRE_COUNT - 1);
    controls.channel = clampi(controls.channel, 1, 16);
    controls.kit_map = clampi(controls.kit_map, 0, KIT_MAP_COUNT - 1);
    controls.bars = clampi(controls.bars, DRUMGEN_MIN_BARS, DRUMGEN_MAX_BARS);
    controls.resolution = clampi(controls.resolution, 0, RESOLUTION_COUNT - 1);
    controls.density = clampf(controls.density, 0.0f, 1.0f);
    controls.variation = clampf(controls.variation, 0.0f, 1.0f);
    controls.fill = clampf(controls.fill, 0.0f, 1.0f);
    controls.vary = clampf(controls.vary, 0.0f, 1.0f);
    controls.kick_amt = clampf(controls.kick_amt, 0.0f, 1.0f);
    controls.backbeat_amt = clampf(controls.backbeat_amt, 0.0f, 1.0f);
    controls.hat_amt = clampf(controls.hat_amt, 0.0f, 1.0f);
    controls.aux_amt = clampf(controls.aux_amt, 0.0f, 1.0f);
    controls.tom_amt = clampf(controls.tom_amt, 0.0f, 1.0f);
    controls.metal_amt = clampf(controls.metal_amt, 0.0f, 1.0f);
    controls.action_new = clampi(controls.action_new, 0, 1048576);
    controls.action_mutate = clampi(controls.action_mutate, 0, 1048576);
    controls.action_fill = clampi(controls.action_fill, 0, 1048576);
    return controls;
}

bool drumgen_structural_controls_changed(const ControlSnapshot& a, const ControlSnapshot& b) {
    return a.genre != b.genre ||
           a.channel != b.channel ||
           a.kit_map != b.kit_map ||
           a.bars != b.bars ||
           a.resolution != b.resolution ||
           fabsf(a.density - b.density) >= 0.0001f ||
           fabsf(a.variation - b.variation) >= 0.0001f ||
           fabsf(a.fill - b.fill) >= 0.0001f ||
           a.seed != b.seed ||
           fabsf(a.kick_amt - b.kick_amt) >= 0.0001f ||
           fabsf(a.backbeat_amt - b.backbeat_amt) >= 0.0001f ||
           fabsf(a.hat_amt - b.hat_amt) >= 0.0001f ||
           fabsf(a.aux_amt - b.aux_amt) >= 0.0001f ||
           fabsf(a.tom_amt - b.tom_amt) >= 0.0001f ||
           fabsf(a.metal_amt - b.metal_amt) >= 0.0001f;
}

int drumgen_steps_per_beat_for_resolution(int resolution) {
    switch (resolution) {
        case RESOLUTION_8TH: return 2;
        case RESOLUTION_16T: return 3;
        case RESOLUTION_16TH:
        default: return 4;
    }
}

void drumgen_regenerate_pattern(PatternStateBlob* pattern,
                                const ControlSnapshot& controls,
                                bool fill_only_refresh) {
    if (!pattern) {
        return;
    }

    const PatternStateBlob previous_pattern = *pattern;
    const bool had_previous_pattern = previous_pattern.total_steps > 0;
    PatternStateBlob next_pattern{};
    clear_pattern(&next_pattern, controls);

    const int32_t next_serial = next_generation_serial(previous_pattern.generation_serial);
    next_pattern.generation_serial = next_serial;

    const bool compatible_shape = had_previous_pattern &&
                                  previous_pattern.bars == next_pattern.bars &&
                                  previous_pattern.steps_per_bar == next_pattern.steps_per_bar &&
                                  previous_pattern.total_steps == next_pattern.total_steps;

    if (fill_only_refresh && compatible_shape && next_pattern.total_steps > next_pattern.steps_per_bar) {
        const int prefix_steps = next_pattern.total_steps - next_pattern.steps_per_bar;
        copy_pattern_prefix(&next_pattern, &previous_pattern, prefix_steps);
        const int last_bar = next_pattern.bars - 1;
        build_bar(&next_pattern,
                  controls,
                  last_bar,
                  true,
                  bar_seed_for_serial(controls, next_serial, last_bar, true));
    } else {
        for (int bar = 0; bar < next_pattern.bars; ++bar) {
            const bool fill_bar = (bar == next_pattern.bars - 1);
            build_bar(&next_pattern,
                      controls,
                      bar,
                      fill_bar,
                      bar_seed_for_serial(controls, next_serial, bar, fill_bar));
        }
    }

    apply_fill_overlay(&next_pattern,
                       controls,
                       base_seed_for_serial(controls, next_serial) ^
                           fill_seed_for_serial(controls, next_serial) ^
                           0x6D2B79F5u);
    cleanup_pattern(&next_pattern, controls);

    next_pattern.version = DRUMGEN_PATTERN_STATE_VERSION;
    *pattern = next_pattern;
}

void drumgen_refresh_bar(PatternStateBlob* pattern,
                         const ControlSnapshot& controls,
                         int bar_index) {
    if (!pattern ||
        pattern->bars <= 0 ||
        pattern->total_steps <= 0 ||
        pattern->bars != controls.bars ||
        pattern->steps_per_beat != drumgen_steps_per_beat_for_resolution(controls.resolution)) {
        drumgen_regenerate_pattern(pattern, controls, false);
        return;
    }

    const int clamped_bar = clampi(bar_index, 0, pattern->bars - 1);
    const int32_t next_serial = next_generation_serial(pattern->generation_serial);
    PatternStateBlob next_pattern = *pattern;
    next_pattern.generation_serial = next_serial;
    next_pattern.version = DRUMGEN_PATTERN_STATE_VERSION;

    for (int lane = 0; lane < DRUMGEN_LANE_COUNT; ++lane) {
        next_pattern.lanes[lane].midi_note = note_for_lane(controls.kit_map, lane);
    }

    clear_bar_hits(&next_pattern, clamped_bar);
    build_bar(&next_pattern,
              controls,
              clamped_bar,
              clamped_bar == next_pattern.bars - 1,
              bar_seed_for_serial(controls, next_serial, clamped_bar, clamped_bar == next_pattern.bars - 1));

    if (clamped_bar == next_pattern.bars - 1) {
        apply_fill_overlay(&next_pattern,
                           controls,
                           base_seed_for_serial(controls, next_serial) ^
                               fill_seed_for_serial(controls, next_serial) ^
                               0x6D2B79F5u);
    }

    cleanup_pattern(&next_pattern, controls);
    *pattern = next_pattern;
}
