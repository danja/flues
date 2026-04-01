#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/state/state.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "../include/achord_engine.h"
#include "../include/achord_ui_state.h"
#include "../include/midi_comm.h"
}

#define ACHORD_URI "https://danja.github.io/flues/plugins/achord"

typedef enum {
    PORT_CONTROL_IN = 0,
    PORT_MIDI_OUT = 1,
    PORT_LAUNCHPAD_OUT = 2,
    PORT_AUDIO_OUT_L = 3,
    PORT_AUDIO_OUT_R = 4,
    PORT_PLAY_STATE = 5,
    PORT_CURRENT_STEP = 6,
    PORT_NOTIFY_OUT = 7,
    PORT_STATUS_BANK = 8,
    PORT_STATUS_OCTAVE = 9,
    PORT_STATUS_SCALE = 10,
    PORT_STATUS_REGISTER = 11,
    PORT_STATUS_TRIGGER = 12,
    PORT_STATUS_HOLD = 13,
    PORT_STATUS_BASS = 14,
    PORT_STATUS_ADD9 = 15,
    PORT_STATUS_SUS = 16,
    PORT_STATUS_INVERSION = 17,
    PORT_STATUS_SPREAD = 18,
    PORT_STATUS_ACCENT = 19,
    PORT_STATUS_VOICE_LEAD = 20,
    PORT_STATUS_CLOCK_SOURCE = 21,
    PORT_STATUS_ACTIVE_CHORDS = 22
} PortIndex;

typedef struct {
    LV2_URID midi_Event;
    LV2_URID atom_Sequence;
    LV2_URID atom_Object;
    LV2_URID atom_Blank;
    LV2_URID atom_Float;
    LV2_URID atom_Double;
    LV2_URID atom_Int;
    LV2_URID atom_Long;
    LV2_URID atom_Chunk;
    LV2_URID time_Position;
    LV2_URID time_bar;
    LV2_URID time_barBeat;
    LV2_URID time_beatsPerBar;
    LV2_URID time_beatsPerMinute;
    LV2_URID time_speed;
    LV2_URID state_config;
} AchordUrids;

typedef struct {
    const LV2_Atom_Sequence *control_in;
    LV2_Atom_Sequence *midi_out;
    LV2_Atom_Sequence *launchpad_out;
    LV2_Atom_Sequence *notify_out;
    float *audio_out_l;
    float *audio_out_r;
    float *play_state_out;
    float *current_step_out;
    float *status_bank_out;
    float *status_octave_out;
    float *status_scale_out;
    float *status_register_out;
    float *status_trigger_out;
    float *status_hold_out;
    float *status_bass_out;
    float *status_add9_out;
    float *status_sus_out;
    float *status_inversion_out;
    float *status_spread_out;
    float *status_accent_out;
    float *status_voice_lead_out;
    float *status_clock_source_out;
    float *status_active_chords_out;

    LV2_URID_Map *map;
    AchordUrids urids;
    LV2_Atom_Forge forge;
    LV2_Atom_Forge lp_forge;
    LV2_Atom_Forge notify_forge;

    AchordEngine engine;
    uint8_t launchpad_initialized;
    uint8_t launchpad_refresh_retries;
    uint8_t launchpad_state_valid;
    LaunchpadState last_launchpad_state;
} Achord;

typedef struct {
    bool valid;
    bool playing;
    double bar;
    double barBeat;
    double beatsPerBar;
    double bpm;
} TimeInfo;

static void emit_midi(LV2_Atom_Forge *forge,
                      LV2_URID midi_urid,
                      uint32_t frame,
                      const uint8_t *data,
                      uint32_t size) {
    lv2_atom_forge_frame_time(forge, frame);
    lv2_atom_forge_atom(forge, size, midi_urid);
    lv2_atom_forge_write(forge, data, size);
}

static bool atom_to_double(const LV2_Atom *atom, const AchordUrids *urids, double *out_value) {
    if (!atom || !out_value) return false;
    if (atom->type == urids->atom_Double) {
        *out_value = ((const LV2_Atom_Double *)atom)->body;
        return true;
    }
    if (atom->type == urids->atom_Float) {
        *out_value = ((const LV2_Atom_Float *)atom)->body;
        return true;
    }
    if (atom->type == urids->atom_Long) {
        *out_value = ((const LV2_Atom_Long *)atom)->body;
        return true;
    }
    if (atom->type == urids->atom_Int) {
        *out_value = ((const LV2_Atom_Int *)atom)->body;
        return true;
    }
    return false;
}

