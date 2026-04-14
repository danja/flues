#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/state/state.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

#define CADENCE_URI "https://danja.github.io/flues/plugins/cadence"

static constexpr int kMaxSegments = 32;
static constexpr int kMaxChordNotes = 4;
static constexpr int kMaxCandidates = 108;
static constexpr uint32_t kBufferCapacity = 16384;
static constexpr double kBeatEpsilon = 1e-6;
static constexpr double kScoreFloor = -1.0e9;

enum PortIndex {
    PORT_CONTROL = 0,
    PORT_MIDI_IN,
    PORT_MIDI_OUT,
    PORT_KEY,
    PORT_SCALE,
    PORT_CYCLE_BARS,
    PORT_GRANULARITY,
    PORT_COMPLEXITY,
    PORT_CHORD_SIZE,
    PORT_REGISTER,
    PORT_SPREAD,
    PORT_PASS_INPUT,
    PORT_OUTPUT_CHANNEL,
    PORT_ACTION_LEARN,
    PORT_STATUS_READY
};

enum ScaleId {
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
};

enum GranularityMode {
    GRANULARITY_BEAT = 0,
    GRANULARITY_HALF_BAR,
    GRANULARITY_BAR
};

enum ChordSizeMode {
    CHORD_SIZE_TRIADS = 0,
    CHORD_SIZE_SEVENTHS
};

enum RegisterMode {
    REGISTER_LOW = 0,
    REGISTER_MID,
    REGISTER_HIGH
};

enum SpreadMode {
    SPREAD_CLOSE = 0,
    SPREAD_OPEN,
    SPREAD_DROP2
};

enum QualityId {
    QUALITY_POWER = 0,
    QUALITY_MAJOR,
    QUALITY_MINOR,
    QUALITY_SUS2,
    QUALITY_SUS4,
    QUALITY_DIM,
    QUALITY_DOM7,
    QUALITY_MAJ7,
    QUALITY_MIN7
};

struct ScaleDef {
    const int* intervals;
    int count;
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
    int key = 0;
    int scale = SCALE_NAT_MINOR;
    int cycle_bars = 2;
    int granularity = GRANULARITY_HALF_BAR;
    float complexity = 0.45f;
    int chord_size = CHORD_SIZE_TRIADS;
    int reg = REGISTER_MID;
    int spread = SPREAD_CLOSE;
    bool pass_input = true;
    int output_channel = 0;
    int action_learn = 0;
};

struct SegmentCapture {
    double duration[12]{};
    double onset[12]{};
};

struct ChordSlot {
    bool valid = false;
    uint8_t root_pc = 0;
    uint8_t quality = QUALITY_MAJOR;
    uint8_t note_count = 0;
    uint8_t velocity = 96;
    uint8_t notes[kMaxChordNotes]{};
};

struct Candidate {
    uint8_t root_pc = 0;
    uint8_t quality = QUALITY_MAJOR;
    uint8_t note_count = 0;
    uint8_t intervals[kMaxChordNotes]{};
    uint16_t mask = 0;
};

struct SavedChordSlot {
    int32_t valid = 0;
    int32_t root_pc = 0;
    int32_t quality = 0;
    int32_t note_count = 0;
    int32_t velocity = 96;
    int32_t notes[kMaxChordNotes]{};
};

struct ProgressionStateBlob {
    int32_t version = 1;
    int32_t segment_count = 0;
    int32_t ready = 0;
    SavedChordSlot slots[kMaxSegments]{};
};

struct CadenceURIDs {
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
    LV2_URID state_progression = 0;
};

struct Cadence {
    const LV2_Atom_Sequence* control = nullptr;
    const LV2_Atom_Sequence* midi_in = nullptr;
    LV2_Atom_Sequence* midi_out = nullptr;

    const float* key_port = nullptr;
    const float* scale_port = nullptr;
    const float* cycle_bars_port = nullptr;
    const float* granularity_port = nullptr;
    const float* complexity_port = nullptr;
    const float* chord_size_port = nullptr;
    const float* register_port = nullptr;
    const float* spread_port = nullptr;
    const float* pass_input_port = nullptr;
    const float* output_channel_port = nullptr;
    const float* action_learn_port = nullptr;
    float* status_ready_port = nullptr;

    LV2_URID_Map* map = nullptr;
    CadenceURIDs urids{};

    double sample_rate = 48000.0;

    ControlSnapshot controls{};
    ControlSnapshot previous_controls{};
    bool controls_initialized = false;

    SegmentCapture capture[kMaxSegments]{};
    ChordSlot playback[kMaxSegments]{};
    int playback_segment_count = 0;
    bool ready = false;

    bool held_notes[128]{};
    uint8_t held_velocity[128]{};

    uint8_t active_harmony_notes[kMaxChordNotes]{};
    uint8_t active_harmony_count = 0;
    int active_harmony_channel = 1;
    int last_input_channel = 1;

    bool was_playing = false;
    double last_abs_beats_start = 0.0;
};

