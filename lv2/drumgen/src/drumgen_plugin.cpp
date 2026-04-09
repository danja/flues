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

#define DRUMGEN_URI "https://danja.github.io/flues/plugins/drumgen"

static constexpr int kMinBars = 1;
static constexpr int kMaxBars = 4;
static constexpr int kLaneCount = 8;
static constexpr int kMaxPatternSteps = 64;
static constexpr int kMaxPendingNoteOffs = 64;
static constexpr int kSafetyGapSamples = 1;

enum PortIndex {
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
    PORT_TOTAL_COUNT
};

enum GenreId {
    GENRE_ROCK = 0,
    GENRE_DISCO,
    GENRE_SHUFFLE,
    GENRE_ELECTRO,
    GENRE_DUB,
    GENRE_MOTORIK,
    GENRE_BOSSA,
    GENRE_AFRO,
    GENRE_COUNT
};

enum ResolutionId {
    RESOLUTION_8TH = 0,
    RESOLUTION_16TH,
    RESOLUTION_16T,
    RESOLUTION_COUNT
};

enum KitMapId {
    KIT_MAP_FLUES_DRUMKIT = 0,
    KIT_MAP_GM,
    KIT_MAP_COUNT
};

enum LaneId {
    LANE_KICK = 0,
    LANE_CLAP,
    LANE_SNARE,
    LANE_CRASH,
    LANE_CLOSED_HAT,
    LANE_LOW_TOM,
    LANE_OPEN_HAT,
    LANE_HIGH_TOM
};

enum StepFlags {
    STEP_FLAG_ACCENT = 1 << 0,
    STEP_FLAG_FILL = 1 << 1
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
    int genre = GENRE_ROCK;
    int channel = 10;
    int kit_map = KIT_MAP_FLUES_DRUMKIT;
    int bars = 2;
    int resolution = RESOLUTION_16TH;
    float density = 0.58f;
    float variation = 0.35f;
    float fill = 0.30f;
    uint32_t seed = 1;
    float kick_amt = 0.78f;
    float backbeat_amt = 0.76f;
    float hat_amt = 0.82f;
    float aux_amt = 0.28f;
    int action_new = 0;
    int action_mutate = 0;
    int action_fill = 0;
};

struct DrumStepCell {
    uint8_t velocity = 0;
    uint8_t flags = 0;
};

struct DrumLaneState {
    int32_t midi_note = 0;
    DrumStepCell steps[kMaxPatternSteps];
};

struct PatternStateBlob {
    int32_t version = 1;
    int32_t bars = 0;
    int32_t steps_per_beat = 4;
    int32_t steps_per_bar = 16;
    int32_t total_steps = 0;
    int32_t generation_serial = 0;
    DrumLaneState lanes[kLaneCount];
};

struct PendingNoteOff {
    bool active = false;
    int note = 0;
    int channel = 10;
    int remaining_samples = 0;
};

struct DrumGenURIDs {
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

struct DrumGen {
    const LV2_Atom_Sequence* control = nullptr;
    LV2_Atom_Sequence* midi_out = nullptr;

    const float* genre_port = nullptr;
    const float* channel_port = nullptr;
    const float* kit_map_port = nullptr;
    const float* bars_port = nullptr;
    const float* resolution_port = nullptr;
    const float* density_port = nullptr;
    const float* variation_port = nullptr;
    const float* fill_port = nullptr;
    const float* seed_port = nullptr;
    const float* kick_amt_port = nullptr;
    const float* backbeat_amt_port = nullptr;
    const float* hat_amt_port = nullptr;
    const float* aux_amt_port = nullptr;
    const float* action_new_port = nullptr;
    const float* action_mutate_port = nullptr;
    const float* action_fill_port = nullptr;

    LV2_URID_Map* map = nullptr;
    DrumGenURIDs urids{};
    double sample_rate = 48000.0;

    ControlSnapshot controls{};
    ControlSnapshot previous_controls{};
    PatternStateBlob pattern{};
    bool pattern_valid = false;