static void read_time_info(const LV2_Atom_Sequence *control_in,
                           const AchordUrids *urids,
                           TimeInfo *info) {
    info->valid = false;
    info->playing = false;
    info->bar = 0.0;
    info->barBeat = 0.0;
    info->beatsPerBar = 4.0;
    info->bpm = 120.0;

    if (!control_in) return;

    LV2_ATOM_SEQUENCE_FOREACH(control_in, ev) {
        const LV2_Atom_Object *obj = NULL;

        if (ev->body.type == urids->time_Position) {
            obj = (const LV2_Atom_Object *)&ev->body;
        } else if (ev->body.type == urids->atom_Object || ev->body.type == urids->atom_Blank) {
            const LV2_Atom_Object *cand = (const LV2_Atom_Object *)&ev->body;
            if (cand->body.otype == urids->time_Position) {
                obj = cand;
            }
        }

        if (!obj) {
            continue;
        }

        const LV2_Atom *bar_atom = NULL;
        const LV2_Atom *bar_beat_atom = NULL;
        const LV2_Atom *beats_per_bar_atom = NULL;
        const LV2_Atom *bpm_atom = NULL;
        const LV2_Atom *speed_atom = NULL;

        lv2_atom_object_get(obj,
                            urids->time_bar, &bar_atom,
                            urids->time_barBeat, &bar_beat_atom,
                            urids->time_beatsPerBar, &beats_per_bar_atom,
                            urids->time_beatsPerMinute, &bpm_atom,
                            urids->time_speed, &speed_atom,
                            0);

        double value = 0.0;
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
        if (atom_to_double(speed_atom, urids, &value)) {
            info->playing = value > 0.0;
        }

        info->valid = true;
    }
}

static void emit_ui_state(Achord *self) {
    if (!self->notify_out || self->notify_out->atom.size < sizeof(LV2_Atom_Sequence_Body)) {
        return;
    }

    AchordUiState state;
    memset(&state, 0, sizeof(state));
    state.magic = ACHORD_UI_STATE_MAGIC;
    state.version = ACHORD_UI_STATE_VERSION;

    for (uint8_t r = 0; r < GRID_HEIGHT; ++r) {
        for (uint8_t c = 0; c < GRID_WIDTH; ++c) {
            state.grid[r][c] = self->engine.grid_state.grid[r][c].color;
        }
    }
    for (uint8_t i = 0; i < GRID_HEIGHT; ++i) {
        state.side[i] = self->engine.grid_state.side[i].color;
    }
    for (uint8_t i = 0; i < 9; ++i) {
        state.top[i] = self->engine.grid_state.top[i].color;
    }

    state.tonic_note = self->engine.config.tonic_note;
    state.bank_offset = self->engine.config.bank_offset;
    state.scale_index = self->engine.config.scale_index;
    state.register_mode = self->engine.config.register_mode;
    state.trigger_mode = self->engine.config.trigger_mode;
    state.hold_mode = self->engine.config.hold_mode;
    state.bass_enabled = self->engine.config.bass_enabled;
    state.add9_enabled = self->engine.config.add9_enabled;
    state.sus_mode = self->engine.config.sus_mode;
    state.inversion_offset = self->engine.config.inversion_offset;
    state.spread_mode = self->engine.config.spread_mode;
    state.accent_enabled = self->engine.config.accent_enabled;
    state.voice_lead_enabled = self->engine.config.voice_lead_enabled;
    state.bpm = self->engine.bpm;
    state.active_chord_count = self->engine.active_chord_count;
    state.current_step16 = self->engine.current_step16;
    state.host_playing = self->engine.host_playing ? 1 : 0;
    state.clock_source = self->engine.clock_source;

    lv2_atom_forge_frame_time(&self->notify_forge, 0);
    lv2_atom_forge_atom(&self->notify_forge, sizeof(state), self->urids.atom_Chunk);
    lv2_atom_forge_write(&self->notify_forge, &state, sizeof(state));
}

static void emit_launchpad_programmer_mode(Achord *self) {
    static const uint8_t sysex[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, 0x01, 0xF7};
    emit_midi(&self->lp_forge, self->urids.midi_Event, 0, sysex, sizeof(sysex));
    emit_midi(&self->forge, self->urids.midi_Event, 0, sysex, sizeof(sysex));
}

