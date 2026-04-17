#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/state/state.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>

#include "bassgen_pattern.hpp"
#include "bassgen_schema.h"
#include "bassgen_state.hpp"
#include "bassgen_transport.hpp"
#include "bassgen_variation.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>

#define BASSGEN_URI "https://danja.github.io/flues/plugins/bassgen"

struct TimeInfo {
    bool valid = false;
    bool playing = false;
    double bar = 0.0;
    double barBeat = 0.0;
    double beatsPerBar = 4.0;
    double bpm = 120.0;
};

struct BassGenURIDs {
    LV2_URID atom_Object = 0;
    LV2_URID atom_Blank = 0;
    LV2_URID atom_Float = 0;
    LV2_URID atom_Double = 0;
    LV2_URID atom_Int = 0;
    LV2_URID atom_Long = 0;
    LV2_URID atom_Sequence = 0;
    LV2_URID midi_Event = 0;
    LV2_URID time_Position = 0;
    LV2_URID time_speed = 0;
    LV2_URID time_bar = 0;
    LV2_URID time_barBeat = 0;
    LV2_URID time_beatsPerBar = 0;
    LV2_URID time_beatsPerMinute = 0;
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
    const float* vary_port = nullptr;

    LV2_URID_Map* map = nullptr;
    BassGenURIDs urids{};
    BassGenStateURIDs state_urids{};
    double sample_rate = 48000.0;

    ControlSnapshot controls = bassgen_default_controls();
    ControlSnapshot previous_controls = bassgen_default_controls();
    PatternStateBlob pattern = bassgen_default_pattern_state();
    VariationStateBlob variation = bassgen_default_variation_state();
    bool pattern_valid = false;

    int active_note = -1;
    int64_t last_transport_step = -1;
    bool was_playing = false;
};

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
    ControlSnapshot controls = bassgen_default_controls();
    controls.root_note = (int)lroundf(self->root_note_port ? *self->root_note_port : (float)BASSGEN_DEFAULT_ROOT_NOTE);
    controls.scale = (int)lroundf(self->scale_port ? *self->scale_port : (float)BASSGEN_DEFAULT_SCALE);
    controls.genre = (int)lroundf(self->genre_port ? *self->genre_port : (float)BASSGEN_DEFAULT_GENRE);
    controls.channel = (int)lroundf(self->channel_port ? *self->channel_port : (float)BASSGEN_DEFAULT_CHANNEL);
    controls.length_beats = (int)lroundf(self->length_beats_port ? *self->length_beats_port : (float)BASSGEN_DEFAULT_LENGTH_BEATS);
    controls.subdivision = (int)lroundf(self->subdivision_port ? *self->subdivision_port : (float)BASSGEN_DEFAULT_SUBDIVISION);
    controls.density = self->density_port ? *self->density_port : BASSGEN_DEFAULT_DENSITY;
    controls.reg = (int)lroundf(self->register_port ? *self->register_port : (float)BASSGEN_DEFAULT_REGISTER);
    controls.hold = self->hold_port ? *self->hold_port : BASSGEN_DEFAULT_HOLD;
    controls.accent = self->accent_port ? *self->accent_port : BASSGEN_DEFAULT_ACCENT;
    controls.vary = (self->vary_port ? *self->vary_port : 0.0f) / 100.0f;
    controls.seed = (uint32_t)lroundf(self->seed_port ? *self->seed_port : (float)BASSGEN_DEFAULT_SEED);
    controls.action_new = (int)lroundf(self->action_new_port ? *self->action_new_port : 0.0f);
    controls.action_notes = (int)lroundf(self->action_notes_port ? *self->action_notes_port : 0.0f);
    controls.action_rhythm = (int)lroundf(self->action_rhythm_port ? *self->action_rhythm_port : 0.0f);
    return bassgen_clamp_controls(controls);
}

static void clear_active_note(BassGen* self, uint32_t frame) {
    if (self->active_note >= 0) {
        emit_note_off(self, frame, self->active_note);
        self->active_note = -1;
    }
}