static const int kScaleChromatic[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const int kScaleMajor[] = {0, 2, 4, 5, 7, 9, 11};
static const int kScaleNatMinor[] = {0, 2, 3, 5, 7, 8, 10};
static const int kScaleHarmMinor[] = {0, 2, 3, 5, 7, 8, 11};
static const int kScalePentMajor[] = {0, 2, 4, 7, 9};
static const int kScalePentMinor[] = {0, 3, 5, 7, 10};
static const int kScaleBlues[] = {0, 3, 5, 6, 7, 10};
static const int kScaleDorian[] = {0, 2, 3, 5, 7, 9, 10};
static const int kScaleMixolydian[] = {0, 2, 4, 5, 7, 9, 10};
static const int kScalePhrygian[] = {0, 1, 3, 5, 7, 8, 10};
static const int kScaleLocrian[] = {0, 1, 3, 5, 6, 8, 10};
static const int kScalePhrygianDominant[] = {0, 1, 4, 5, 7, 8, 10};

static const ScaleDef kScales[SCALE_COUNT] = {
    {kScaleChromatic, 12},
    {kScaleMajor, 7},
    {kScaleNatMinor, 7},
    {kScaleHarmMinor, 7},
    {kScalePentMajor, 5},
    {kScalePentMinor, 5},
    {kScaleBlues, 6},
    {kScaleDorian, 7},
    {kScaleMixolydian, 7},
    {kScalePhrygian, 7},
    {kScaleLocrian, 7},
    {kScalePhrygianDominant, 7}
};

static inline int clampi(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static inline float clampf(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static inline double clampd(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static inline int wrap12(int value) {
    int out = value % 12;
    if (out < 0) out += 12;
    return out;
}

static void sort_notes(uint8_t* notes, uint8_t count) {
    std::sort(notes, notes + count);
}

static bool atom_to_double(const LV2_Atom* atom, const CadenceURIDs& urids, double* out) {
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

static TimeInfo read_time_info(const LV2_Atom_Sequence* control, const CadenceURIDs& urids) {
    TimeInfo info{};
    if (!control) {
        return info;
    }

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

        info.valid = true;

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
            info.bar = value;
        }
        if (atom_to_double(bar_beat_atom, urids, &value)) {
            info.barBeat = value;
        }
        if (atom_to_double(beats_per_bar_atom, urids, &value)) {
            info.beatsPerBar = value > 0.0 ? value : 4.0;
        }
        if (atom_to_double(bpm_atom, urids, &value)) {
            info.bpm = value > 1.0 ? value : 120.0;
        }
        if (atom_to_double(speed_atom, urids, &value)) {
            info.playing = value > 0.0;
        } else {
            info.playing = true;
        }
    }

    return info;
}

static void append_midi(LV2_Atom_Sequence* seq,
                        LV2_URID midi_event_urid,
                        uint32_t frame,
                        const uint8_t* msg,
                        uint32_t size) {
    LV2_Atom_Event ev;
    ev.time.frames = frame;
    ev.body.type = midi_event_urid;
    ev.body.size = size;

    LV2_Atom_Event* appended = lv2_atom_sequence_append_event(seq, kBufferCapacity, &ev);
    if (appended) {
        uint8_t* body = (uint8_t*)(appended + 1);
        std::memcpy(body, msg, size);
    }
}

static int resolve_output_channel(const Cadence* self, int configured_channel) {
    if (configured_channel == 0) {
        return clampi(self ? self->last_input_channel : 1, 1, 16);
    }
    return clampi(configured_channel, 1, 16);
}

static void emit_note_on_on_channel(Cadence* self, uint32_t frame, int note, int velocity, int output_channel) {
    const uint8_t channel = (uint8_t)(clampi(output_channel, 1, 16) - 1);
    const uint8_t msg[3] = {
        (uint8_t)(0x90 | channel),
        (uint8_t)clampi(note, 0, 127),
        (uint8_t)clampi(velocity, 1, 127)
    };
    append_midi(self->midi_out, self->urids.midi_Event, frame, msg, 3);
}

static void emit_note_on(Cadence* self, uint32_t frame, int note, int velocity) {
    emit_note_on_on_channel(self, frame, note, velocity, resolve_output_channel(self, self->controls.output_channel));
}

static void emit_note_off_on_channel(Cadence* self, uint32_t frame, int note, int output_channel) {
    const uint8_t channel = (uint8_t)(clampi(output_channel, 1, 16) - 1);
    const uint8_t msg[3] = {
        (uint8_t)(0x80 | channel),
        (uint8_t)clampi(note, 0, 127),
        0
    };
    append_midi(self->midi_out, self->urids.midi_Event, frame, msg, 3);
}

static void emit_note_off(Cadence* self, uint32_t frame, int note) {
    emit_note_off_on_channel(self, frame, note, self->active_harmony_channel);
}

static double cycle_beats_for_controls(const ControlSnapshot& controls, double beats_per_bar) {
    return clampd((double)controls.cycle_bars * beats_per_bar, 1.0, (double)kMaxSegments * beats_per_bar);
}

static double segment_beats_for_controls(const ControlSnapshot& controls, double beats_per_bar) {
    switch (controls.granularity) {
        case GRANULARITY_BEAT:
            return 1.0;
        case GRANULARITY_HALF_BAR:
            return std::max(0.5, beats_per_bar * 0.5);
        case GRANULARITY_BAR:
        default:
            return std::max(1.0, beats_per_bar);
    }
}

static int segment_count_for_controls(const ControlSnapshot& controls, double beats_per_bar) {
    const double cycle_beats = cycle_beats_for_controls(controls, beats_per_bar);
    const double segment_beats = segment_beats_for_controls(controls, beats_per_bar);
    return clampi((int)lround(cycle_beats / segment_beats), 1, kMaxSegments);
}

static double wrapped_cycle_position(double abs_beats, const ControlSnapshot& controls, double beats_per_bar) {
    const double cycle_beats = cycle_beats_for_controls(controls, beats_per_bar);
    double local = std::fmod(abs_beats, cycle_beats);
    if (local < 0.0) {
        local += cycle_beats;
    }
    return local;
}

static int segment_index_for_time(const ControlSnapshot& controls,
                                  double beats_per_bar,
                                  int segment_count,
                                  double abs_beats) {
    const double cycle_pos = wrapped_cycle_position(abs_beats, controls, beats_per_bar);
    const double segment_beats = segment_beats_for_controls(controls, beats_per_bar);
    int index = (int)std::floor((cycle_pos + kBeatEpsilon) / segment_beats);
    if (index >= segment_count) {
        index = segment_count - 1;
    }
    return clampi(index, 0, segment_count - 1);
}

static uint32_t frame_for_beat(double abs_beats_start,
                               double abs_beats_end,
                               uint32_t nframes,
                               double target_beat) {
    if (nframes == 0 || abs_beats_end <= abs_beats_start + 1e-12) {
        return 0;
    }
    const double t = clampd((target_beat - abs_beats_start) / (abs_beats_end - abs_beats_start), 0.0, 1.0);
    return (uint32_t)clampi((int)lround(t * (double)nframes), 0, (int)nframes);
}

static void clear_capture(Cadence* self) {
    std::memset(self->capture, 0, sizeof(self->capture));
}

static void clear_playback(Cadence* self) {
    std::memset(self->playback, 0, sizeof(self->playback));
    self->playback_segment_count = 0;
    self->ready = false;
}

static void clear_held_notes(Cadence* self) {
    std::memset(self->held_notes, 0, sizeof(self->held_notes));
    std::memset(self->held_velocity, 0, sizeof(self->held_velocity));
}

static bool note_in_set(const uint8_t* notes, uint8_t count, uint8_t note) {
    for (uint8_t i = 0; i < count; ++i) {
        if (notes[i] == note) {
            return true;
        }
    }
    return false;
}

static void transition_to_slot(Cadence* self, uint32_t frame, const ChordSlot* slot, bool retrigger) {
    const uint8_t* next_notes = slot && slot->valid ? slot->notes : nullptr;
    const uint8_t next_count = slot && slot->valid ? slot->note_count : 0;
    const int next_channel = resolve_output_channel(self, self->controls.output_channel);

    if (retrigger && slot && slot->valid) {
        for (uint8_t i = 0; i < self->active_harmony_count; ++i) {
            emit_note_off(self, frame, self->active_harmony_notes[i]);
        }
        for (uint8_t i = 0; i < slot->note_count; ++i) {
            emit_note_on_on_channel(self, frame, slot->notes[i], slot->velocity, next_channel);
        }
        self->active_harmony_count = slot->note_count;
        self->active_harmony_channel = next_channel;
        for (uint8_t i = 0; i < slot->note_count; ++i) {
            self->active_harmony_notes[i] = slot->notes[i];
        }
        return;
    }

    for (uint8_t i = 0; i < self->active_harmony_count; ++i) {
        const uint8_t note = self->active_harmony_notes[i];
        if (!note_in_set(next_notes, next_count, note)) {
            emit_note_off(self, frame, note);
        }
    }

    if (slot && slot->valid) {
        for (uint8_t i = 0; i < slot->note_count; ++i) {
            const uint8_t note = slot->notes[i];
            if (!note_in_set(self->active_harmony_notes, self->active_harmony_count, note)) {
                emit_note_on_on_channel(self, frame, note, slot->velocity, next_channel);
            }
        }
        self->active_harmony_channel = next_channel;
    }

    self->active_harmony_count = next_count;
    for (uint8_t i = 0; i < next_count; ++i) {
        self->active_harmony_notes[i] = next_notes[i];
    }
}

static void silence_harmony(Cadence* self, uint32_t frame) {
    transition_to_slot(self, frame, nullptr, false);
}

static bool scale_contains_pc(int scale_index, int key, int note_pc) {
    scale_index = clampi(scale_index, 0, SCALE_COUNT - 1);
    key = wrap12(key);
    note_pc = wrap12(note_pc);

    const ScaleDef& scale = kScales[scale_index];
    const int rel = wrap12(note_pc - key);
    for (int i = 0; i < scale.count; ++i) {
        if (scale.intervals[i] == rel) {
            return true;
        }
    }
    return false;
}

static double segment_activity(const SegmentCapture& segment) {
    double total = 0.0;
    for (int pc = 0; pc < 12; ++pc) {
        total += segment.duration[pc];
        total += segment.onset[pc] * 1.35;
    }
    return total;
}

static int dominant_pc_for_segment(const SegmentCapture& segment, double* out_weight, double* out_total) {
    int best_pc = 0;
    double best_weight = -1.0;
    double total = 0.0;

    for (int pc = 0; pc < 12; ++pc) {
        const double weight = segment.duration[pc] + segment.onset[pc] * 1.6;
        total += weight;
        if (weight > best_weight) {
            best_weight = weight;
            best_pc = pc;
        }
    }

    if (out_weight) {
        *out_weight = best_weight > 0.0 ? best_weight : 0.0;
    }
    if (out_total) {
        *out_total = total;
    }
    return best_pc;
}

struct RootPreference {
    uint16_t mask = 0;
    int primary_pc = 0;
    double primary_ratio = 0.0;
};

static RootPreference preferred_roots_for_segment(const SegmentCapture& segment) {
    RootPreference pref{};
    double weights[12]{};
    double total = 0.0;

    for (int pc = 0; pc < 12; ++pc) {
        weights[pc] = segment.duration[pc] + segment.onset[pc] * 1.8;
        total += weights[pc];
    }

    if (total < 0.02) {
        pref.mask = 0x0FFFu;
        return pref;
    }

    int ranked[3] = {0, 0, 0};
    double ranked_weight[3] = {-1.0, -1.0, -1.0};
    for (int pc = 0; pc < 12; ++pc) {
        const double w = weights[pc];
        for (int slot = 0; slot < 3; ++slot) {
            if (w > ranked_weight[slot]) {
                for (int move = 2; move > slot; --move) {
                    ranked_weight[move] = ranked_weight[move - 1];
                    ranked[move] = ranked[move - 1];
                }
                ranked_weight[slot] = w;
                ranked[slot] = pc;
                break;
            }
        }
    }

    pref.primary_pc = ranked[0];
    pref.primary_ratio = ranked_weight[0] / total;
    pref.mask |= (uint16_t)(1u << ranked[0]);

    if (ranked_weight[1] > total * 0.14 || pref.primary_ratio < 0.74) {
        pref.mask |= (uint16_t)(1u << ranked[1]);
    }
    if (ranked_weight[2] > total * 0.20 || pref.primary_ratio < 0.54) {
        pref.mask |= (uint16_t)(1u << ranked[2]);
    }

    return pref;
}

static int register_center(int reg) {
    switch (reg) {
        case REGISTER_LOW:
            return 52;
        case REGISTER_HIGH:
            return 76;
        case REGISTER_MID:
        default:
            return 64;
    }
}

static int nearest_midi_for_pc(int pc, int target) {
    int best_note = 60 + pc;
    int best_dist = 999;
    for (int octave = -1; octave <= 10; ++octave) {
        const int candidate = octave * 12 + pc;
        if (candidate < 0 || candidate > 127) {
            continue;
        }
        const int dist = std::abs(candidate - target);
        if (dist < best_dist) {
            best_dist = dist;
            best_note = candidate;
        }
    }
    return best_note;
}

static void sort_int_notes(int* notes, int count) {
    std::sort(notes, notes + count);
}

static void apply_spread_to_notes(int* notes, int count, int spread) {
    if (count < 3) {
        return;
    }

    sort_int_notes(notes, count);
    switch (spread) {
        case SPREAD_OPEN:
            notes[1] += 12;
            break;
        case SPREAD_DROP2:
            if (count >= 4) {
                notes[count - 2] -= 12;
            }
            break;
        case SPREAD_CLOSE:
        default:
            break;
    }
    sort_int_notes(notes, count);
}

static double voicing_cost(const int* notes,
                           int count,
                           const uint8_t* previous_notes,
                           uint8_t previous_count,
                           int center) {
    if (count <= 0) {
        return 0.0;
    }

    double cost = 0.0;
    if (previous_count > 0 && previous_notes) {
        uint8_t prev[kMaxChordNotes];
        std::memcpy(prev, previous_notes, previous_count);
        sort_notes(prev, previous_count);

        for (int i = 0; i < count; ++i) {
            const int target_index = clampi((i * previous_count) / count, 0, previous_count - 1);
            cost += std::abs(notes[i] - (int)prev[target_index]) * 0.18;
        }
    }

    int sum = 0;
    for (int i = 0; i < count; ++i) {
        sum += notes[i];
    }
    const double avg = (double)sum / (double)count;
    cost += std::abs(avg - (double)center) * 0.08;

    const int range = notes[count - 1] - notes[0];
    if (range > 20) {
        cost += (double)(range - 20) * 0.07;
    }
    if (notes[0] < 36) {
        cost += (double)(36 - notes[0]) * 0.12;
    }
    if (notes[count - 1] > 100) {
        cost += (double)(notes[count - 1] - 100) * 0.12;
    }

    return cost;
}

static void build_voicing(const Candidate& candidate,
                          const ControlSnapshot& controls,
                          const uint8_t* previous_notes,
                          uint8_t previous_count,
                          uint8_t velocity,
                          ChordSlot* out) {
    out->valid = false;
    out->root_pc = candidate.root_pc;
    out->quality = candidate.quality;
    out->note_count = candidate.note_count;
    out->velocity = velocity;

    if (candidate.note_count == 0) {
        return;
    }

    const int center = register_center(controls.reg);
    const int anchor_target = center - 7;
    const int base_root = nearest_midi_for_pc(candidate.root_pc, anchor_target);

    int base[kMaxChordNotes]{};
    for (uint8_t i = 0; i < candidate.note_count; ++i) {
        base[i] = base_root + candidate.intervals[i];
    }

    double best_cost = 1.0e18;
    int best_notes[kMaxChordNotes]{};

    for (uint8_t inversion = 0; inversion < candidate.note_count; ++inversion) {
        int notes[kMaxChordNotes]{};
        for (uint8_t i = 0; i < candidate.note_count; ++i) {
            notes[i] = base[i];
        }

        for (uint8_t i = 0; i < inversion; ++i) {
            notes[i] += 12;
        }
        sort_int_notes(notes, candidate.note_count);
        apply_spread_to_notes(notes, candidate.note_count, controls.spread);

        for (int shift = -1; shift <= 1; ++shift) {
            int shifted[kMaxChordNotes]{};
            bool valid = true;
            for (uint8_t i = 0; i < candidate.note_count; ++i) {
                shifted[i] = notes[i] + shift * 12;
                if (shifted[i] < 0 || shifted[i] > 127) {
                    valid = false;
                }
            }
            if (!valid) {
                continue;
            }
            sort_int_notes(shifted, candidate.note_count);
            const double cost = voicing_cost(shifted, candidate.note_count, previous_notes, previous_count, center);
            if (cost < best_cost) {
                best_cost = cost;
                for (uint8_t i = 0; i < candidate.note_count; ++i) {
                    best_notes[i] = shifted[i];
                }
            }
        }
    }

    for (uint8_t i = 0; i < candidate.note_count; ++i) {
        out->notes[i] = (uint8_t)clampi(best_notes[i], 0, 127);
    }
    sort_notes(out->notes, out->note_count);
    out->valid = true;
}

static bool controls_match(const ControlSnapshot& a, const ControlSnapshot& b) {
    return a.key == b.key &&
           a.scale == b.scale &&
           a.cycle_bars == b.cycle_bars &&
           a.granularity == b.granularity &&
           std::fabs(a.complexity - b.complexity) < 0.0001f &&
           a.chord_size == b.chord_size &&
           a.reg == b.reg &&
           a.spread == b.spread &&
           a.pass_input == b.pass_input &&
           a.output_channel == b.output_channel;
}

static ControlSnapshot read_controls(Cadence* self) {
    ControlSnapshot controls{};
    controls.key = clampi((int)lroundf(self->key_port ? *self->key_port : 0.0f), 0, 11);
    controls.scale = clampi((int)lroundf(self->scale_port ? *self->scale_port : (float)SCALE_NAT_MINOR), 0, SCALE_COUNT - 1);
    controls.cycle_bars = clampi((int)lroundf(self->cycle_bars_port ? *self->cycle_bars_port : 2.0f), 1, 8);
    controls.granularity = clampi((int)lroundf(self->granularity_port ? *self->granularity_port : (float)GRANULARITY_HALF_BAR), 0, 2);
    controls.complexity = clampf(self->complexity_port ? *self->complexity_port : 0.45f, 0.0f, 1.0f);
    controls.chord_size = clampi((int)lroundf(self->chord_size_port ? *self->chord_size_port : (float)CHORD_SIZE_TRIADS), 0, 1);
    controls.reg = clampi((int)lroundf(self->register_port ? *self->register_port : (float)REGISTER_MID), 0, 2);
    controls.spread = clampi((int)lroundf(self->spread_port ? *self->spread_port : (float)SPREAD_CLOSE), 0, 2);
    controls.pass_input = (self->pass_input_port ? *self->pass_input_port : 1.0f) >= 0.5f;
    controls.output_channel = clampi((int)lroundf(self->output_channel_port ? *self->output_channel_port : 0.0f), 0, 16);
    controls.action_learn = clampi((int)lroundf(self->action_learn_port ? *self->action_learn_port : 0.0f), 0, 1048576);
    return controls;
}

static void reset_learning(Cadence* self) {
    clear_capture(self);
    clear_playback(self);
    clear_held_notes(self);
}

static bool quality_uses_seventh(uint8_t quality) {
    return quality == QUALITY_DOM7 || quality == QUALITY_MAJ7 || quality == QUALITY_MIN7;
}

static int build_candidates(const ControlSnapshot& controls, Candidate* out) {
    static const Candidate defs[] = {
        {0, QUALITY_POWER, 2, {0, 7, 0, 0}, 0},
        {0, QUALITY_MAJOR, 3, {0, 4, 7, 0}, 0},
        {0, QUALITY_MINOR, 3, {0, 3, 7, 0}, 0},
        {0, QUALITY_SUS2, 3, {0, 2, 7, 0}, 0},
        {0, QUALITY_SUS4, 3, {0, 5, 7, 0}, 0},
        {0, QUALITY_DIM, 3, {0, 3, 6, 0}, 0},
        {0, QUALITY_DOM7, 4, {0, 4, 7, 10}, 0},
        {0, QUALITY_MAJ7, 4, {0, 4, 7, 11}, 0},
        {0, QUALITY_MIN7, 4, {0, 3, 7, 10}, 0}
    };

    int count = 0;
    for (int root = 0; root < 12; ++root) {
        for (const Candidate& def : defs) {
            if (controls.chord_size == CHORD_SIZE_TRIADS && quality_uses_seventh(def.quality)) {
                continue;
            }

            Candidate candidate = def;
            candidate.root_pc = (uint8_t)root;
            candidate.mask = 0;
            for (uint8_t i = 0; i < candidate.note_count; ++i) {
                const int pc = wrap12(root + candidate.intervals[i]);
                candidate.mask |= (uint16_t)(1u << pc);
            }
            out[count++] = candidate;
        }
    }
    return count;
}

static double score_candidate(const SegmentCapture& segment,
                              const Candidate& candidate,
                              const ControlSnapshot& controls) {
    double weights[12]{};
    double total = 0.0;
    for (int pc = 0; pc < 12; ++pc) {
        weights[pc] = segment.duration[pc] + segment.onset[pc] * 1.35;
        total += weights[pc];
    }
    if (total < 0.02) {
        return 0.0;
    }

    double dominant_weight = 0.0;
    double dominant_total = 0.0;
    const int dominant_pc = dominant_pc_for_segment(segment, &dominant_weight, &dominant_total);
    const double dominant_ratio = dominant_total > 1e-9 ? dominant_weight / dominant_total : 0.0;
    const RootPreference root_pref = preferred_roots_for_segment(segment);

    bool chord_pcs[12]{};
    double score = 0.0;
    for (uint8_t i = 0; i < candidate.note_count; ++i) {
        const int pc = wrap12(candidate.root_pc + candidate.intervals[i]);
        chord_pcs[pc] = true;

        double role_weight = 0.95;
        if (i == 0) {
            role_weight = 1.45;
        } else if (i == 1) {
            role_weight = 1.12;
        } else if (i == 2) {
            role_weight = 0.92;
        } else {
            role_weight = 0.72;
        }

        score += weights[pc] * role_weight;
        if (!scale_contains_pc(controls.scale, controls.key, pc)) {
            score -= 0.30 + (1.0 - controls.complexity) * 0.22;
        }
    }

    for (int pc = 0; pc < 12; ++pc) {
        if (!chord_pcs[pc]) {
            score -= weights[pc] * 0.66;
        }
    }

    score += segment.onset[candidate.root_pc] * 0.95;
    score += segment.duration[candidate.root_pc] * 0.45;

    if (candidate.root_pc == root_pref.primary_pc) {
        score += 0.95 + root_pref.primary_ratio * 0.95;
    } else if (root_pref.mask & (uint16_t)(1u << candidate.root_pc)) {
        score += 0.22;
    } else {
        score -= 0.95 + root_pref.primary_ratio * 0.70;
    }

    if (candidate.root_pc == dominant_pc) {
        score += 0.48 + dominant_ratio * 0.72;
    } else if (chord_pcs[dominant_pc]) {
        score += 0.12 + dominant_ratio * 0.20;
    } else {
        score -= 0.24 + dominant_ratio * 0.42;
    }

    if (scale_contains_pc(controls.scale, controls.key, candidate.root_pc)) {
        score += 0.18;
    } else {
        score -= 0.55 - controls.complexity * 0.18;
    }

    switch (candidate.quality) {
        case QUALITY_POWER:
            score -= 0.42 + controls.complexity * 0.28;
            break;
        case QUALITY_SUS2: {
            const double sus_weight = weights[wrap12(candidate.root_pc + 2)];
            const double major_third = weights[wrap12(candidate.root_pc + 4)];
            const double minor_third = weights[wrap12(candidate.root_pc + 3)];
            score -= 0.18;
            score += sus_weight / (total + 1e-9) * 0.50;
            score += (major_third + minor_third < total * 0.18) ? 0.14 : -0.08;
            break;
        }
        case QUALITY_SUS4: {
            const double sus_weight = weights[wrap12(candidate.root_pc + 5)];
            const double major_third = weights[wrap12(candidate.root_pc + 4)];
            const double minor_third = weights[wrap12(candidate.root_pc + 3)];
            score -= 0.18;
            score += sus_weight / (total + 1e-9) * 0.50;
            score += (major_third + minor_third < total * 0.18) ? 0.14 : -0.08;
            break;
        }
        case QUALITY_DIM:
            score -= 0.30;
            score += weights[wrap12(candidate.root_pc + 6)] / (total + 1e-9) * 0.32;
            break;
        case QUALITY_DOM7:
        case QUALITY_MAJ7:
        case QUALITY_MIN7: {
            const int seventh_pc = wrap12(candidate.root_pc + candidate.intervals[3]);
            score -= 0.12 + (1.0 - controls.complexity) * 0.24;
            score += weights[seventh_pc] / (total + 1e-9) * 0.36;
            break;
        }
        case QUALITY_MAJOR:
            score += weights[wrap12(candidate.root_pc + 4)] / (total + 1e-9) * 0.22;
            break;
        case QUALITY_MINOR:
            score += weights[wrap12(candidate.root_pc + 3)] / (total + 1e-9) * 0.22;
            break;
        default:
            break;
    }

    return score / (total + 1e-9);
}

static int popcount16(uint16_t value) {
    int count = 0;
    while (value) {
        count += (value & 1u) ? 1 : 0;
        value >>= 1u;
    }
    return count;
}

static double transition_score(const Candidate& previous,
                               const Candidate& next,
                               const ControlSnapshot& controls,
                               int segment_index,
                               int segment_count) {
    const int interval = wrap12((int)next.root_pc - (int)previous.root_pc);
    const int step = std::min(interval, 12 - interval);

    double score = 0.0;
    if (interval == 0 && previous.quality == next.quality) {
        score -= 0.34;
    } else if (interval == 0) {
        score -= 0.16;
    } else if (interval == 5 || interval == 7) {
        score += 0.24;
    } else if (interval == 2 || interval == 10) {
        score += 0.12;
    } else if (step == 1) {
        score += 0.04;
    } else if (step == 6) {
        score -= 0.16;
    } else {
        score += 0.02;
    }

    score += (double)popcount16(previous.mask & next.mask) * 0.09;

    if (segment_index == segment_count - 1 && next.root_pc == controls.key) {
        score += 0.22;
    }
    if (segment_index == 0 && next.root_pc == controls.key) {
        score += 0.12;
    }
    if (next.quality == QUALITY_DIM) {
        score -= 0.08;
    }

    return score;
}

static bool infer_progression(Cadence* self, int segment_count) {
    double total_activity = 0.0;
    for (int i = 0; i < segment_count; ++i) {
        total_activity += segment_activity(self->capture[i]);
    }
    if (total_activity < 0.2) {
        return false;
    }

    Candidate candidates[kMaxCandidates]{};
    const int candidate_count = build_candidates(self->controls, candidates);
    if (candidate_count <= 0) {
        return false;
    }

    double local_scores[kMaxSegments][kMaxCandidates]{};
    double dp[kMaxSegments][kMaxCandidates]{};
    int trace[kMaxSegments][kMaxCandidates]{};
    RootPreference root_prefs[kMaxSegments]{};

    for (int s = 0; s < segment_count; ++s) {
        root_prefs[s] = preferred_roots_for_segment(self->capture[s]);
        for (int c = 0; c < candidate_count; ++c) {
            if (!(root_prefs[s].mask & (uint16_t)(1u << candidates[c].root_pc))) {
                local_scores[s][c] = kScoreFloor * 0.5;
            } else {
                local_scores[s][c] = score_candidate(self->capture[s], candidates[c], self->controls);
            }
            dp[s][c] = kScoreFloor;
            trace[s][c] = -1;
        }
    }

    for (int c = 0; c < candidate_count; ++c) {
        double start_bonus = 0.0;
        if (candidates[c].root_pc == self->controls.key) {
            start_bonus += 0.10;
        }
        dp[0][c] = local_scores[0][c] + start_bonus;
    }

    for (int s = 1; s < segment_count; ++s) {
        for (int c = 0; c < candidate_count; ++c) {
            for (int p = 0; p < candidate_count; ++p) {
                const double candidate_score = dp[s - 1][p] +
                                               local_scores[s][c] +
                                               transition_score(candidates[p], candidates[c], self->controls, s, segment_count);
                if (candidate_score > dp[s][c]) {
                    dp[s][c] = candidate_score;
                    trace[s][c] = p;
                }
            }
        }
    }

    int best_index = 0;
    double best_score = dp[segment_count - 1][0];
    for (int c = 1; c < candidate_count; ++c) {
        if (dp[segment_count - 1][c] > best_score) {
            best_score = dp[segment_count - 1][c];
            best_index = c;
        }
    }

    int chosen[kMaxSegments]{};
    chosen[segment_count - 1] = best_index;
    for (int s = segment_count - 1; s > 0; --s) {
        const int prev = trace[s][chosen[s]];
        chosen[s - 1] = prev >= 0 ? prev : 0;
    }

    ChordSlot previous_slot{};
    for (int s = 0; s < segment_count; ++s) {
        const Candidate& candidate = candidates[chosen[s]];
        const double activity = segment_activity(self->capture[s]);
        const uint8_t velocity = (uint8_t)clampi((int)lround(76.0 + std::min(28.0, activity * 12.0)), 60, 110);
        build_voicing(candidate,
                      self->controls,
                      previous_slot.valid ? previous_slot.notes : nullptr,
                      previous_slot.valid ? previous_slot.note_count : 0,
                      velocity,
                      &self->playback[s]);
        previous_slot = self->playback[s];
    }

    self->playback_segment_count = segment_count;
    self->ready = true;
    return true;
}

static void capture_interval(Cadence* self,
                             double beats_per_bar,
                             int segment_count,
                             double abs_start,
                             double abs_end) {
    if (abs_end <= abs_start + 1e-12 || segment_count <= 0) {
        return;
    }

    const int segment = segment_index_for_time(self->controls, beats_per_bar, segment_count, abs_start + kBeatEpsilon);
    for (int note = 0; note < 128; ++note) {
        if (!self->held_notes[note]) {
            continue;
        }
        self->capture[segment].duration[note % 12] += abs_end - abs_start;
    }
}

static void capture_onset(Cadence* self,
                          double beats_per_bar,
                          int segment_count,
                          double abs_beats,
                          uint8_t note,
                          uint8_t velocity) {
    if (segment_count <= 0) {
        return;
    }

    const int segment = segment_index_for_time(self->controls, beats_per_bar, segment_count, abs_beats + kBeatEpsilon);
    const double segment_beats = segment_beats_for_controls(self->controls, beats_per_bar);
    const double cycle_pos = wrapped_cycle_position(abs_beats, self->controls, beats_per_bar);
    const double segment_pos = std::fmod(cycle_pos, segment_beats);
    double bonus = 0.35 + ((double)velocity / 127.0) * 0.65;

    if (segment_pos < std::max(0.12, segment_beats * 0.15)) {
        bonus *= 1.15;
    }

    self->capture[segment].onset[note % 12] += bonus;
}

static void handle_boundary(Cadence* self,
                            uint32_t frame,
                            double abs_boundary_beat,
                            double beats_per_bar,
                            int segment_count) {
    const double segment_beats = segment_beats_for_controls(self->controls, beats_per_bar);
    const int64_t boundary_index = (int64_t)llround(abs_boundary_beat / segment_beats);
    const bool cycle_boundary = (segment_count > 0) ? ((boundary_index % segment_count) == 0) : false;

    if (cycle_boundary) {
        infer_progression(self, segment_count);
        clear_capture(self);
    }

    if (self->ready && self->playback_segment_count == segment_count) {
        const int segment = segment_index_for_time(self->controls, beats_per_bar, segment_count, abs_boundary_beat + kBeatEpsilon);
        transition_to_slot(self, frame, &self->playback[segment], true);
    } else {
        silence_harmony(self, frame);
    }
}

static bool is_note_message(const uint8_t* msg, uint32_t size) {
    if (!msg || size < 2) {
        return false;
    }
    const uint8_t type = msg[0] & 0xF0;
    return (type == 0x80 || type == 0x90) && size >= 3;
}

static void forward_input_event(Cadence* self, const LV2_Atom_Event* ev, const uint8_t* msg, uint32_t size) {
    if (self->controls.pass_input) {
        append_midi(self->midi_out, self->urids.midi_Event, ev->time.frames, msg, size);
    } else if (!is_note_message(msg, size)) {
        append_midi(self->midi_out, self->urids.midi_Event, ev->time.frames, msg, size);
    }
}

static void sync_harmony_to_position(Cadence* self,
                                     uint32_t frame,
                                     double abs_beats,
                                     double beats_per_bar,
                                     int segment_count) {
    if (self->ready && self->playback_segment_count == segment_count) {
        const int segment = segment_index_for_time(self->controls, beats_per_bar, segment_count, abs_beats + kBeatEpsilon);
        transition_to_slot(self, frame, &self->playback[segment], false);
    } else {
        silence_harmony(self, frame);
    }
}

static LV2_State_Status cadence_state_save(
    LV2_Handle instance,
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t /*flags*/,
    const LV2_Feature* const* /*features*/) {
    Cadence* self = (Cadence*)instance;
    if (!self || !store) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    ProgressionStateBlob blob{};
    blob.version = 1;
    blob.segment_count = self->playback_segment_count;
    blob.ready = self->ready ? 1 : 0;

    for (int i = 0; i < self->playback_segment_count && i < kMaxSegments; ++i) {
        const ChordSlot& slot = self->playback[i];
        SavedChordSlot& saved = blob.slots[i];
        saved.valid = slot.valid ? 1 : 0;
        saved.root_pc = slot.root_pc;
        saved.quality = slot.quality;
        saved.note_count = slot.note_count;
        saved.velocity = slot.velocity;
        for (uint8_t j = 0; j < slot.note_count && j < kMaxChordNotes; ++j) {
            saved.notes[j] = slot.notes[j];
        }
    }

    const uint32_t flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;
    store(handle,
          self->urids.state_progression,
          &blob,
          sizeof(blob),
          self->urids.atom_Chunk,
          flags);
    return LV2_STATE_SUCCESS;
}

static LV2_State_Status cadence_state_restore(
    LV2_Handle instance,
    LV2_State_Retrieve_Function retrieve,
    LV2_State_Handle handle,
    uint32_t /*flags*/,
    const LV2_Feature* const* /*features*/) {
    Cadence* self = (Cadence*)instance;
    if (!self || !retrieve) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    size_t size = 0;
    uint32_t type = 0;
    uint32_t val_flags = 0;
    const void* data = retrieve(handle, self->urids.state_progression, &size, &type, &val_flags);
    if (!data || size != sizeof(ProgressionStateBlob) || type != self->urids.atom_Chunk) {
        return LV2_STATE_SUCCESS;
    }

    const ProgressionStateBlob* blob = (const ProgressionStateBlob*)data;
    if (blob->version != 1) {
        return LV2_STATE_SUCCESS;
    }

    clear_playback(self);
    self->playback_segment_count = clampi(blob->segment_count, 0, kMaxSegments);
    self->ready = blob->ready != 0 && self->playback_segment_count > 0;

    for (int i = 0; i < self->playback_segment_count; ++i) {
        const SavedChordSlot& saved = blob->slots[i];
        ChordSlot& slot = self->playback[i];
        slot.valid = saved.valid != 0;
        slot.root_pc = (uint8_t)clampi(saved.root_pc, 0, 11);
        slot.quality = (uint8_t)clampi(saved.quality, 0, QUALITY_MIN7);
        slot.note_count = (uint8_t)clampi(saved.note_count, 0, kMaxChordNotes);
        slot.velocity = (uint8_t)clampi(saved.velocity, 1, 127);
        for (uint8_t j = 0; j < slot.note_count; ++j) {
            slot.notes[j] = (uint8_t)clampi(saved.notes[j], 0, 127);
        }
        sort_notes(slot.notes, slot.note_count);
    }

    return LV2_STATE_SUCCESS;
}

static const LV2_State_Interface cadence_state_interface = {
    cadence_state_save,
    cadence_state_restore
};

static LV2_Handle instantiate(const LV2_Descriptor* /*descriptor*/,
                              double rate,
                              const char* /*bundle_path*/,
                              const LV2_Feature* const* features) {
    Cadence* self = new Cadence();
    if (!self) {
        return nullptr;
    }

    self->sample_rate = rate;

    for (int i = 0; features[i]; ++i) {
        if (!std::strcmp(features[i]->URI, LV2_URID__map)) {
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
    self->urids.state_progression = self->map->map(self->map->handle, CADENCE_URI "#progression");

    clear_capture(self);
    clear_playback(self);
    clear_held_notes(self);
    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    Cadence* self = (Cadence*)instance;
    if (!self) {
        return;
    }

    switch (port) {
        case PORT_CONTROL: self->control = (const LV2_Atom_Sequence*)data; break;
        case PORT_MIDI_IN: self->midi_in = (const LV2_Atom_Sequence*)data; break;
        case PORT_MIDI_OUT: self->midi_out = (LV2_Atom_Sequence*)data; break;
        case PORT_KEY: self->key_port = (const float*)data; break;
        case PORT_SCALE: self->scale_port = (const float*)data; break;
        case PORT_CYCLE_BARS: self->cycle_bars_port = (const float*)data; break;
        case PORT_GRANULARITY: self->granularity_port = (const float*)data; break;
        case PORT_COMPLEXITY: self->complexity_port = (const float*)data; break;
        case PORT_CHORD_SIZE: self->chord_size_port = (const float*)data; break;
        case PORT_REGISTER: self->register_port = (const float*)data; break;
        case PORT_SPREAD: self->spread_port = (const float*)data; break;
        case PORT_PASS_INPUT: self->pass_input_port = (const float*)data; break;
        case PORT_OUTPUT_CHANNEL: self->output_channel_port = (const float*)data; break;
        case PORT_ACTION_LEARN: self->action_learn_port = (const float*)data; break;
        case PORT_STATUS_READY: self->status_ready_port = (float*)data; break;
        default: break;
    }
}

static void activate(LV2_Handle instance) {
    Cadence* self = (Cadence*)instance;
    if (!self) {
        return;
    }

    clear_capture(self);
    clear_held_notes(self);
    self->active_harmony_count = 0;
    self->active_harmony_channel = 1;
    self->last_input_channel = 1;
    self->was_playing = false;
    self->last_abs_beats_start = 0.0;
    self->controls_initialized = false;
}

static void run(LV2_Handle instance, uint32_t nframes) {
    Cadence* self = (Cadence*)instance;
    if (!self || !self->midi_out) {
        return;
    }

    self->controls = read_controls(self);

    if (!self->controls_initialized) {
        self->previous_controls = self->controls;
        self->controls_initialized = true;
    }

    const bool learn_triggered = self->controls.action_learn != self->previous_controls.action_learn;
    const bool params_changed = !controls_match(self->controls, self->previous_controls);

    if (learn_triggered || params_changed) {
        silence_harmony(self, 0);
        reset_learning(self);
    }
    self->previous_controls = self->controls;

    self->midi_out->atom.type = self->urids.atom_Sequence;
    self->midi_out->atom.size = sizeof(LV2_Atom_Sequence_Body);
    self->midi_out->body.unit = 0;
    self->midi_out->body.pad = 0;

    const TimeInfo info = read_time_info(self->control, self->urids);

    if (!info.valid || !info.playing) {
        silence_harmony(self, 0);
        clear_held_notes(self);
        self->was_playing = false;

        if (self->midi_in) {
            LV2_ATOM_SEQUENCE_FOREACH(self->midi_in, ev) {
                if (ev->body.type != self->urids.midi_Event) {
                    continue;
                }
                const uint8_t* msg = (const uint8_t*)(ev + 1);
                forward_input_event(self, ev, msg, ev->body.size);
            }
        }

        if (self->status_ready_port) {
            *self->status_ready_port = self->ready ? 1.0f : 0.0f;
        }
        return;
    }

    const int segment_count = segment_count_for_controls(self->controls, info.beatsPerBar);
    if (self->ready && self->playback_segment_count != segment_count) {
        silence_harmony(self, 0);
        clear_playback(self);
    }

    const double abs_beats_start = info.bar * info.beatsPerBar + info.barBeat;
    const double abs_beats_step = ((double)nframes * info.bpm) / (60.0 * self->sample_rate);
    const double abs_beats_end = abs_beats_start + abs_beats_step;
    const double segment_beats = segment_beats_for_controls(self->controls, info.beatsPerBar);
    const bool restart = !self->was_playing || (abs_beats_start + kBeatEpsilon < self->last_abs_beats_start);
    const bool on_boundary_at_start = std::fabs(std::fmod(abs_beats_start, segment_beats)) < kBeatEpsilon ||
                                      std::fabs(std::fmod(abs_beats_start, segment_beats) - segment_beats) < kBeatEpsilon;

    if (restart && !on_boundary_at_start) {
        sync_harmony_to_position(self, 0, abs_beats_start, info.beatsPerBar, segment_count);
    }

    int64_t boundary_index = (int64_t)std::ceil((abs_beats_start - kBeatEpsilon) / segment_beats);
    double next_boundary = (double)boundary_index * segment_beats;

    double cursor_beats = abs_beats_start;

    if (self->midi_in) {
        LV2_ATOM_SEQUENCE_FOREACH(self->midi_in, ev) {
            if (ev->body.type != self->urids.midi_Event) {
                continue;
            }

            const uint8_t* msg = (const uint8_t*)(ev + 1);
            const uint32_t size = ev->body.size;
            if (size < 2) {
                continue;
            }
            if ((msg[0] & 0xF0) != 0xF0) {
                self->last_input_channel = (int)((msg[0] & 0x0F) + 1);
            }

            const double event_beats = abs_beats_start + ((double)ev->time.frames / (double)std::max(1u, nframes)) * abs_beats_step;
            while (next_boundary <= event_beats + kBeatEpsilon && next_boundary <= abs_beats_end + kBeatEpsilon) {
                capture_interval(self, info.beatsPerBar, segment_count, cursor_beats, next_boundary);
                const uint32_t frame = frame_for_beat(abs_beats_start, abs_beats_end, nframes, next_boundary);
                handle_boundary(self, frame, next_boundary, info.beatsPerBar, segment_count);
                cursor_beats = next_boundary;
                boundary_index += 1;
                next_boundary = (double)boundary_index * segment_beats;
            }

            capture_interval(self, info.beatsPerBar, segment_count, cursor_beats, event_beats);
            cursor_beats = event_beats;

            forward_input_event(self, ev, msg, size);

            const uint8_t type = msg[0] & 0xF0;
            if ((type == 0x90 || type == 0x80) && size >= 3) {
                const uint8_t note = msg[1] & 0x7F;
                const uint8_t vel = msg[2] & 0x7F;
                const bool is_note_on = (type == 0x90) && vel > 0;
                if (is_note_on) {
                    self->held_notes[note] = true;
                    self->held_velocity[note] = vel;
                    capture_onset(self, info.beatsPerBar, segment_count, event_beats, note, vel);
                } else {
                    self->held_notes[note] = false;
                    self->held_velocity[note] = 0;
                }
            }
        }
    }

    while (next_boundary <= abs_beats_end + kBeatEpsilon) {
        capture_interval(self, info.beatsPerBar, segment_count, cursor_beats, next_boundary);
        const uint32_t frame = frame_for_beat(abs_beats_start, abs_beats_end, nframes, next_boundary);
        handle_boundary(self, frame, next_boundary, info.beatsPerBar, segment_count);
        cursor_beats = next_boundary;
        boundary_index += 1;
        next_boundary = (double)boundary_index * segment_beats;
    }

    capture_interval(self, info.beatsPerBar, segment_count, cursor_beats, abs_beats_end);

    self->was_playing = true;
    self->last_abs_beats_start = abs_beats_start;

    if (self->status_ready_port) {
        *self->status_ready_port = self->ready ? 1.0f : 0.0f;
    }
}

static void deactivate(LV2_Handle instance) {
    Cadence* self = (Cadence*)instance;
    if (!self) {
        return;
    }
    clear_held_notes(self);
    self->active_harmony_count = 0;
    self->active_harmony_channel = 1;
    self->was_playing = false;
}

static void cleanup(LV2_Handle instance) {
    Cadence* self = (Cadence*)instance;
    delete self;
}

static const void* extension_data(const char* uri) {
    if (!std::strcmp(uri, LV2_STATE__interface)) {
        return &cadence_state_interface;
    }
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    CADENCE_URI,
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
    return (index == 0) ? &descriptor : nullptr;
}
