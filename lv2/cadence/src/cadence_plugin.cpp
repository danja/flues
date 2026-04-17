#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/state/state.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>

#include "cadence_harmony.hpp"
#include "cadence_schema.h"
#include "cadence_state.hpp"
#include "cadence_transport.hpp"
#include "cadence_urid.hpp"
#include "cadence_variation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#define CADENCE_URI "https://danja.github.io/flues/plugins/cadence"

static constexpr uint32_t kBufferCapacity = 16384;

struct Cadence {
    const LV2_Atom_Sequence* control = nullptr;
    const LV2_Atom_Sequence* midi_in = nullptr;
    LV2_Atom_Sequence* midi_out = nullptr;

    const float* key_port = nullptr;
    const float* scale_port = nullptr;
    const float* cycle_bars_port = nullptr;
    const float* granularity_port = nullptr;
    const float* complexity_port = nullptr;
    const float* movement_port = nullptr;
    const float* chord_size_port = nullptr;
    const float* note_length_port = nullptr;
    const float* register_port = nullptr;
    const float* spread_port = nullptr;
    const float* pass_input_port = nullptr;
    const float* output_channel_port = nullptr;
    const float* action_learn_port = nullptr;
    float* status_ready_port = nullptr;
    const float* vary_port = nullptr;

    LV2_URID_Map* map = nullptr;
    CadenceURIDs urids{};

    double sample_rate = 48000.0;

    ControlSnapshot controls = cadence_default_controls();
    ControlSnapshot previous_controls = cadence_default_controls();
    bool controls_initialized = false;

    SegmentCapture capture[CADENCE_MAX_SEGMENTS]{};
    SegmentCapture learned_capture[CADENCE_MAX_SEGMENTS]{};
    int learned_segment_count = 0;
    bool have_learned_capture = false;

    ChordSlot base_progression[CADENCE_MAX_SEGMENTS]{};
    int base_segment_count = 0;
    ChordSlot playback[CADENCE_MAX_SEGMENTS]{};
    int playback_segment_count = 0;
    bool ready = false;

    bool held_notes[128]{};
    uint8_t held_velocity[128]{};

    uint8_t active_harmony_notes[CADENCE_MAX_CHORD_NOTES]{};
    uint8_t active_harmony_count = 0;
    int active_harmony_channel = 1;
    int last_input_channel = 1;
    bool harmony_off_pending = false;
    double pending_harmony_off_beat = 0.0;

    VariationStateBlob variation = cadence_default_variation_state();
    bool was_playing = false;
    double last_abs_beats_start = 0.0;
};