static void emit_launchpad_clear(Achord *self) {
    MidiMessage clear_msg;
    midi_build_clear_all(&clear_msg);
    emit_midi(&self->lp_forge, self->urids.midi_Event, 0, clear_msg.data, clear_msg.size);
    emit_midi(&self->forge, self->urids.midi_Event, 0, clear_msg.data, clear_msg.size);
}

static void emit_launchpad_full(Achord *self) {
    MidiMessage bulk;
    midi_update_grid(&bulk, &self->engine.grid_state);
    emit_midi(&self->lp_forge, self->urids.midi_Event, 0, bulk.data, bulk.size);
    emit_midi(&self->forge, self->urids.midi_Event, 0, bulk.data, bulk.size);
    self->last_launchpad_state = self->engine.grid_state;
    self->launchpad_state_valid = 1;
}

static LV2_State_Status achord_state_save(LV2_Handle instance,
                                          LV2_State_Store_Function store,
                                          LV2_State_Handle handle,
                                          uint32_t,
                                          const LV2_Feature *const *) {
    Achord *self = (Achord *)instance;
    if (!self || !store) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    const uint32_t flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;
    store(handle, self->urids.state_config,
          &self->engine.config, sizeof(self->engine.config),
          self->urids.atom_Chunk, flags);
    return LV2_STATE_SUCCESS;
}

static LV2_State_Status achord_state_restore(LV2_Handle instance,
                                             LV2_State_Retrieve_Function retrieve,
                                             LV2_State_Handle handle,
                                             uint32_t,
                                             const LV2_Feature *const *) {
    Achord *self = (Achord *)instance;
    if (!self || !retrieve) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    size_t size = 0;
    uint32_t type = 0;
    uint32_t flags = 0;
    const void *config = retrieve(handle, self->urids.state_config, &size, &type, &flags);
    if (config && size == sizeof(self->engine.config) && type == self->urids.atom_Chunk) {
        memcpy(&self->engine.config, config, sizeof(self->engine.config));
        achord_reset(&self->engine);
    }
    return LV2_STATE_SUCCESS;
}

static const LV2_State_Interface achord_state_interface = {
    achord_state_save,
    achord_state_restore
};

static LV2_Handle instantiate(const LV2_Descriptor *,
                              double rate,
                              const char *,
                              const LV2_Feature *const *features) {
    Achord *self = (Achord *)calloc(1, sizeof(Achord));
    if (!self) {
        return NULL;
    }

    for (int i = 0; features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_URID__map)) {
            self->map = (LV2_URID_Map *)features[i]->data;
        }
    }
    if (!self->map) {
        free(self);
        return NULL;
    }

    self->urids.midi_Event = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);
    self->urids.atom_Sequence = self->map->map(self->map->handle, LV2_ATOM__Sequence);
    self->urids.atom_Object = self->map->map(self->map->handle, LV2_ATOM__Object);
    self->urids.atom_Blank = self->map->map(self->map->handle, LV2_ATOM__Blank);
    self->urids.atom_Float = self->map->map(self->map->handle, LV2_ATOM__Float);
    self->urids.atom_Double = self->map->map(self->map->handle, LV2_ATOM__Double);
    self->urids.atom_Int = self->map->map(self->map->handle, LV2_ATOM__Int);
    self->urids.atom_Long = self->map->map(self->map->handle, LV2_ATOM__Long);
    self->urids.atom_Chunk = self->map->map(self->map->handle, LV2_ATOM__Chunk);
    self->urids.time_Position = self->map->map(self->map->handle, LV2_TIME__Position);
    self->urids.time_bar = self->map->map(self->map->handle, LV2_TIME__bar);
    self->urids.time_barBeat = self->map->map(self->map->handle, LV2_TIME__barBeat);
    self->urids.time_beatsPerBar = self->map->map(self->map->handle, LV2_TIME__beatsPerBar);
    self->urids.time_beatsPerMinute = self->map->map(self->map->handle, LV2_TIME__beatsPerMinute);
    self->urids.time_speed = self->map->map(self->map->handle, LV2_TIME__speed);
    self->urids.state_config = self->map->map(self->map->handle, ACHORD_URI "#config");

    lv2_atom_forge_init(&self->forge, self->map);
    lv2_atom_forge_init(&self->lp_forge, self->map);
    lv2_atom_forge_init(&self->notify_forge, self->map);

    achord_init(&self->engine, (float)rate);
    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void *data) {
    Achord *self = (Achord *)instance;
    switch ((PortIndex)port) {
        case PORT_CONTROL_IN: self->control_in = (const LV2_Atom_Sequence *)data; break;
        case PORT_MIDI_OUT: self->midi_out = (LV2_Atom_Sequence *)data; break;
        case PORT_LAUNCHPAD_OUT: self->launchpad_out = (LV2_Atom_Sequence *)data; break;
        case PORT_NOTIFY_OUT: self->notify_out = (LV2_Atom_Sequence *)data; break;
        case PORT_AUDIO_OUT_L: self->audio_out_l = (float *)data; break;
        case PORT_AUDIO_OUT_R: self->audio_out_r = (float *)data; break;
        case PORT_PLAY_STATE: self->play_state_out = (float *)data; break;
        case PORT_CURRENT_STEP: self->current_step_out = (float *)data; break;
        case PORT_STATUS_BANK: self->status_bank_out = (float *)data; break;
        case PORT_STATUS_OCTAVE: self->status_octave_out = (float *)data; break;
        case PORT_STATUS_SCALE: self->status_scale_out = (float *)data; break;
        case PORT_STATUS_REGISTER: self->status_register_out = (float *)data; break;
        case PORT_STATUS_TRIGGER: self->status_trigger_out = (float *)data; break;
        case PORT_STATUS_HOLD: self->status_hold_out = (float *)data; break;
        case PORT_STATUS_BASS: self->status_bass_out = (float *)data; break;
        case PORT_STATUS_ADD9: self->status_add9_out = (float *)data; break;
        case PORT_STATUS_SUS: self->status_sus_out = (float *)data; break;
        case PORT_STATUS_INVERSION: self->status_inversion_out = (float *)data; break;
        case PORT_STATUS_SPREAD: self->status_spread_out = (float *)data; break;
        case PORT_STATUS_ACCENT: self->status_accent_out = (float *)data; break;
        case PORT_STATUS_VOICE_LEAD: self->status_voice_lead_out = (float *)data; break;
        case PORT_STATUS_CLOCK_SOURCE: self->status_clock_source_out = (float *)data; break;
        case PORT_STATUS_ACTIVE_CHORDS: self->status_active_chords_out = (float *)data; break;
    }
}

