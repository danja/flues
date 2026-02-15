#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "../include/arpiso_engine.h"
#include "../include/arpiso_ui_state.h"
#include "../include/launchpad_config.h"
#include "../include/midi_comm.h"
}

#define ARPISO_URI "https://danja.github.io/flues/plugins/arpiso"

typedef enum {
    PORT_CONTROL_IN = 0,
    PORT_MIDI_OUT = 1,
    PORT_LAUNCHPAD_OUT = 2,
    PORT_AUDIO_OUT_L = 3,
    PORT_AUDIO_OUT_R = 4,
    PORT_PLAY_STATE = 5,
    PORT_CURRENT_STEP = 6,
    PORT_NOTIFY_OUT = 7
} PortIndex;

typedef struct {
    LV2_URID midi_Event;
    LV2_URID atom_Sequence;
    LV2_URID atom_Object;
    LV2_URID atom_Blank;
    LV2_URID atom_Float;
    LV2_URID atom_Chunk;
    LV2_URID time_Position;
    LV2_URID time_beatsPerMinute;
    LV2_URID time_speed;
} ArpIsoUrids;

typedef struct {
    const LV2_Atom_Sequence *control_in;
    LV2_Atom_Sequence *midi_out;
    LV2_Atom_Sequence *launchpad_out;
    LV2_Atom_Sequence *notify_out;
    float *audio_out_l;
    float *audio_out_r;
    float *play_state_out;
    float *current_step_out;

    LV2_URID_Map *map;
    ArpIsoUrids urids;

    LV2_Atom_Forge forge;
    LV2_Atom_Forge lp_forge;
    LV2_Atom_Forge notify_forge;

    ArpIsoEngine engine;
    uint8_t launchpad_initialized;
    uint8_t pad_down[GRID_HEIGHT][GRID_WIDTH];
} ArpIso;

static void emit_midi(LV2_Atom_Forge *forge,
                      LV2_URID midi_urid,
                      uint32_t frame,
                      const uint8_t *data,
                      uint32_t size) {
    lv2_atom_forge_frame_time(forge, frame);
    lv2_atom_forge_atom(forge, size, midi_urid);
    lv2_atom_forge_write(forge, data, size);
}

static void emit_ui_state(ArpIso *self) {
    if (!self->notify_out || self->notify_out->atom.size < sizeof(LV2_Atom_Sequence_Body)) {
        return;
    }

    ArpIsoUiState state;
    memset(&state, 0, sizeof(state));
    state.magic = ARPISO_UI_STATE_MAGIC;
    state.version = ARPISO_UI_STATE_VERSION;

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

    state.selected_voice = self->engine.held_count;
    state.pattern = self->engine.pattern_slot;
    state.playing = self->engine.playing;
    state.bpm = self->engine.grid_state.tempo_bpm;
    state.current_step = (uint8_t)(self->engine.current_step & 0x3F);
    if (self->engine.wells[0].active) {
        state.euclid_pulses = self->engine.wells[0].pulses;
        state.euclid_offset = self->engine.wells[0].offset;
    }

    lv2_atom_forge_frame_time(&self->notify_forge, 0);
    lv2_atom_forge_atom(&self->notify_forge, sizeof(state), self->urids.atom_Chunk);
    lv2_atom_forge_write(&self->notify_forge, &state, sizeof(state));
}

static void emit_launchpad_bulk(ArpIso *self) {
    MidiMessage bulk;
    midi_update_grid(&bulk, &self->engine.grid_state);
    emit_midi(&self->lp_forge, self->urids.midi_Event, 0, bulk.data, bulk.size);
    emit_midi(&self->forge, self->urids.midi_Event, 0, bulk.data, bulk.size);
}

static LV2_Handle instantiate(const LV2_Descriptor *,
                              double rate,
                              const char *,
                              const LV2_Feature *const *features) {
    ArpIso *self = (ArpIso *)calloc(1, sizeof(ArpIso));
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
    self->urids.atom_Chunk = self->map->map(self->map->handle, LV2_ATOM__Chunk);
    self->urids.time_Position = self->map->map(self->map->handle, LV2_TIME__Position);
    self->urids.time_beatsPerMinute = self->map->map(self->map->handle, LV2_TIME__beatsPerMinute);
    self->urids.time_speed = self->map->map(self->map->handle, LV2_TIME__speed);

    lv2_atom_forge_init(&self->forge, self->map);
    lv2_atom_forge_init(&self->lp_forge, self->map);
    lv2_atom_forge_init(&self->notify_forge, self->map);

    arpiso_init(&self->engine, (float)rate);
    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void *data) {
    ArpIso *self = (ArpIso *)instance;
    switch ((PortIndex)port) {
        case PORT_CONTROL_IN: self->control_in = (const LV2_Atom_Sequence *)data; break;
        case PORT_MIDI_OUT: self->midi_out = (LV2_Atom_Sequence *)data; break;
        case PORT_LAUNCHPAD_OUT: self->launchpad_out = (LV2_Atom_Sequence *)data; break;
        case PORT_NOTIFY_OUT: self->notify_out = (LV2_Atom_Sequence *)data; break;
        case PORT_AUDIO_OUT_L: self->audio_out_l = (float *)data; break;
        case PORT_AUDIO_OUT_R: self->audio_out_r = (float *)data; break;
        case PORT_PLAY_STATE: self->play_state_out = (float *)data; break;
        case PORT_CURRENT_STEP: self->current_step_out = (float *)data; break;
    }
}

