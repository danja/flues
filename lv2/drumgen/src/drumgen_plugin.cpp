#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/state/state.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>

#include "drumgen_pattern.hpp"
#include "drumgen_schema.h"
#include "drumgen_state.hpp"
#include "drumgen_transport.hpp"
#include "drumgen_variation.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>

#define DRUMGEN_URI "https://danja.github.io/flues/plugins/drumgen"

struct TimeInfo {
    bool valid = false;
    bool playing = false;
    double bar = 0.0;
    double barBeat = 0.0;
    double beatsPerBar = 4.0;
    double bpm = 120.0;
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
    LV2_URID atom_Sequence = 0;
    LV2_URID midi_Event = 0;
    LV2_URID time_Position = 0;
    LV2_URID time_speed = 0;
    LV2_URID time_bar = 0;
    LV2_URID time_barBeat = 0;
    LV2_URID time_beatsPerBar = 0;
    LV2_URID time_beatsPerMinute = 0;
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
    const float* tom_amt_port = nullptr;
    const float* metal_amt_port = nullptr;
    const float* vary_port = nullptr;

    LV2_URID_Map* map = nullptr;
    DrumGenURIDs urids{};
    DrumGenStateURIDs state_urids{};
    double sample_rate = 48000.0;

    ControlSnapshot controls = drumgen_default_controls();
    ControlSnapshot previous_controls = drumgen_default_controls();
    PatternStateBlob pattern = drumgen_default_pattern_state();
    VariationStateBlob variation = drumgen_default_variation_state();
    bool pattern_valid = false;