    PendingNoteOff pending_note_offs[kMaxPendingNoteOffs];
    int64_t last_transport_step = -1;
    bool was_playing = false;
};

static const int kFluesDrumkitNotes[kLaneCount] = {36, 39, 40, 41, 42, 45, 46, 50};
static const int kGMNotes[kLaneCount] = {36, 39, 38, 49, 42, 45, 46, 50};

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

static bool atom_to_double(const LV2_Atom* atom, const DrumGenURIDs& urids, double* out) {
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

static void read_time_info(const LV2_Atom_Sequence* control, const DrumGenURIDs& urids, TimeInfo* info) {
    if (!control || !info) {
        return;
    }

    LV2_ATOM_SEQUENCE_FOREACH(control, ev) {
        if (ev->body.type != urids.atom_Object && ev->body.type != urids.atom_Blank) {
            continue;
        }

        const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;
        if (obj->body.otype != urids.time_Position) {
            continue;
        }

        info->valid = true;
        const LV2_Atom* speed_atom = nullptr;
        const LV2_Atom* bar_atom = nullptr;
        const LV2_Atom* bar_beat_atom = nullptr;
        const LV2_Atom* beats_per_bar_atom = nullptr;
        const LV2_Atom* bpm_atom = nullptr;

        lv2_atom_object_get(obj,
                            urids.time_speed, &speed_atom,
                            urids.time_bar, &bar_atom,
                            urids.time_barBeat, &bar_beat_atom,
                            urids.time_beatsPerBar, &beats_per_bar_atom,
                            urids.time_beatsPerMinute, &bpm_atom,
                            0);

        double value = 0.0;
        if (atom_to_double(speed_atom, urids, &value)) {
            info->playing = value > 0.0;
        }
        if (atom_to_double(bar_atom, urids, &value)) {
            info->bar = value;
        }
        if (atom_to_double(bar_beat_atom, urids, &value)) {
            info->barBeat = value;
        }
        if (atom_to_double(beats_per_bar_atom, urids, &value) && value > 0.0) {
            info->beatsPerBar = value;
        }
        if (atom_to_double(bpm_atom, urids, &value) && value > 0.0) {
            info->bpm = value;
        }
    }
}

static int steps_per_beat_for_resolution(int resolution) {
    switch (resolution) {
        case RESOLUTION_8TH: return 2;
        case RESOLUTION_16T: return 3;
        case RESOLUTION_16TH:
        default: return 4;
    }
}

static bool euclid_hit(int step, int pulses, int offset, int length) {
    if (length <= 0 || pulses <= 0) {
        return false;
    }
    if (pulses >= length) {
        return true;
    }

    const int base_step = (step - offset + length) % length;
    return ((base_step * pulses) % length) < pulses;
}

static bool controls_match_excluding_actions(const ControlSnapshot& a, const ControlSnapshot& b) {
    return a.genre == b.genre &&
           a.channel == b.channel &&
           a.kit_map == b.kit_map &&
           a.bars == b.bars &&
           a.resolution == b.resolution &&
           fabsf(a.density - b.density) < 0.0001f &&
           fabsf(a.variation - b.variation) < 0.0001f &&
           fabsf(a.fill - b.fill) < 0.0001f &&
           a.seed == b.seed &&
           fabsf(a.kick_amt - b.kick_amt) < 0.0001f &&
           fabsf(a.backbeat_amt - b.backbeat_amt) < 0.0001f &&
           fabsf(a.hat_amt - b.hat_amt) < 0.0001f &&
           fabsf(a.aux_amt - b.aux_amt) < 0.0001f;
}

static int note_for_lane(int kit_map, int lane) {
    const int* table = (kit_map == KIT_MAP_GM) ? kGMNotes : kFluesDrumkitNotes;
    return table[clampi(lane, 0, kLaneCount - 1)];
}

static float lane_macro(const ControlSnapshot& controls, int lane) {
    switch (lane) {
        case LANE_KICK: return controls.kick_amt;
        case LANE_CLAP:
        case LANE_SNARE: return controls.backbeat_amt;
        case LANE_CLOSED_HAT:
        case LANE_OPEN_HAT: return controls.hat_amt;
        case LANE_CRASH:
        case LANE_LOW_TOM:
        case LANE_HIGH_TOM:
        default: return controls.aux_amt;
    }
}

static bool is_offbeat_step(int step_in_bar, int steps_per_beat) {
    if (steps_per_beat <= 0) {
        return false;
    }
    if (steps_per_beat == 3) {
        return (step_in_bar % steps_per_beat) == 2;
    }
    return (step_in_bar % steps_per_beat) == (steps_per_beat / 2);
}

static float anchor_probability(const ControlSnapshot& controls,
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
    const float aux = controls.aux_amt;
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
            break;

        case LANE_CRASH:
            if (beat_start && beat_index == 0) {
                return (bar_index == 0 ? 0.34f : 0.16f) + 0.18f * aux;
            }
            if (fill_bar && beat_start && beat_index >= 2) {
                return 0.08f + 0.18f * fill;
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
                return 0.08f + 0.28f * fill + 0.12f * aux;
            }
            break;

        case LANE_HIGH_TOM:
            if (fill_bar && beat_index >= 2 && !beat_start) {
                return 0.08f + 0.26f * fill + 0.12f * aux;
            }
            break;

        default:
            break;
    }

    return 0.0f;
}

static int euclid_pulses_for_lane(const ControlSnapshot& controls,
                                  int lane,
                                  int steps_per_bar,
                                  bool fill_bar,
                                  Rng* rng) {
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
        default:
            break;
    }

