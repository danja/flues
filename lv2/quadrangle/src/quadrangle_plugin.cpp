#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/atom/forge.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>
#include <lv2/time/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern "C" {
#include "../include/quadrangle_engine.h"
#include "../include/midi_comm.h"
#include "../include/launchpad_config.h"
}

#define QUADRANGLE_URI "https://danja.github.io/flues/plugins/quadrangle"

// LV2 Port Indices
typedef enum {
    PORT_CONTROL_IN  = 0,  // Atom input (MIDI from Launchpad + transport)
    PORT_MIDI_OUT    = 1,  // Atom output (Musical MIDI notes)
    PORT_LAUNCHPAD_OUT = 2,  // Atom output (LED commands to Launchpad)
    PORT_AUDIO_OUT_L = 3,  // Audio output (left)
    PORT_AUDIO_OUT_R = 4   // Audio output (right)
} PortIndex;

// URIDs we need
typedef struct {
    LV2_URID midi_Event;
    LV2_URID atom_Sequence;
    LV2_URID atom_Blank;
    LV2_URID atom_Object;
    LV2_URID atom_Float;
    LV2_URID time_Position;
    LV2_URID time_beatsPerMinute;
    LV2_URID time_speed;
} QuadrangleURIDs;

// Plugin Instance
typedef struct {
    // Ports
    const LV2_Atom_Sequence *control_in;
    LV2_Atom_Sequence *midi_out;
    LV2_Atom_Sequence *launchpad_out;
    float *audio_out_l;
    float *audio_out_r;

    // URIDs
    QuadrangleURIDs urids;
    LV2_URID_Map *map;

    // Atom forges
    LV2_Atom_Forge forge;           // For MIDI notes
    LV2_Atom_Forge launchpad_forge; // For Launchpad LED commands

    // Engine
    QuadrangleEngine engine;

    // Launchpad state
    uint8_t launchpad_initialized;
    uint32_t frame_counter;

} Quadrangle;

// ============================================================================
// LV2 Callbacks
// ============================================================================

static LV2_Handle instantiate(const LV2_Descriptor *descriptor,
                               double rate,
                               const char *path,
                               const LV2_Feature *const *features) {
    (void)descriptor;
    (void)path;

    Quadrangle *self = (Quadrangle *)calloc(1, sizeof(Quadrangle));
    if (!self) return NULL;

    // Get URID map feature
    for (int i = 0; features[i]; i++) {
        if (!strcmp(features[i]->URI, LV2_URID__map)) {
            self->map = (LV2_URID_Map *)features[i]->data;
        }
    }

    if (!self->map) {
        free(self);
        return NULL;
    }

    // Map URIDs
    self->urids.midi_Event = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);
    self->urids.atom_Sequence = self->map->map(self->map->handle, LV2_ATOM__Sequence);
    self->urids.atom_Blank = self->map->map(self->map->handle, LV2_ATOM__Blank);
    self->urids.atom_Object = self->map->map(self->map->handle, LV2_ATOM__Object);
    self->urids.atom_Float = self->map->map(self->map->handle, LV2_ATOM__Float);
    self->urids.time_Position = self->map->map(self->map->handle, LV2_TIME__Position);
    self->urids.time_beatsPerMinute = self->map->map(self->map->handle, LV2_TIME__beatsPerMinute);
    self->urids.time_speed = self->map->map(self->map->handle, LV2_TIME__speed);

    // Initialize atom forges
    lv2_atom_forge_init(&self->forge, self->map);
    lv2_atom_forge_init(&self->launchpad_forge, self->map);

    // Initialize engine
    quadrangle_init(&self->engine, (float)rate);

    self->launchpad_initialized = 0;
    self->frame_counter = 0;

    return (LV2_Handle)self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void *data) {
    Quadrangle *self = (Quadrangle *)instance;

    switch (port) {
        case PORT_CONTROL_IN:
            self->control_in = (const LV2_Atom_Sequence *)data;
            break;
        case PORT_MIDI_OUT:
            self->midi_out = (LV2_Atom_Sequence *)data;
            break;
        case PORT_LAUNCHPAD_OUT:
            self->launchpad_out = (LV2_Atom_Sequence *)data;
            break;
        case PORT_AUDIO_OUT_L:
            self->audio_out_l = (float *)data;
            break;
        case PORT_AUDIO_OUT_R:
            self->audio_out_r = (float *)data;
            break;
    }
}

static void activate(LV2_Handle instance) {
    Quadrangle *self = (Quadrangle *)instance;
    quadrangle_reset(&self->engine);
    self->launchpad_initialized = 0;
}