static void activate(LV2_Handle instance) {
    ArpIso *self = (ArpIso *)instance;
    arpiso_reset(&self->engine);
    memset(self->pad_down, 0, sizeof(self->pad_down));
    self->launchpad_initialized = 0;
}

static void run(LV2_Handle instance, uint32_t n_samples) {
    ArpIso *self = (ArpIso *)instance;
    arpiso_begin_block(&self->engine);

    LV2_ATOM_SEQUENCE_FOREACH(self->control_in, ev) {
        const LV2_Atom_Object *obj = NULL;

        if (ev->body.type == self->urids.time_Position) {
            obj = (const LV2_Atom_Object *)&ev->body;
        } else if (ev->body.type == self->urids.atom_Object || ev->body.type == self->urids.atom_Blank) {
            const LV2_Atom_Object *cand = (const LV2_Atom_Object *)&ev->body;
            if (cand->body.otype == self->urids.time_Position) {
                obj = cand;
            }
        }

        if (obj) {
            LV2_Atom *bpm = NULL;
            lv2_atom_object_get(obj, self->urids.time_beatsPerMinute, &bpm, 0);
            if (bpm && bpm->type == self->urids.atom_Float) {
                arpiso_set_tempo(&self->engine, (uint16_t)((LV2_Atom_Float *)bpm)->body);
            }

            LV2_Atom *speed = NULL;
            lv2_atom_object_get(obj, self->urids.time_speed, &speed, 0);
            if (speed && speed->type == self->urids.atom_Float) {
                float s = ((LV2_Atom_Float *)speed)->body;
                if (s > 0.0f && !self->engine.playing) arpiso_start(&self->engine);
                if (s == 0.0f && self->engine.playing) arpiso_stop(&self->engine);
            }
            continue;
        }

        if (ev->body.type != self->urids.midi_Event || ev->body.size < 2) {
            continue;
        }

        const uint8_t *m = (const uint8_t *)(ev + 1);
        const uint8_t msg = (uint8_t)(m[0] & 0xF0);

        if (msg == 0x90 && ev->body.size >= 3 && m[2] > 0) {
            uint8_t row = 0;
            uint8_t col = 0;
            uint8_t idx = 0;
            if (note_to_grid(m[1], &row, &col)) {
                self->pad_down[row][col] = 1;
                arpiso_handle_pad_press(&self->engine, row, col, m[2]);
            } else if (is_top_button(m[1], &idx)) {
                arpiso_handle_top_button(&self->engine, idx);
            }
        } else if ((msg == 0x80) || (msg == 0x90 && ev->body.size >= 3 && m[2] == 0)) {
            uint8_t row = 0;
            uint8_t col = 0;
            if (note_to_grid(m[1], &row, &col)) {
                self->pad_down[row][col] = 0;
                arpiso_handle_pad_release(&self->engine, row, col);
            }
        } else if (msg == 0xB0 && ev->body.size >= 3 && m[2] > 0) {
            uint8_t idx = 0;
            if (is_side_button(m[1], &idx)) {
                arpiso_handle_side_button(&self->engine, idx);
            } else if (is_top_button(m[1], &idx)) {
                arpiso_handle_top_button(&self->engine, idx);
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

    if (!self->launchpad_initialized) {
        static const uint8_t sysex[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, 0x01, 0xF7};
        emit_midi(&self->lp_forge, self->urids.midi_Event, 0, sysex, sizeof(sysex));
        emit_midi(&self->forge, self->urids.midi_Event, 0, sysex, sizeof(sysex));

        MidiMessage clear_msg;
        midi_build_clear_all(&clear_msg);
        emit_midi(&self->lp_forge, self->urids.midi_Event, 0, clear_msg.data, clear_msg.size);
        emit_midi(&self->forge, self->urids.midi_Event, 0, clear_msg.data, clear_msg.size);

        self->launchpad_initialized = 1;
    }

    arpiso_process(&self->engine, n_samples);
    arpiso_refresh_grid_state(&self->engine);

    for (uint16_t i = 0; i < self->engine.midi_event_count; ++i) {
        const MidiOutEvent *ev = &self->engine.midi_events[i];
        emit_midi(&self->forge, self->urids.midi_Event, ev->frame, ev->data, ev->size);
    }

    emit_launchpad_bulk(self);
    emit_ui_state(self);

    lv2_atom_forge_pop(&self->forge, &midi_frame);
    lv2_atom_forge_pop(&self->lp_forge, &lp_frame);
    if (self->notify_out) {
        lv2_atom_forge_pop(&self->notify_forge, &notify_frame);
    }

    if (self->play_state_out) *self->play_state_out = self->engine.playing ? 1.0f : 0.0f;
    if (self->current_step_out) *self->current_step_out = (float)(self->engine.current_step & 0x3F);

    if (self->audio_out_l) memset(self->audio_out_l, 0, n_samples * sizeof(float));
    if (self->audio_out_r) memset(self->audio_out_r, 0, n_samples * sizeof(float));
}

static void deactivate(LV2_Handle) {}

static void cleanup(LV2_Handle instance) {
    free(instance);
}

static const void *extension_data(const char *) {
    return NULL;
}

static const LV2_Descriptor descriptor = {
    ARPISO_URI,
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