    PendingNoteOff pending_note_offs[DRUMGEN_MAX_PENDING_NOTE_OFFS];
    int64_t last_transport_step = -1;
    bool was_playing = false;
};

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
    for (int i = 0; i < DRUMGEN_MAX_PENDING_NOTE_OFFS; ++i) {
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
    for (int i = 0; i < DRUMGEN_MAX_PENDING_NOTE_OFFS; ++i) {
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
    for (int i = 0; i < DRUMGEN_MAX_PENDING_NOTE_OFFS; ++i) {
        PendingNoteOff& pending = self->pending_note_offs[i];
        if (!pending.active) {
            continue;
        }
        emit_note_off(self, frame, pending.note, pending.channel);
        pending.active = false;
    }
}

static ControlSnapshot read_controls(const DrumGen* self) {
    ControlSnapshot controls = drumgen_default_controls();
    controls.genre = (int)lroundf(self->genre_port ? *self->genre_port : (float)DRUMGEN_DEFAULT_GENRE);
    controls.channel = (int)lroundf(self->channel_port ? *self->channel_port : (float)DRUMGEN_DEFAULT_CHANNEL);
    controls.kit_map = (int)lroundf(self->kit_map_port ? *self->kit_map_port : (float)DRUMGEN_DEFAULT_KIT_MAP);
    controls.bars = (int)lroundf(self->bars_port ? *self->bars_port : (float)DRUMGEN_DEFAULT_BARS);
    controls.resolution = (int)lroundf(self->resolution_port ? *self->resolution_port : (float)DRUMGEN_DEFAULT_RESOLUTION);
    controls.density = self->density_port ? *self->density_port : DRUMGEN_DEFAULT_DENSITY;
    controls.variation = self->variation_port ? *self->variation_port : DRUMGEN_DEFAULT_VARIATION;
    controls.fill = self->fill_port ? *self->fill_port : DRUMGEN_DEFAULT_FILL;
    controls.vary = (self->vary_port ? *self->vary_port : 0.0f) / 100.0f;
    controls.seed = (uint32_t)lroundf(self->seed_port ? *self->seed_port : (float)DRUMGEN_DEFAULT_SEED);
    controls.kick_amt = self->kick_amt_port ? *self->kick_amt_port : DRUMGEN_DEFAULT_KICK_AMT;
    controls.backbeat_amt = self->backbeat_amt_port ? *self->backbeat_amt_port : DRUMGEN_DEFAULT_BACKBEAT_AMT;
    controls.hat_amt = self->hat_amt_port ? *self->hat_amt_port : DRUMGEN_DEFAULT_HAT_AMT;
    controls.aux_amt = self->aux_amt_port ? *self->aux_amt_port : DRUMGEN_DEFAULT_AUX_AMT;
    controls.tom_amt = self->tom_amt_port ? *self->tom_amt_port : DRUMGEN_DEFAULT_TOM_AMT;
    controls.metal_amt = self->metal_amt_port ? *self->metal_amt_port : DRUMGEN_DEFAULT_METAL_AMT;
    controls.action_new = (int)lroundf(self->action_new_port ? *self->action_new_port : 0.0f);
    controls.action_mutate = (int)lroundf(self->action_mutate_port ? *self->action_mutate_port : 0.0f);
    controls.action_fill = (int)lroundf(self->action_fill_port ? *self->action_fill_port : 0.0f);
    return drumgen_clamp_controls(controls);
}

static void update_pattern_if_needed(DrumGen* self, const ControlSnapshot& current) {
    const bool controls_changed = drumgen_structural_controls_changed(current, self->previous_controls);
    const bool new_changed = current.action_new != self->previous_controls.action_new;
    const bool mutate_changed = current.action_mutate != self->previous_controls.action_mutate;
    const bool fill_changed = current.action_fill != self->previous_controls.action_fill;
    const bool vary_changed = fabsf(current.vary - self->previous_controls.vary) >= 0.0001f;

    if (!self->pattern_valid || controls_changed || new_changed || mutate_changed || fill_changed) {
        const bool fill_only_refresh = fill_changed && !controls_changed && !new_changed && !mutate_changed;
        drumgen_regenerate_pattern(&self->pattern, current, fill_only_refresh);
        self->pattern_valid = true;
        drumgen_reset_variation_progress(&self->variation);
    } else if (vary_changed) {
        drumgen_reset_variation_progress(&self->variation);
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

static void emit_step_hits(DrumGen* self,
                           uint32_t frame,
                           int local_step,
                           double samples_per_step,
                           uint32_t nframes) {
    if (!self->pattern_valid || local_step < 0 || local_step >= self->pattern.total_steps) {
        return;
    }

    for (int lane = 0; lane < DRUMGEN_LANE_COUNT; ++lane) {
        const DrumStepCell& cell = self->pattern.lanes[lane].steps[local_step];
        if (cell.velocity == 0) {
            continue;
        }

        const int note = self->pattern.lanes[lane].midi_note;
        const int on_frame = clampi((int)frame + (lane == LANE_OPEN_HAT ? DRUMGEN_SAFETY_GAP_SAMPLES : 0), 0, (int)nframes - 1);
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

static void process_boundary(DrumGen* self,
                             uint32_t nframes,
                             double abs_steps_start,
                             double abs_steps_end,
                             int64_t boundary,
                             double samples_per_step) {
    const uint32_t frame = drumgen_frame_for_boundary(abs_steps_start, abs_steps_end, nframes, boundary);
    const int local_step = drumgen_local_step_for_boundary(self->pattern, boundary);

    if (local_step == 0 &&
        drumgen_apply_loop_variation(&self->pattern, &self->variation, self->controls)) {
        clear_pending_note_offs(self, frame);
    }

    emit_step_hits(self, frame, local_step, samples_per_step, nframes);
}

static LV2_State_Status drumgen_state_save_cb(LV2_Handle instance,
                                              LV2_State_Store_Function store,
                                              LV2_State_Handle handle,
                                              uint32_t flags,
                                              const LV2_Feature* const* features) {
    (void)flags;
    (void)features;

    DrumGen* self = (DrumGen*)instance;
    if (!self) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    return drumgen_save_state(store,
                              handle,
                              self->state_urids,
                              self->controls,
                              self->pattern,
                              self->variation);
}

static LV2_State_Status drumgen_state_restore_cb(LV2_Handle instance,
                                                 LV2_State_Retrieve_Function retrieve,
                                                 LV2_State_Handle handle,
                                                 uint32_t flags,
                                                 const LV2_Feature* const* features) {
    (void)flags;
    (void)features;

    DrumGen* self = (DrumGen*)instance;
    if (!self) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    const LV2_State_Status status = drumgen_restore_state(retrieve,
                                                          handle,
                                                          self->state_urids,
                                                          &self->controls,
                                                          &self->pattern,
                                                          &self->variation,
                                                          &self->pattern_valid);
    self->previous_controls = self->controls;
    return status;
}

static const LV2_State_Interface drumgen_state_interface = {
    drumgen_state_save_cb,
    drumgen_state_restore_cb
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
    self->urids.atom_Sequence = self->map->map(self->map->handle, LV2_ATOM__Sequence);
    self->urids.midi_Event = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);
    self->urids.time_Position = self->map->map(self->map->handle, LV2_TIME__Position);
    self->urids.time_speed = self->map->map(self->map->handle, LV2_TIME__speed);
    self->urids.time_bar = self->map->map(self->map->handle, LV2_TIME__bar);
    self->urids.time_barBeat = self->map->map(self->map->handle, LV2_TIME__barBeat);
    self->urids.time_beatsPerBar = self->map->map(self->map->handle, LV2_TIME__beatsPerBar);
    self->urids.time_beatsPerMinute = self->map->map(self->map->handle, LV2_TIME__beatsPerMinute);

    self->state_urids.atom_chunk = self->map->map(self->map->handle, LV2_ATOM__Chunk);
    self->state_urids.controls = self->map->map(self->map->handle, DRUMGEN_URI "#controls");
    self->state_urids.pattern = self->map->map(self->map->handle, DRUMGEN_URI "#pattern");
    self->state_urids.variation = self->map->map(self->map->handle, DRUMGEN_URI "#variation");

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
        case PORT_TOM_AMT: self->tom_amt_port = (const float*)data; break;
        case PORT_METAL_AMT: self->metal_amt_port = (const float*)data; break;
        case PORT_VARY: self->vary_port = (const float*)data; break;
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
        drumgen_regenerate_pattern(&self->pattern, self->controls, false);
        self->pattern_valid = true;
        drumgen_reset_variation_progress(&self->variation);
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

    if (drumgen_transport_restart_detected(self->was_playing, self->last_transport_step, start_step_floor)) {
        clear_pending_note_offs(self, 0);
        const double wrapped = drumgen_local_step_from_absolute(self->pattern, abs_steps_start);
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
        process_boundary(self, nframes, abs_steps_start, abs_steps_end, boundary, samples_per_step);
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