static void run(LV2_Handle instance, uint32_t n_samples) {
    Quadrangle *self = (Quadrangle *)instance;

    static int run_count = 0;
    if (run_count == 0) {
        fprintf(stderr, "quadrangle: First run() call - plugin is active\n");
        fprintf(stderr, "quadrangle: control_in pointer: %p\n", (void*)self->control_in);
    }
    run_count++;

    // Check for MIDI events (skip transport spam)
    static int midi_event_count = 0;
    LV2_ATOM_SEQUENCE_FOREACH(self->control_in, ev) {
        if (ev->body.type == self->urids.midi_Event) {
            const uint8_t *midi = (const uint8_t*)(ev + 1);
            fprintf(stderr, "quadrangle: MIDI [%02X %02X %02X]\n",
                    midi[0], midi[1], midi[2]);
            midi_event_count++;
        }
    }

    // Show message if no MIDI received after 1000 checks
    static int check_count = 0;
    if (midi_event_count == 0 && check_count == 1000) {
        fprintf(stderr, "quadrangle: WARNING - No MIDI events received yet. Check MIDI routing!\n");
    }
    check_count++;

    // Process incoming events
    LV2_ATOM_SEQUENCE_FOREACH(self->control_in, ev) {
        // Process transport position first
        if (ev->body.type == self->urids.atom_Object ||
            ev->body.type == self->urids.atom_Blank) {
            const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;

            if (obj->body.otype == self->urids.time_Position) {
                // Extract BPM
                LV2_Atom *bpm = NULL;
                lv2_atom_object_get(obj, self->urids.time_beatsPerMinute, &bpm, 0);
                if (bpm && bpm->type == self->urids.atom_Float) {
                    float new_bpm = ((LV2_Atom_Float*)bpm)->body;
                    quadrangle_set_tempo(&self->engine, (uint8_t)new_bpm);
                }

                // Extract transport speed (0.0 = stopped, 1.0 = playing)
                LV2_Atom *speed = NULL;
                lv2_atom_object_get(obj, self->urids.time_speed, &speed, 0);
                if (speed && speed->type == self->urids.atom_Float) {
                    float transport_speed = ((LV2_Atom_Float*)speed)->body;
                    if (transport_speed > 0.0f && !self->engine.playing) {
                        quadrangle_start(&self->engine);
                    } else if (transport_speed == 0.0f && self->engine.playing) {
                        quadrangle_stop(&self->engine);
                    }
                }
            }
        }
        // Process MIDI events from Launchpad
        else if (ev->body.type == self->urids.midi_Event) {
            const uint8_t *midi = (const uint8_t*)(ev + 1);  // Grid-seq pattern

            // Note On (0x90)
            if ((midi[0] & 0xF0) == 0x90 && midi[2] > 0) {
                uint8_t note = midi[1];
                fprintf(stderr, "quadrangle: Received Note On - note=%d\n", note);

                // Check if it's a grid pad
                uint8_t row, col;
                if (note_to_grid(note, &row, &col)) {
                    fprintf(stderr, "quadrangle: Pad press at row=%u, col=%u, vel=%u\n",
                            row, col, midi[2]);
                    quadrangle_handle_pad_press(&self->engine, row, col, midi[2]);
                }
            }
            // Note Off (0x80) or Note On with velocity 0
            else if ((midi[0] & 0xF0) == 0x80 || ((midi[0] & 0xF0) == 0x90 && midi[2] == 0)) {
                uint8_t note = midi[1];
                uint8_t row, col;
                if (note_to_grid(note, &row, &col)) {
                    fprintf(stderr, "quadrangle: Pad release at row=%u, col=%u\n", row, col);
                    quadrangle_handle_pad_release(&self->engine, row, col);
                }
            }
            // Control Change (0xB0) - side and top buttons
            else if ((midi[0] & 0xF0) == 0xB0) {
                uint8_t cc = midi[1];
                uint8_t value = midi[2];

                fprintf(stderr, "quadrangle: Received CC %u = %u\n", cc, value);

                uint8_t index;
                if (is_side_button(cc, &index)) {
                    fprintf(stderr, "quadrangle: Side button %u\n", index);
                    quadrangle_handle_side_button(&self->engine, index);
                } else if (is_top_button(cc, &index)) {
                    fprintf(stderr, "quadrangle: Top button %u\n", index);
                    quadrangle_handle_top_button(&self->engine, index);
                }
            }
        }
    }

    // Setup forges AFTER processing input (grid-seq pattern)
    const uint32_t midi_capacity = self->midi_out->atom.size;
    lv2_atom_forge_set_buffer(&self->forge,
                               (uint8_t*)self->midi_out,
                               midi_capacity);

    const uint32_t launchpad_capacity = self->launchpad_out->atom.size;
    lv2_atom_forge_set_buffer(&self->launchpad_forge,
                               (uint8_t*)self->launchpad_out,
                               launchpad_capacity);

    // Start MIDI note sequence
    LV2_Atom_Forge_Frame midi_frame;
    lv2_atom_forge_sequence_head(&self->forge, &midi_frame, 0);

    // Start Launchpad control sequence
    LV2_Atom_Forge_Frame launchpad_frame;
    lv2_atom_forge_sequence_head(&self->launchpad_forge, &launchpad_frame, 0);

    // Initialize Launchpad on first run
    if (!self->launchpad_initialized) {
        fprintf(stderr, "quadrangle: Initializing Launchpad...\n");

        // Send Programmer Mode SysEx
        uint8_t sysex[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, 0x01, 0xF7};

        fprintf(stderr, "quadrangle: Sending Programmer Mode SysEx: ");
        for (size_t i = 0; i < sizeof(sysex); i++) {
            fprintf(stderr, "%02X ", sysex[i]);
        }
        fprintf(stderr, "\n");

        // IMPORTANT: Send to BOTH outputs like grid-seq
        lv2_atom_forge_frame_time(&self->forge, 0);
        lv2_atom_forge_atom(&self->forge, sizeof(sysex), self->urids.midi_Event);
        lv2_atom_forge_write(&self->forge, sysex, sizeof(sysex));

        lv2_atom_forge_frame_time(&self->launchpad_forge, 0);
        lv2_atom_forge_atom(&self->launchpad_forge, sizeof(sysex), self->urids.midi_Event);
        lv2_atom_forge_write(&self->launchpad_forge, sysex, sizeof(sysex));

        self->launchpad_initialized = 1;
        fprintf(stderr, "quadrangle: Sent Programmer Mode to BOTH outputs\n");
    }

    // Update LEDs - send individual Note On messages like grid-seq
    self->frame_counter += n_samples;
    if (self->frame_counter >= 64) {
        self->frame_counter = 0;

        static int led_count = 0;
        if (led_count < 3) {
            fprintf(stderr, "quadrangle: Sending LED update #%d\n", led_count);
            led_count++;
        }

        // Send LED update for each pad
        for (uint8_t row = 0; row < 8; row++) {
            for (uint8_t col = 0; col < 8; col++) {
                uint8_t note = grid_to_note(row, col);
                uint8_t color = self->engine.grid_state.grid[row][col].color;

                // Send as Note On (0x90) like grid-seq does
                uint8_t msg[3] = {0x90, note, color};
                lv2_atom_forge_frame_time(&self->launchpad_forge, 0);
                lv2_atom_forge_atom(&self->launchpad_forge, 3, self->urids.midi_Event);
                lv2_atom_forge_write(&self->launchpad_forge, msg, 3);
            }
        }

        // Update side buttons
        for (uint8_t i = 0; i < 8; i++) {
            uint8_t color = self->engine.grid_state.side[i].color;
            uint8_t cc_num = SIDE_BUTTONS[i];

            // Send as CC (0xB0)
            uint8_t msg[3] = {0xB0, cc_num, color};
            lv2_atom_forge_frame_time(&self->launchpad_forge, 0);
            lv2_atom_forge_atom(&self->launchpad_forge, 3, self->urids.midi_Event);
            lv2_atom_forge_write(&self->launchpad_forge, msg, 3);
        }

        // Update top buttons
        for (uint8_t i = 0; i < 9; i++) {
            uint8_t color = self->engine.grid_state.top[i].color;
            uint8_t cc_num = TOP_BUTTONS[i];

            // Send as CC (0xB0)
            uint8_t msg[3] = {0xB0, cc_num, color};
            lv2_atom_forge_frame_time(&self->launchpad_forge, 0);
            lv2_atom_forge_atom(&self->launchpad_forge, 3, self->urids.midi_Event);
            lv2_atom_forge_write(&self->launchpad_forge, msg, 3);
        }
    }

    // Process audio (sequencer clock)
    quadrangle_process(&self->engine, n_samples);

    // Clear audio output (no audio generation yet)
    memset(self->audio_out_l, 0, n_samples * sizeof(float));
    memset(self->audio_out_r, 0, n_samples * sizeof(float));
}

static void deactivate(LV2_Handle instance) {
    Quadrangle *self = (Quadrangle *)instance;

    // Exit programmer mode
    MidiMessage msg;
    midi_build_set_mode(&msg, MODE_LIVE);

    // Clear all LEDs
    midi_build_clear_all(&msg);
}

static void cleanup(LV2_Handle instance) {
    free(instance);
}

static const void *extension_data(const char *uri) {
    (void)uri;
    return NULL;
}

// ============================================================================
// LV2 Descriptor
// ============================================================================

static const LV2_Descriptor descriptor = {
    QUADRANGLE_URI,
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