    const int jitter = (int)lroundf((rng->next_float() * 2.0f - 1.0f) * (variation * 2.0f));
    return clampi((int)lroundf(desired_hits) + jitter, 0, steps_per_bar);
}

static float euclid_influence_for_lane(const ControlSnapshot& controls, int lane, bool fill_bar) {
    switch (lane) {
        case LANE_KICK:
            return 0.10f + 0.35f * controls.variation * controls.kick_amt;
        case LANE_CLAP:
        case LANE_SNARE:
            return 0.10f + 0.32f * controls.variation * controls.backbeat_amt;
        case LANE_CRASH:
            return 0.10f + 0.28f * controls.variation * controls.aux_amt + (fill_bar ? 0.16f : 0.0f);
        case LANE_CLOSED_HAT:
            return 0.24f + 0.52f * controls.variation * controls.hat_amt;
        case LANE_OPEN_HAT:
            return 0.16f + 0.42f * controls.variation * controls.hat_amt;
        case LANE_LOW_TOM:
        case LANE_HIGH_TOM:
            return 0.10f + 0.44f * controls.variation * controls.aux_amt + (fill_bar ? 0.24f : 0.0f);
        default:
            return 0.20f;
    }
}

static int step_velocity(const ControlSnapshot& controls,
                         int lane,
                         int beat_index,
                         int sub_index,
                         int steps_per_beat,
                         bool fill_bar,
                         Rng* rng,
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
    if (fill_bar && (lane == LANE_CRASH || lane == LANE_LOW_TOM || lane == LANE_HIGH_TOM)) {
        velocity += 6;
        flags |= STEP_FLAG_FILL;
    }

    velocity += rng->next_int(-6, 6);
    velocity = clampi(velocity, 1, 127);

    if (flags_out) {
        *flags_out = flags;
    }
    return velocity;
}

static void clear_pattern(PatternStateBlob* pattern, const ControlSnapshot& controls) {
    memset(pattern, 0, sizeof(PatternStateBlob));
    pattern->version = 1;
    pattern->bars = controls.bars;
    pattern->steps_per_beat = steps_per_beat_for_resolution(controls.resolution);
    pattern->steps_per_bar = pattern->steps_per_beat * 4;
    pattern->total_steps = clampi(pattern->bars * pattern->steps_per_bar, 1, kMaxPatternSteps);
    for (int lane = 0; lane < kLaneCount; ++lane) {
        pattern->lanes[lane].midi_note = note_for_lane(controls.kit_map, lane);
    }
}

static void copy_pattern_prefix(PatternStateBlob* target, const PatternStateBlob* source, int prefix_steps) {
    if (!target || !source || prefix_steps <= 0) {
        return;
    }
    for (int lane = 0; lane < kLaneCount; ++lane) {
        memcpy(target->lanes[lane].steps, source->lanes[lane].steps, (size_t)prefix_steps * sizeof(DrumStepCell));
    }
}