namespace {

inline int clampi(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

void append_midi(LV2_Atom_Sequence* seq,
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

int resolve_output_channel(const Cadence* self, int configured_channel) {
    if (configured_channel == 0) {
        return clampi(self ? self->last_input_channel : 1, 1, 16);
    }
    return clampi(configured_channel, 1, 16);
}

void emit_note_on_on_channel(Cadence* self, uint32_t frame, int note, int velocity, int output_channel) {
    const uint8_t channel = (uint8_t)(clampi(output_channel, 1, 16) - 1);
    const uint8_t msg[3] = {
        (uint8_t)(0x90 | channel),
        (uint8_t)clampi(note, 0, 127),
        (uint8_t)clampi(velocity, 1, 127)
    };
    append_midi(self->midi_out, self->urids.midi_Event, frame, msg, 3);
}

void emit_note_off_on_channel(Cadence* self, uint32_t frame, int note, int output_channel) {
    const uint8_t channel = (uint8_t)(clampi(output_channel, 1, 16) - 1);
    const uint8_t msg[3] = {
        (uint8_t)(0x80 | channel),
        (uint8_t)clampi(note, 0, 127),
        0
    };
    append_midi(self->midi_out, self->urids.midi_Event, frame, msg, 3);
}

void emit_note_off(Cadence* self, uint32_t frame, int note) {
    emit_note_off_on_channel(self, frame, note, self->active_harmony_channel);
}

void emit_cc_on_channel(Cadence* self,
                        uint32_t frame,
                        int controller,
                        int value,
                        int output_channel) {
    const uint8_t channel = (uint8_t)(clampi(output_channel, 1, 16) - 1);
    const uint8_t msg[3] = {
        (uint8_t)(0xB0 | channel),
        (uint8_t)clampi(controller, 0, 127),
        (uint8_t)clampi(value, 0, 127)
    };
    append_midi(self->midi_out, self->urids.midi_Event, frame, msg, 3);
}

void clear_pending_harmony_off(Cadence* self) {
    self->harmony_off_pending = false;
    self->pending_harmony_off_beat = 0.0;
}

void schedule_harmony_off_at(Cadence* self, double abs_off_beat) {
    clear_pending_harmony_off(self);
    if (self->active_harmony_count == 0 || abs_off_beat <= 0.0) {
        return;
    }

    if (cadence_note_length_fraction(self->controls) >= 0.995) {
        return;
    }

    self->harmony_off_pending = true;
    self->pending_harmony_off_beat = abs_off_beat;
}

void schedule_harmony_off_for_segment(Cadence* self,
                                      double segment_start_beat,
                                      double segment_beats) {
    const double fraction = cadence_note_length_fraction(self->controls);
    if (fraction >= 0.995) {
        clear_pending_harmony_off(self);
        return;
    }
    schedule_harmony_off_at(self, segment_start_beat + segment_beats * fraction);
}

void clear_held_notes(Cadence* self) {
    std::memset(self->held_notes, 0, sizeof(self->held_notes));
    std::memset(self->held_velocity, 0, sizeof(self->held_velocity));
}

bool note_in_set(const uint8_t* notes, uint8_t count, uint8_t note) {
    for (uint8_t i = 0; i < count; ++i) {
        if (notes[i] == note) {
            return true;
        }
    }
    return false;
}

void transition_to_slot(Cadence* self, uint32_t frame, const ChordSlot* slot, bool retrigger) {
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

void silence_harmony(Cadence* self, uint32_t frame) {
    clear_pending_harmony_off(self);
    transition_to_slot(self, frame, nullptr, false);
}

void panic_harmony(Cadence* self, uint32_t frame) {
    for (uint8_t i = 0; i < self->active_harmony_count; ++i) {
        emit_note_off(self, frame, self->active_harmony_notes[i]);
    }

    const int current_channel = clampi(self->active_harmony_channel, 1, 16);
    emit_cc_on_channel(self, frame, 123, 0, current_channel);
    emit_cc_on_channel(self, frame, 120, 0, current_channel);

    const int configured_channel = resolve_output_channel(self, self->controls.output_channel);
    if (configured_channel != current_channel) {
        emit_cc_on_channel(self, frame, 123, 0, configured_channel);
        emit_cc_on_channel(self, frame, 120, 0, configured_channel);
    }

    self->active_harmony_count = 0;
    clear_pending_harmony_off(self);
}

bool controls_match(const ControlSnapshot& a, const ControlSnapshot& b) {
    return a.key == b.key &&
           a.scale == b.scale &&
           a.cycle_bars == b.cycle_bars &&
           a.granularity == b.granularity &&
           std::fabs(a.complexity - b.complexity) < 0.0001f &&
           std::fabs(a.movement - b.movement) < 0.0001f &&
           a.chord_size == b.chord_size &&
           a.reg == b.reg &&
           a.spread == b.spread;
}

ControlSnapshot read_controls(Cadence* self) {
    ControlSnapshot controls = cadence_default_controls();
    controls.key = clampi((int)lroundf(self->key_port ? *self->key_port : (float)CADENCE_DEFAULT_KEY), 0, 11);
    controls.scale = clampi((int)lroundf(self->scale_port ? *self->scale_port : (float)CADENCE_DEFAULT_SCALE), 0, SCALE_COUNT - 1);
    controls.cycle_bars = clampi((int)lroundf(self->cycle_bars_port ? *self->cycle_bars_port : (float)CADENCE_DEFAULT_CYCLE_BARS), 1, 8);
    controls.granularity = clampi((int)lroundf(self->granularity_port ? *self->granularity_port : (float)CADENCE_DEFAULT_GRANULARITY), 0, 2);
    controls.complexity = self->complexity_port ? *self->complexity_port : CADENCE_DEFAULT_COMPLEXITY;
    controls.movement = self->movement_port ? *self->movement_port : CADENCE_DEFAULT_MOVEMENT;
    controls.chord_size = clampi((int)lroundf(self->chord_size_port ? *self->chord_size_port : (float)CADENCE_DEFAULT_CHORD_SIZE), 0, 1);
    controls.note_length = self->note_length_port ? *self->note_length_port : CADENCE_DEFAULT_NOTE_LENGTH;
    controls.reg = clampi((int)lroundf(self->register_port ? *self->register_port : (float)CADENCE_DEFAULT_REGISTER), 0, 2);
    controls.spread = clampi((int)lroundf(self->spread_port ? *self->spread_port : (float)CADENCE_DEFAULT_SPREAD), 0, 2);
    controls.pass_input = (self->pass_input_port ? *self->pass_input_port : 1.0f) >= 0.5f;
    controls.output_channel = clampi((int)lroundf(self->output_channel_port ? *self->output_channel_port : 0.0f), 0, 16);
    controls.action_learn = clampi((int)lroundf(self->action_learn_port ? *self->action_learn_port : 0.0f), 0, 1048576);
    controls.vary = (self->vary_port ? *self->vary_port : 0.0f) / 100.0f;
    return controls;
}

void clear_learning_state(Cadence* self) {
    cadence_clear_capture(self->capture, CADENCE_MAX_SEGMENTS);
    cadence_clear_capture(self->learned_capture, CADENCE_MAX_SEGMENTS);
    self->learned_segment_count = 0;
    self->have_learned_capture = false;

    cadence_clear_progression(self->base_progression, CADENCE_MAX_SEGMENTS);
    self->base_segment_count = 0;
    cadence_clear_progression(self->playback, CADENCE_MAX_SEGMENTS);
    self->playback_segment_count = 0;
    self->ready = false;

    clear_held_notes(self);
    clear_pending_harmony_off(self);
    self->active_harmony_count = 0;
    self->active_harmony_channel = 1;
    self->last_input_channel = 1;
    cadence_reset_variation_progress(&self->variation);
}

void adopt_base_progression(Cadence* self) {
    cadence_clear_progression(self->playback, CADENCE_MAX_SEGMENTS);
    if (self->base_segment_count > 0) {
        cadence_copy_progression(self->playback, self->base_progression, self->base_segment_count);
    }
    self->playback_segment_count = self->base_segment_count;
}

bool update_base_from_capture(Cadence* self, int segment_count) {
    ChordSlot built[CADENCE_MAX_SEGMENTS]{};
    const CadenceBuildOptions base_options{};
    if (!cadence_build_progression_from_capture(self->capture,
                                                segment_count,
                                                self->controls,
                                                nullptr,
                                                0,
                                                base_options,
                                                built)) {
        return false;
    }

    cadence_clear_progression(self->base_progression, CADENCE_MAX_SEGMENTS);
    cadence_copy_progression(self->base_progression, built, segment_count);
    self->base_segment_count = segment_count;
    self->ready = true;

    cadence_clear_capture(self->learned_capture, CADENCE_MAX_SEGMENTS);
    cadence_copy_capture(self->learned_capture, self->capture, segment_count);
    self->learned_segment_count = segment_count;
    self->have_learned_capture = true;
    return true;
}

void capture_interval(Cadence* self,
                      double beats_per_bar,
                      int segment_count,
                      double abs_start,
                      double abs_end) {
    if (abs_end <= abs_start + 1e-12 || segment_count <= 0) {
        return;
    }

    const int segment = cadence_segment_index_for_time(self->controls,
                                                       beats_per_bar,
                                                       segment_count,
                                                       abs_start + CADENCE_BEAT_EPSILON);
    for (int note = 0; note < 128; ++note) {
        if (!self->held_notes[note]) {
            continue;
        }
        self->capture[segment].duration[note % 12] += abs_end - abs_start;
    }
}

void capture_onset(Cadence* self,
                   double beats_per_bar,
                   int segment_count,
                   double abs_beats,
                   uint8_t note,
                   uint8_t velocity) {
    if (segment_count <= 0) {
        return;
    }

    const int segment = cadence_segment_index_for_time(self->controls,
                                                       beats_per_bar,
                                                       segment_count,
                                                       abs_beats + CADENCE_BEAT_EPSILON);
    const double segment_beats = cadence_segment_beats_for_controls(self->controls, beats_per_bar);
    const double cycle_pos = cadence_wrapped_cycle_position(abs_beats, self->controls, beats_per_bar);
    const double segment_pos = std::fmod(cycle_pos, segment_beats);
    double bonus = 0.35 + ((double)velocity / 127.0) * 0.65;

    if (segment_pos < std::max(0.12, segment_beats * 0.15)) {
        bonus *= 1.15;
    }

    self->capture[segment].onset[note % 12] += bonus;
}

void handle_boundary(Cadence* self,
                     uint32_t frame,
                     double abs_boundary_beat,
                     double beats_per_bar,
                     int segment_count) {
    const double segment_beats = cadence_segment_beats_for_controls(self->controls, beats_per_bar);
    const int64_t boundary_index = (int64_t)llround(abs_boundary_beat / segment_beats);
    const bool cycle_boundary = segment_count > 0 ? ((boundary_index % segment_count) == 0) : false;

    bool base_updated = false;
    if (cycle_boundary) {
        base_updated = update_base_from_capture(self, segment_count);
        cadence_clear_capture(self->capture, CADENCE_MAX_SEGMENTS);

        if (self->ready && self->base_segment_count == segment_count) {
            ChordSlot varied[CADENCE_MAX_SEGMENTS]{};
            const bool mutated = cadence_apply_cycle_variation(self->have_learned_capture ? self->learned_capture : nullptr,
                                                               self->learned_segment_count,
                                                               self->controls,
                                                               &self->variation,
                                                               self->base_progression,
                                                               self->base_segment_count,
                                                               self->playback,
                                                               self->playback_segment_count,
                                                               varied);
            if (mutated) {
                cadence_clear_progression(self->playback, CADENCE_MAX_SEGMENTS);
                cadence_copy_progression(self->playback, varied, self->base_segment_count);
                self->playback_segment_count = self->base_segment_count;
            } else if (base_updated || self->playback_segment_count != self->base_segment_count) {
                adopt_base_progression(self);
            }
        }
    }

    if (self->ready && self->playback_segment_count == segment_count) {
        const int segment = cadence_segment_index_for_time(self->controls,
                                                           beats_per_bar,
                                                           segment_count,
                                                           abs_boundary_beat + CADENCE_BEAT_EPSILON);
        transition_to_slot(self, frame, &self->playback[segment], true);
        schedule_harmony_off_for_segment(self, abs_boundary_beat, segment_beats);
    } else {
        silence_harmony(self, frame);
    }
}

bool is_note_message(const uint8_t* msg, uint32_t size) {
    if (!msg || size < 2) {
        return false;
    }
    const uint8_t type = msg[0] & 0xF0;
    return (type == 0x80 || type == 0x90) && size >= 3;
}

void forward_input_event(Cadence* self, const LV2_Atom_Event* ev, const uint8_t* msg, uint32_t size) {
    if (self->controls.pass_input) {
        append_midi(self->midi_out, self->urids.midi_Event, ev->time.frames, msg, size);
    } else if (!is_note_message(msg, size)) {
        append_midi(self->midi_out, self->urids.midi_Event, ev->time.frames, msg, size);
    }
}

void sync_harmony_to_position(Cadence* self,
                              uint32_t frame,
                              double abs_beats,
                              double beats_per_bar,
                              int segment_count) {
    if (self->ready && self->playback_segment_count == segment_count) {
        const int segment = cadence_segment_index_for_time(self->controls,
                                                           beats_per_bar,
                                                           segment_count,
                                                           abs_beats + CADENCE_BEAT_EPSILON);
        const double segment_beats = cadence_segment_beats_for_controls(self->controls, beats_per_bar);
        const double segment_start = std::floor((abs_beats + CADENCE_BEAT_EPSILON) / segment_beats) * segment_beats;
        const double off_beat = segment_start + segment_beats * cadence_note_length_fraction(self->controls);

        if (cadence_note_length_fraction(self->controls) < 0.995 &&
            off_beat <= abs_beats + CADENCE_BEAT_EPSILON) {
            silence_harmony(self, frame);
            return;
        }

        transition_to_slot(self, frame, &self->playback[segment], false);
        schedule_harmony_off_at(self, off_beat);
    } else {
        silence_harmony(self, frame);
    }
}

void process_timeline_until(Cadence* self,
                            uint32_t nframes,
                            double abs_beats_start,
                            double abs_beats_end,
                            double target_beat,
                            double beats_per_bar,
                            int segment_count,
                            double* cursor_beats,
                            int64_t* boundary_index,
                            double* next_boundary) {
    if (target_beat < *cursor_beats) {
        target_beat = *cursor_beats;
    }

    const double segment_beats = cadence_segment_beats_for_controls(self->controls, beats_per_bar);

    while (true) {
        const bool boundary_due = (*next_boundary <= target_beat + CADENCE_BEAT_EPSILON);
        const bool off_due = self->harmony_off_pending &&
                             (self->pending_harmony_off_beat <= target_beat + CADENCE_BEAT_EPSILON);
        if (!boundary_due && !off_due) {
            break;
        }

        double marker_beat = target_beat;
        bool process_boundary_event = false;
        if (boundary_due && (!off_due || *next_boundary <= self->pending_harmony_off_beat + CADENCE_BEAT_EPSILON)) {
            marker_beat = *next_boundary;
            process_boundary_event = true;
        } else {
            marker_beat = self->pending_harmony_off_beat;
        }

        if (marker_beat > *cursor_beats + 1e-12) {
            capture_interval(self, beats_per_bar, segment_count, *cursor_beats, marker_beat);
        }

        const uint32_t frame = cadence_frame_for_beat(abs_beats_start, abs_beats_end, nframes, marker_beat);
        if (process_boundary_event) {
            handle_boundary(self, frame, marker_beat, beats_per_bar, segment_count);
            *boundary_index += 1;
            *next_boundary = (double)(*boundary_index) * segment_beats;
        } else {
            silence_harmony(self, frame);
        }
        *cursor_beats = marker_beat;
    }

    if (target_beat > *cursor_beats + 1e-12) {
        capture_interval(self, beats_per_bar, segment_count, *cursor_beats, target_beat);
        *cursor_beats = target_beat;
    }
}

}  // namespace

static LV2_State_Status cadence_state_save_cb(LV2_Handle instance,
                                              LV2_State_Store_Function store,
                                              LV2_State_Handle handle,
                                              uint32_t flags,
                                              const LV2_Feature* const* features) {
    (void)flags;
    (void)features;

    Cadence* self = (Cadence*)instance;
    if (!self) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    return cadence_save_state(store,
                              handle,
                              self->urids,
                              self->playback,
                              self->playback_segment_count,
                              self->ready,
                              self->variation);
}

static LV2_State_Status cadence_state_restore_cb(LV2_Handle instance,
                                                 LV2_State_Retrieve_Function retrieve,
                                                 LV2_State_Handle handle,
                                                 uint32_t flags,
                                                 const LV2_Feature* const* features) {
    (void)flags;
    (void)features;

    Cadence* self = (Cadence*)instance;
    if (!self) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    LV2_State_Status status = cadence_restore_state(retrieve,
                                                    handle,
                                                    self->urids,
                                                    self->playback,
                                                    &self->playback_segment_count,
                                                    &self->ready,
                                                    &self->variation);
    cadence_clear_progression(self->base_progression, CADENCE_MAX_SEGMENTS);
    if (self->playback_segment_count > 0) {
        cadence_copy_progression(self->base_progression, self->playback, self->playback_segment_count);
    }
    self->base_segment_count = self->playback_segment_count;
    cadence_clear_capture(self->capture, CADENCE_MAX_SEGMENTS);
    cadence_clear_capture(self->learned_capture, CADENCE_MAX_SEGMENTS);
    self->learned_segment_count = 0;
    self->have_learned_capture = false;
    return status;
}

static const LV2_State_Interface cadence_state_interface = {
    cadence_state_save_cb,
    cadence_state_restore_cb
};

static LV2_Handle instantiate(const LV2_Descriptor* descriptor,
                              double rate,
                              const char* bundle_path,
                              const LV2_Feature* const* features) {
    (void)descriptor;
    (void)bundle_path;

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
    self->urids.state_variation = self->map->map(self->map->handle, CADENCE_URI "#variation");

    cadence_clear_capture(self->capture, CADENCE_MAX_SEGMENTS);
    cadence_clear_capture(self->learned_capture, CADENCE_MAX_SEGMENTS);
    cadence_clear_progression(self->base_progression, CADENCE_MAX_SEGMENTS);
    cadence_clear_progression(self->playback, CADENCE_MAX_SEGMENTS);
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
        case PORT_MOVEMENT: self->movement_port = (const float*)data; break;
        case PORT_CHORD_SIZE: self->chord_size_port = (const float*)data; break;
        case PORT_NOTE_LENGTH: self->note_length_port = (const float*)data; break;
        case PORT_REGISTER: self->register_port = (const float*)data; break;
        case PORT_SPREAD: self->spread_port = (const float*)data; break;
        case PORT_PASS_INPUT: self->pass_input_port = (const float*)data; break;
        case PORT_OUTPUT_CHANNEL: self->output_channel_port = (const float*)data; break;
        case PORT_ACTION_LEARN: self->action_learn_port = (const float*)data; break;
        case PORT_STATUS_READY: self->status_ready_port = (float*)data; break;
        case PORT_VARY: self->vary_port = (const float*)data; break;
        default: break;
    }
}

static void activate(LV2_Handle instance) {
    Cadence* self = (Cadence*)instance;
    if (!self) {
        return;
    }

    cadence_clear_capture(self->capture, CADENCE_MAX_SEGMENTS);
    clear_held_notes(self);
    self->active_harmony_count = 0;
    self->active_harmony_channel = 1;
    self->last_input_channel = 1;
    clear_pending_harmony_off(self);
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
    const bool vary_changed = std::fabs(self->controls.vary - self->previous_controls.vary) >= 0.0001f;

    if (learn_triggered || params_changed) {
        silence_harmony(self, 0);
        clear_learning_state(self);
    } else if (vary_changed) {
        cadence_reset_variation_progress(&self->variation);
    }
    self->previous_controls = self->controls;

    self->midi_out->atom.type = self->urids.atom_Sequence;
    self->midi_out->atom.size = sizeof(LV2_Atom_Sequence_Body);
    self->midi_out->body.unit = 0;
    self->midi_out->body.pad = 0;

    const TimeInfo info = cadence_read_time_info(self->control, self->urids);
    if (!info.valid || !info.playing) {
        if (self->was_playing || self->active_harmony_count > 0 || self->harmony_off_pending) {
            panic_harmony(self, 0);
        } else {
            clear_pending_harmony_off(self);
            silence_harmony(self, 0);
        }
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

    const int segment_count = cadence_segment_count_for_controls(self->controls, info.beatsPerBar);
    const bool mismatch =
        (self->playback_segment_count > 0 && self->playback_segment_count != segment_count) ||
        (self->base_segment_count > 0 && self->base_segment_count != segment_count);
    if (mismatch) {
        silence_harmony(self, 0);
        cadence_clear_capture(self->capture, CADENCE_MAX_SEGMENTS);
        cadence_clear_capture(self->learned_capture, CADENCE_MAX_SEGMENTS);
        cadence_clear_progression(self->base_progression, CADENCE_MAX_SEGMENTS);
        cadence_clear_progression(self->playback, CADENCE_MAX_SEGMENTS);
        self->base_segment_count = 0;
        self->playback_segment_count = 0;
        self->ready = false;
        self->have_learned_capture = false;
        self->learned_segment_count = 0;
        cadence_reset_variation_progress(&self->variation);
    }

    const double abs_beats_start = info.bar * info.beatsPerBar + info.barBeat;
    const double abs_beats_step = ((double)nframes * info.bpm) / (60.0 * self->sample_rate);
    const double abs_beats_end = abs_beats_start + abs_beats_step;
    const double segment_beats = cadence_segment_beats_for_controls(self->controls, info.beatsPerBar);
    const bool restart = !self->was_playing || (abs_beats_start + CADENCE_BEAT_EPSILON < self->last_abs_beats_start);
    const double boundary_phase = std::fmod(abs_beats_start, segment_beats);
    const bool on_boundary_at_start = std::fabs(boundary_phase) < CADENCE_BEAT_EPSILON ||
                                      std::fabs(boundary_phase - segment_beats) < CADENCE_BEAT_EPSILON;

    if (restart && !on_boundary_at_start) {
        sync_harmony_to_position(self, 0, abs_beats_start, info.beatsPerBar, segment_count);
    }

    int64_t boundary_index = (int64_t)std::ceil((abs_beats_start - CADENCE_BEAT_EPSILON) / segment_beats);
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

            const double event_beats = abs_beats_start +
                                       ((double)ev->time.frames / (double)std::max(1u, nframes)) * abs_beats_step;
            process_timeline_until(self,
                                   nframes,
                                   abs_beats_start,
                                   abs_beats_end,
                                   event_beats,
                                   info.beatsPerBar,
                                   segment_count,
                                   &cursor_beats,
                                   &boundary_index,
                                   &next_boundary);

            forward_input_event(self, ev, msg, size);

            const uint8_t type = msg[0] & 0xF0;
            if ((type == 0x90 || type == 0x80) && size >= 3) {
                const uint8_t note = msg[1] & 0x7F;
                const uint8_t velocity = msg[2] & 0x7F;
                const bool is_note_on = (type == 0x90) && velocity > 0;
                if (is_note_on) {
                    self->held_notes[note] = true;
                    self->held_velocity[note] = velocity;
                    capture_onset(self, info.beatsPerBar, segment_count, event_beats, note, velocity);
                } else {
                    self->held_notes[note] = false;
                    self->held_velocity[note] = 0;
                }
            }
        }
    }

    process_timeline_until(self,
                           nframes,
                           abs_beats_start,
                           abs_beats_end,
                           abs_beats_end,
                           info.beatsPerBar,
                           segment_count,
                           &cursor_beats,
                           &boundary_index,
                           &next_boundary);

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
    clear_pending_harmony_off(self);
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