static void sync_note_state_to_position(BassGen* self, double local_step_pos) {
    const NoteEventData* ev = self->pattern_valid ? bassgen_find_active_event(self->pattern, local_step_pos) : nullptr;
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

static void handle_transport_restart(BassGen* self, double abs_steps_start) {
    clear_active_note(self, 0);
    const double local_step = bassgen_local_step_from_absolute(self->pattern, abs_steps_start);
    sync_note_state_to_position(self, local_step);
}

static void prepare_midi_output(BassGen* self) {
    self->midi_out->atom.type = self->urids.atom_Sequence;
    self->midi_out->atom.size = sizeof(LV2_Atom_Sequence_Body);
    self->midi_out->body.unit = 0;
    self->midi_out->body.pad = 0;
}

static void update_pattern_if_needed(BassGen* self, const ControlSnapshot& fresh) {
    const bool params_changed = bassgen_structural_controls_changed(fresh, self->previous_controls);
    const bool trigger_new = fresh.action_new != self->previous_controls.action_new;
    const bool trigger_notes = fresh.action_notes != self->previous_controls.action_notes;
    const bool trigger_rhythm = fresh.action_rhythm != self->previous_controls.action_rhythm;
    const bool vary_just_enabled = self->previous_controls.vary <= 0.0001f && fresh.vary > 0.0001f;

    self->controls = fresh;

    if (!self->pattern_valid || params_changed || trigger_new) {
        bassgen_regenerate_pattern(&self->pattern, self->controls, true, true);
        self->pattern_valid = true;
        bassgen_reset_variation_progress(&self->variation);
    } else if (trigger_rhythm) {
        bassgen_regenerate_pattern(&self->pattern, self->controls, true, false);
        bassgen_reset_variation_progress(&self->variation);
    } else if (trigger_notes) {
        bassgen_regenerate_pattern(&self->pattern, self->controls, false, true);
        bassgen_reset_variation_progress(&self->variation);
    } else if (vary_just_enabled) {
        bassgen_reset_variation_progress(&self->variation);
    }

    self->previous_controls = fresh;
}

static void process_boundary(BassGen* self,
                             uint32_t nframes,
                             double abs_steps_start,
                             double abs_steps_end,
                             int64_t boundary,
                             double beats_per_bar) {
    const uint32_t frame = bassgen_frame_for_boundary(abs_steps_start, abs_steps_end, nframes, boundary);
    const int local_step = bassgen_local_step_for_boundary(self->pattern, boundary);

    if (local_step == 0 &&
        bassgen_apply_loop_variation(&self->pattern, &self->variation, self->controls, beats_per_bar)) {
        clear_active_note(self, frame);
    }

    if (bassgen_any_event_ends_at(self->pattern, local_step)) {
        clear_active_note(self, frame);
    }

    const NoteEventData* start_event = bassgen_find_event_starting_at(self->pattern, local_step);
    if (start_event) {
        const uint32_t on_frame = (uint32_t)clampi((int)frame + BASSGEN_SAFETY_GAP_SAMPLES, 0, (int)nframes - 1);
        clear_active_note(self, frame);
        emit_note_on(self, on_frame, start_event->note, start_event->velocity);
        self->active_note = start_event->note;
    }
}

static LV2_State_Status bassgen_state_save_cb(LV2_Handle instance,
                                              LV2_State_Store_Function store,
                                              LV2_State_Handle handle,
                                              uint32_t flags,
                                              const LV2_Feature* const* features) {
    (void)flags;
    (void)features;

    BassGen* self = (BassGen*)instance;
    if (!self) {
        return LV2_STATE_ERR_UNKNOWN;
    }
    return bassgen_save_state(store,
                              handle,
                              self->state_urids,
                              self->controls,
                              self->pattern,
                              self->variation);
}

static LV2_State_Status bassgen_state_restore_cb(LV2_Handle instance,
                                                 LV2_State_Retrieve_Function retrieve,
                                                 LV2_State_Handle handle,
                                                 uint32_t flags,
                                                 const LV2_Feature* const* features) {
    (void)flags;
    (void)features;

    BassGen* self = (BassGen*)instance;
    if (!self) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    const LV2_State_Status status = bassgen_restore_state(retrieve,
                                                          handle,
                                                          self->state_urids,
                                                          &self->controls,
                                                          &self->pattern,
                                                          &self->variation,
                                                          &self->pattern_valid);
    self->previous_controls = self->controls;
    return status;
}

static const LV2_State_Interface bassgen_state_interface = {
    bassgen_state_save_cb,
    bassgen_state_restore_cb
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
    self->urids.atom_Sequence = self->map->map(self->map->handle, LV2_ATOM__Sequence);
    self->urids.midi_Event = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);
    self->urids.time_Position = self->map->map(self->map->handle, LV2_TIME__Position);
    self->urids.time_speed = self->map->map(self->map->handle, LV2_TIME__speed);
    self->urids.time_bar = self->map->map(self->map->handle, LV2_TIME__bar);
    self->urids.time_barBeat = self->map->map(self->map->handle, LV2_TIME__barBeat);
    self->urids.time_beatsPerBar = self->map->map(self->map->handle, LV2_TIME__beatsPerBar);
    self->urids.time_beatsPerMinute = self->map->map(self->map->handle, LV2_TIME__beatsPerMinute);

    self->state_urids.atom_chunk = self->map->map(self->map->handle, LV2_ATOM__Chunk);
    self->state_urids.controls = self->map->map(self->map->handle, BASSGEN_URI "#controls");
    self->state_urids.pattern = self->map->map(self->map->handle, BASSGEN_URI "#pattern");
    self->state_urids.variation = self->map->map(self->map->handle, BASSGEN_URI "#variation");

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
        case PORT_VARY: self->vary_port = (const float*)data; break;
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
        bassgen_regenerate_pattern(&self->pattern, self->controls, true, true);
        self->pattern_valid = true;
        bassgen_reset_variation_progress(&self->variation);
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

    if (bassgen_transport_restart_detected(self->was_playing, self->last_transport_step, start_step_floor)) {
        handle_transport_restart(self, abs_steps_start);
    }

    self->was_playing = true;
    self->last_transport_step = start_step_floor;

    int64_t boundary = (int64_t)floor(abs_steps_start) + 1;
    const int64_t boundary_end = (int64_t)floor(abs_steps_end + 1e-9);

    while (boundary <= boundary_end) {
        process_boundary(self, nframes, abs_steps_start, abs_steps_end, boundary, info.beatsPerBar);
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