static void build_bar(PatternStateBlob* pattern,
                      const ControlSnapshot& controls,
                      int bar_index,
                      bool fill_bar,
                      uint32_t seed_value) {
    const int steps_per_bar = pattern->steps_per_bar;
    const int steps_per_beat = pattern->steps_per_beat;
    Rng rng;
    rng.seed(seed_value);

    int pulses[kLaneCount];
    int offsets[kLaneCount];
    for (int lane = 0; lane < kLaneCount; ++lane) {
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

        for (int lane = 0; lane < kLaneCount; ++lane) {
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

static void cleanup_pattern(PatternStateBlob* pattern, const ControlSnapshot& controls) {
    if (!pattern || pattern->total_steps <= 0) {
        return;
    }

    const int steps_per_bar = pattern->steps_per_bar;
    const int steps_per_beat = pattern->steps_per_beat;

    for (int step = 0; step < pattern->total_steps; ++step) {
        DrumStepCell& closed = pattern->lanes[LANE_CLOSED_HAT].steps[step];
        DrumStepCell& open = pattern->lanes[LANE_OPEN_HAT].steps[step];
        if (closed.velocity > 0 && open.velocity > 0) {
            if (is_offbeat_step(step % steps_per_bar, steps_per_beat)) {
                closed.velocity = 0;
                closed.flags = 0;
            } else {
                open.velocity = 0;
                open.flags = 0;
            }
        }
    }

    for (int bar = 0; bar < pattern->bars; ++bar) {
        const int bar_start = bar * steps_per_bar;
        int crash_hits = 0;
        for (int step = 0; step < steps_per_bar && (bar_start + step) < pattern->total_steps; ++step) {
            DrumStepCell& crash = pattern->lanes[LANE_CRASH].steps[bar_start + step];
            if (crash.velocity > 0) {
                crash_hits += 1;
                if (crash_hits > 2) {
                    crash.velocity = 0;
                    crash.flags = 0;
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

static void regenerate_pattern(DrumGen* self,
                               const ControlSnapshot& controls,
                               bool fill_only_refresh) {
    PatternStateBlob next_pattern{};
    clear_pattern(&next_pattern, controls);
    next_pattern.generation_serial = self->pattern.generation_serial + 1;

    const uint32_t base_seed = controls.seed ^
                               ((uint32_t)controls.genre * 0x45D9F3Bu) ^
                               ((uint32_t)controls.action_new * 0x9E3779B9u) ^
                               ((uint32_t)controls.action_mutate * 0x7F4A7C15u);
    const uint32_t fill_seed = controls.seed ^
                               ((uint32_t)controls.action_fill * 0x85EBCA6Bu) ^
                               0xA511E9B3u;

    const bool compatible_shape = self->pattern_valid &&
                                  self->pattern.bars == next_pattern.bars &&
                                  self->pattern.steps_per_bar == next_pattern.steps_per_bar &&
                                  self->pattern.total_steps == next_pattern.total_steps &&
                                  self->controls.kit_map == controls.kit_map;

    if (fill_only_refresh && compatible_shape && next_pattern.total_steps > next_pattern.steps_per_bar) {
        const int prefix_steps = next_pattern.total_steps - next_pattern.steps_per_bar;
        copy_pattern_prefix(&next_pattern, &self->pattern, prefix_steps);
        for (int lane = 0; lane < kLaneCount; ++lane) {
            next_pattern.lanes[lane].midi_note = note_for_lane(controls.kit_map, lane);
        }
        const int last_bar = next_pattern.bars - 1;
        build_bar(&next_pattern, controls, last_bar, true, base_seed ^ fill_seed ^ (uint32_t)(last_bar + 1) * 0x27D4EB2Du);
    } else {
        for (int bar = 0; bar < next_pattern.bars; ++bar) {
            const bool fill_bar = (bar == next_pattern.bars - 1);
            const uint32_t bar_seed = base_seed ^
                                      (uint32_t)(bar + 1) * 0x27D4EB2Du ^
                                      (fill_bar ? fill_seed : 0u);
            build_bar(&next_pattern, controls, bar, fill_bar, bar_seed);
        }
    }

    cleanup_pattern(&next_pattern, controls);
    self->pattern = next_pattern;
    self->pattern_valid = true;
}

static void emit_midi_3(DrumGen* self, uint32_t frame, uint8_t status, uint8_t data1, uint8_t data2) {
    if (!self || !self->midi_out) {
        return;
    }

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

static void emit_note_on(DrumGen* self, uint32_t frame, int note, int velocity, int channel) {
    const uint8_t status = (uint8_t)(0x90 | clampi(channel - 1, 0, 15));
    emit_midi_3(self, frame, status, (uint8_t)clampi(note, 0, 127), (uint8_t)clampi(velocity, 1, 127));
}

static void emit_note_off(DrumGen* self, uint32_t frame, int note, int channel) {
    const uint8_t status = (uint8_t)(0x80 | clampi(channel - 1, 0, 15));
    emit_midi_3(self, frame, status, (uint8_t)clampi(note, 0, 127), 0);
}

static void enqueue_note_off(DrumGen* self, int note, int channel, int remaining_samples) {
    if (remaining_samples < 0) {
        remaining_samples = 0;
    }
    for (int i = 0; i < kMaxPendingNoteOffs; ++i) {
        if (!self->pending_note_offs[i].active) {
            self->pending_note_offs[i].active = true;
            self->pending_note_offs[i].note = note;
            self->pending_note_offs[i].channel = channel;
            self->pending_note_offs[i].remaining_samples = remaining_samples;
            return;
        }
    }
}

static void process_pending_note_offs(DrumGen* self, uint32_t nframes) {
    for (int i = 0; i < kMaxPendingNoteOffs; ++i) {
        PendingNoteOff& pending = self->pending_note_offs[i];
        if (!pending.active) {
            continue;
        }
        if (pending.remaining_samples < (int)nframes) {
            emit_note_off(self, (uint32_t)clampi(pending.remaining_samples, 0, (int)nframes - 1), pending.note, pending.channel);
            pending.active = false;
        } else {
            pending.remaining_samples -= (int)nframes;
        }
    }
}

static void clear_pending_note_offs(DrumGen* self, uint32_t frame) {
    for (int i = 0; i < kMaxPendingNoteOffs; ++i) {
        PendingNoteOff& pending = self->pending_note_offs[i];
        if (!pending.active) {
            continue;
        }
        emit_note_off(self, frame, pending.note, pending.channel);
        pending.active = false;
    }
}

static ControlSnapshot read_controls(const DrumGen* self) {
    ControlSnapshot c;
    c.genre = clampi((int)lroundf(self->genre_port ? *self->genre_port : 0.0f), 0, GENRE_COUNT - 1);
    c.channel = clampi((int)lroundf(self->channel_port ? *self->channel_port : 10.0f), 1, 16);
    c.kit_map = clampi((int)lroundf(self->kit_map_port ? *self->kit_map_port : 0.0f), 0, KIT_MAP_COUNT - 1);
    c.bars = clampi((int)lroundf(self->bars_port ? *self->bars_port : 2.0f), kMinBars, kMaxBars);
    c.resolution = clampi((int)lroundf(self->resolution_port ? *self->resolution_port : 1.0f), 0, RESOLUTION_COUNT - 1);
    c.density = clampf(self->density_port ? *self->density_port : 0.58f, 0.0f, 1.0f);
    c.variation = clampf(self->variation_port ? *self->variation_port : 0.35f, 0.0f, 1.0f);
    c.fill = clampf(self->fill_port ? *self->fill_port : 0.30f, 0.0f, 1.0f);
    c.seed = (uint32_t)clampf(self->seed_port ? *self->seed_port : 1.0f, 0.0f, 65535.0f);
    c.kick_amt = clampf(self->kick_amt_port ? *self->kick_amt_port : 0.78f, 0.0f, 1.0f);
    c.backbeat_amt = clampf(self->backbeat_amt_port ? *self->backbeat_amt_port : 0.76f, 0.0f, 1.0f);
    c.hat_amt = clampf(self->hat_amt_port ? *self->hat_amt_port : 0.82f, 0.0f, 1.0f);
    c.aux_amt = clampf(self->aux_amt_port ? *self->aux_amt_port : 0.28f, 0.0f, 1.0f);
    c.action_new = clampi((int)lroundf(self->action_new_port ? *self->action_new_port : 0.0f), 0, 1048576);
    c.action_mutate = clampi((int)lroundf(self->action_mutate_port ? *self->action_mutate_port : 0.0f), 0, 1048576);
    c.action_fill = clampi((int)lroundf(self->action_fill_port ? *self->action_fill_port : 0.0f), 0, 1048576);
    return c;
}

static void update_pattern_if_needed(DrumGen* self, const ControlSnapshot& current) {
    const bool controls_changed = !controls_match_excluding_actions(current, self->previous_controls);
    const bool new_changed = current.action_new != self->previous_controls.action_new;
    const bool mutate_changed = current.action_mutate != self->previous_controls.action_mutate;
    const bool fill_changed = current.action_fill != self->previous_controls.action_fill;

    if (!self->pattern_valid || controls_changed || new_changed || mutate_changed || fill_changed) {
        const bool fill_only_refresh = fill_changed && !controls_changed && !new_changed && !mutate_changed;
        regenerate_pattern(self, current, fill_only_refresh);
    }

    self->controls = current;
    self->previous_controls = current;
}

static void prepare_midi_output(DrumGen* self) {
    self->midi_out->atom.type = self->urids.atom_Sequence;
    self->midi_out->atom.size = sizeof(LV2_Atom_Sequence_Body);
    self->midi_out->body.unit = 0;
    self->midi_out->body.pad = 0;
}

static void reset_transport_state(DrumGen* self) {
    self->was_playing = false;
    self->last_transport_step = -1;
}

static void handle_stopped_transport(DrumGen* self) {
    clear_pending_note_offs(self, 0);
    reset_transport_state(self);
}

static bool transport_restart_detected(const DrumGen* self, int64_t start_step_floor) {
    return !self->was_playing || (self->last_transport_step >= 0 && start_step_floor < self->last_transport_step);
}

static int local_step_for_boundary(const PatternStateBlob& pattern, int64_t boundary) {
    const double local_step_d = fmod((double)boundary, (double)pattern.total_steps);
    return (int)(local_step_d < 0.0 ? local_step_d + pattern.total_steps : local_step_d);
}

static uint32_t frame_for_boundary(double abs_steps_start, double abs_steps_end, uint32_t nframes, int64_t boundary) {
    const double rel_steps = (double)boundary - abs_steps_start;
    const double t = rel_steps / (abs_steps_end - abs_steps_start + 1e-12);
    return (uint32_t)clampi((int)floor(t * (double)nframes), 0, (int)nframes - 1);
}

static void emit_step_hits(DrumGen* self,
                           uint32_t frame,
                           int local_step,
                           double samples_per_step,
                           uint32_t nframes) {
    if (!self->pattern_valid || local_step < 0 || local_step >= self->pattern.total_steps) {
        return;
    }

    for (int lane = 0; lane < kLaneCount; ++lane) {
        const DrumStepCell& cell = self->pattern.lanes[lane].steps[local_step];
        if (cell.velocity == 0) {
            continue;
        }

        const int note = self->pattern.lanes[lane].midi_note;
        const int on_frame = clampi((int)frame + (lane == LANE_OPEN_HAT ? kSafetyGapSamples : 0), 0, (int)nframes - 1);
        emit_note_on(self, on_frame, note, cell.velocity, self->controls.channel);

        const int gate_samples = clampi((int)lround(samples_per_step * (lane == LANE_CRASH ? 0.60 : 0.35)),
                                        24,
                                        (int)(self->sample_rate * 0.05));
        const int off_at = (int)on_frame + gate_samples;
        if (off_at < (int)nframes) {
            emit_note_off(self, (uint32_t)off_at, note, self->controls.channel);
        } else if (off_at >= 0) {
            enqueue_note_off(self, note, self->controls.channel, off_at - (int)nframes);
        }
    }
}

static LV2_State_Status drumgen_state_save(LV2_Handle instance,
                                           LV2_State_Store_Function store,
                                           LV2_State_Handle handle,
                                           uint32_t,
                                           const LV2_Feature* const*) {
    DrumGen* self = (DrumGen*)instance;
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

static LV2_State_Status drumgen_state_restore(LV2_Handle instance,
                                              LV2_State_Retrieve_Function retrieve,
                                              LV2_State_Handle handle,
                                              uint32_t,
                                              const LV2_Feature* const*) {
    DrumGen* self = (DrumGen*)instance;
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
        self->pattern.bars = clampi(self->pattern.bars, kMinBars, kMaxBars);
        self->pattern.steps_per_beat = clampi(self->pattern.steps_per_beat, 1, 4);
        self->pattern.steps_per_bar = clampi(self->pattern.steps_per_bar, 4, 16);
        self->pattern.total_steps = clampi(self->pattern.total_steps, 1, kMaxPatternSteps);
        self->pattern_valid = true;
    }

    return LV2_STATE_SUCCESS;
}

static const LV2_State_Interface drumgen_state_interface = {
    drumgen_state_save,
    drumgen_state_restore
};

static LV2_Handle instantiate(const LV2_Descriptor*,
                              double rate,
                              const char*,
                              const LV2_Feature* const* features) {
    DrumGen* self = new DrumGen();
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
    self->urids.state_controls = self->map->map(self->map->handle, DRUMGEN_URI "#controls");
    self->urids.state_pattern = self->map->map(self->map->handle, DRUMGEN_URI "#pattern");

    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    DrumGen* self = (DrumGen*)instance;
    switch (port) {
        case PORT_CONTROL: self->control = (const LV2_Atom_Sequence*)data; break;
        case PORT_MIDI_OUT: self->midi_out = (LV2_Atom_Sequence*)data; break;
        case PORT_GENRE: self->genre_port = (const float*)data; break;
        case PORT_CHANNEL: self->channel_port = (const float*)data; break;
        case PORT_KIT_MAP: self->kit_map_port = (const float*)data; break;
        case PORT_BARS: self->bars_port = (const float*)data; break;
        case PORT_RESOLUTION: self->resolution_port = (const float*)data; break;
        case PORT_DENSITY: self->density_port = (const float*)data; break;
        case PORT_VARIATION: self->variation_port = (const float*)data; break;
        case PORT_FILL: self->fill_port = (const float*)data; break;
        case PORT_SEED: self->seed_port = (const float*)data; break;
        case PORT_KICK_AMT: self->kick_amt_port = (const float*)data; break;
        case PORT_BACKBEAT_AMT: self->backbeat_amt_port = (const float*)data; break;
        case PORT_HAT_AMT: self->hat_amt_port = (const float*)data; break;
        case PORT_AUX_AMT: self->aux_amt_port = (const float*)data; break;
        case PORT_ACTION_NEW: self->action_new_port = (const float*)data; break;
        case PORT_ACTION_MUTATE: self->action_mutate_port = (const float*)data; break;
        case PORT_ACTION_FILL: self->action_fill_port = (const float*)data; break;
        default: break;
    }
}

static void activate(LV2_Handle instance) {
    DrumGen* self = (DrumGen*)instance;
    memset(self->pending_note_offs, 0, sizeof(self->pending_note_offs));
    reset_transport_state(self);
    self->controls = read_controls(self);
    self->previous_controls = self->controls;
    if (!self->pattern_valid) {
        regenerate_pattern(self, self->controls, false);
    }
}

static void run(LV2_Handle instance, uint32_t nframes) {
    DrumGen* self = (DrumGen*)instance;
    if (!self || !self->midi_out) {
        return;
    }

    prepare_midi_output(self);
    update_pattern_if_needed(self, read_controls(self));
    process_pending_note_offs(self, nframes);

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
    const double samples_per_step = self->sample_rate * 60.0 / (info.bpm * (double)spb);
    const int64_t start_step_floor = (int64_t)floor(abs_steps_start + 1e-9);

    if (transport_restart_detected(self, start_step_floor)) {
        clear_pending_note_offs(self, 0);
        const double local_step = fmod(abs_steps_start, (double)self->pattern.total_steps);
        const double wrapped = local_step < 0.0 ? local_step + self->pattern.total_steps : local_step;
        const double frac = wrapped - floor(wrapped);
        if (frac < 1e-6 || frac > 1.0 - 1e-6) {
            emit_step_hits(self, 0, (int)floor(wrapped + 1e-6) % self->pattern.total_steps, samples_per_step, nframes);
        }
    }

    self->was_playing = true;
    self->last_transport_step = start_step_floor;

    int64_t boundary = (int64_t)floor(abs_steps_start) + 1;
    const int64_t boundary_end = (int64_t)floor(abs_steps_end + 1e-9);

    while (boundary <= boundary_end) {
        const uint32_t frame = frame_for_boundary(abs_steps_start, abs_steps_end, nframes, boundary);
        const int local_step = local_step_for_boundary(self->pattern, boundary);
        emit_step_hits(self, frame, local_step, samples_per_step, nframes);
        boundary += 1;
    }
}

static void deactivate(LV2_Handle instance) {
    DrumGen* self = (DrumGen*)instance;
    reset_transport_state(self);
    memset(self->pending_note_offs, 0, sizeof(self->pending_note_offs));
}

static void cleanup(LV2_Handle instance) {
    DrumGen* self = (DrumGen*)instance;
    delete self;
}

static const void* extension_data(const char* uri) {
    if (!strcmp(uri, LV2_STATE__interface)) {
        return &drumgen_state_interface;
    }
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    DRUMGEN_URI,
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