static void activate(LV2_Handle instance) {
    Achord *self = (Achord *)instance;
    achord_reset(&self->engine);
    self->launchpad_initialized = 0;
    self->launchpad_refresh_retries = 12;
    self->launchpad_state_valid = 0;
    memset(&self->last_launchpad_state, 0, sizeof(self->last_launchpad_state));
}

static void run(LV2_Handle instance, uint32_t n_samples) {
    Achord *self = (Achord *)instance;
    if (!self || !self->midi_out || !self->launchpad_out) {
        return;
    }

    TimeInfo info;
    read_time_info(self->control_in, &self->urids, &info);

    achord_begin_block(&self->engine,
                       n_samples,
                       info.valid ? 1 : 0,
                       info.playing ? 1 : 0,
                       info.bar,
                       info.barBeat,
                       info.beatsPerBar,
                       info.bpm);

    bool saw_launchpad_input = false;

    if (self->control_in) {
        LV2_ATOM_SEQUENCE_FOREACH(self->control_in, ev) {
            if (ev->body.type != self->urids.midi_Event || ev->body.size < 3) {
                continue;
            }

            const uint8_t *m = (const uint8_t *)(ev + 1);
            const uint8_t msg = (uint8_t)(m[0] & 0xF0);
            const uint32_t frame = ev->time.frames;

            if (msg == 0x90 && m[2] > 0) {
                uint8_t row = 0;
                uint8_t col = 0;
                if (note_to_grid(m[1], &row, &col)) {
                    saw_launchpad_input = true;
                    achord_handle_pad_press(&self->engine, row, col, m[2], frame);
                }
            } else if (msg == 0x80 || (msg == 0x90 && m[2] == 0)) {
                uint8_t row = 0;
                uint8_t col = 0;
                if (note_to_grid(m[1], &row, &col)) {
                    saw_launchpad_input = true;
                    achord_handle_pad_release(&self->engine, row, col, frame);
                }
            } else if (msg == 0xB0 && m[2] > 0) {
                uint8_t index = 0;
                if (is_side_button(m[1], &index)) {
                    saw_launchpad_input = true;
                    achord_handle_side_button(&self->engine, index, frame);
                } else if (is_top_button(m[1], &index)) {
                    saw_launchpad_input = true;
                    achord_handle_top_button(&self->engine, index, frame);
                }
            }
        }
    }

    lv2_atom_forge_set_buffer(&self->forge, (uint8_t *)self->midi_out, self->midi_out->atom.size);
    lv2_atom_forge_set_buffer(&self->lp_forge, (uint8_t *)self->launchpad_out, self->launchpad_out->atom.size);
    if (self->notify_out) {
        lv2_atom_forge_set_buffer(&self->notify_forge, (uint8_t *)self->notify_out, self->notify_out->atom.size);
    }

    LV2_Atom_Forge_Frame midi_frame;
    LV2_Atom_Forge_Frame lp_frame;
    lv2_atom_forge_sequence_head(&self->forge, &midi_frame, 0);
    lv2_atom_forge_sequence_head(&self->lp_forge, &lp_frame, 0);

    LV2_Atom_Forge_Frame notify_frame;
    if (self->notify_out) {
        lv2_atom_forge_sequence_head(&self->notify_forge, &notify_frame, 0);
    }

    if (saw_launchpad_input && self->launchpad_refresh_retries < 4) {
        self->launchpad_refresh_retries = 4;
    }

    achord_process(&self->engine, n_samples);
    achord_refresh_grid_state(&self->engine);

    for (uint16_t i = 0; i < self->engine.midi_event_count; ++i) {
        const MidiOutEvent *ev = &self->engine.midi_events[i];
        emit_midi(&self->forge, self->urids.midi_Event, ev->frame, ev->data, ev->size);
    }

    if (!self->launchpad_initialized) {
        emit_launchpad_programmer_mode(self);
        emit_launchpad_clear(self);
        self->launchpad_initialized = 1;
    }

    if (self->launchpad_refresh_retries > 0) {
        emit_launchpad_programmer_mode(self);
        self->launchpad_refresh_retries--;
    }

    emit_launchpad_full(self);
    emit_ui_state(self);

    lv2_atom_forge_pop(&self->forge, &midi_frame);
    lv2_atom_forge_pop(&self->lp_forge, &lp_frame);
    if (self->notify_out) {
        lv2_atom_forge_pop(&self->notify_forge, &notify_frame);
    }

    if (self->play_state_out) *self->play_state_out = self->engine.host_playing ? 1.0f : 0.0f;
    if (self->current_step_out) *self->current_step_out = (float)(self->engine.current_step16 & 0x0F);
    if (self->status_bank_out) *self->status_bank_out = (float)self->engine.config.bank_offset;
    if (self->status_octave_out) *self->status_octave_out = (float)((int)self->engine.config.tonic_note / 12 - 1);
    if (self->status_scale_out) *self->status_scale_out = (float)self->engine.config.scale_index;
    if (self->status_register_out) *self->status_register_out = (float)self->engine.config.register_mode;
    if (self->status_trigger_out) *self->status_trigger_out = (float)self->engine.config.trigger_mode;
    if (self->status_hold_out) *self->status_hold_out = (float)self->engine.config.hold_mode;
    if (self->status_bass_out) *self->status_bass_out = self->engine.config.bass_enabled ? 1.0f : 0.0f;
    if (self->status_add9_out) *self->status_add9_out = self->engine.config.add9_enabled ? 1.0f : 0.0f;
    if (self->status_sus_out) *self->status_sus_out = (float)self->engine.config.sus_mode;
    if (self->status_inversion_out) *self->status_inversion_out = (float)self->engine.config.inversion_offset;
    if (self->status_spread_out) *self->status_spread_out = (float)self->engine.config.spread_mode;
    if (self->status_accent_out) *self->status_accent_out = self->engine.config.accent_enabled ? 1.0f : 0.0f;
    if (self->status_voice_lead_out) *self->status_voice_lead_out = self->engine.config.voice_lead_enabled ? 1.0f : 0.0f;
    if (self->status_clock_source_out) *self->status_clock_source_out = (float)self->engine.clock_source;
    if (self->status_active_chords_out) *self->status_active_chords_out = (float)self->engine.active_chord_count;

    if (self->audio_out_l) memset(self->audio_out_l, 0, n_samples * sizeof(float));
    if (self->audio_out_r) memset(self->audio_out_r, 0, n_samples * sizeof(float));
}

static void deactivate(LV2_Handle) {}

static void cleanup(LV2_Handle instance) {
    free(instance);
}

static const void *extension_data(const char *uri) {
    if (!strcmp(uri, LV2_STATE__interface)) {
        return &achord_state_interface;
    }
    return NULL;
}

static const LV2_Descriptor descriptor = {
    ACHORD_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor *lv2_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : NULL;
}
