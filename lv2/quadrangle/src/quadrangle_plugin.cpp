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
#include "../include/quadrangle_ui_state.h"
}

#define QUADRANGLE_URI "https://danja.github.io/flues/plugins/quadrangle"

// LV2 Port Indices
typedef enum {
    PORT_CONTROL_IN  = 0,  // Atom input (MIDI from Launchpad + transport)
    PORT_MIDI_OUT    = 1,  // Atom output (Musical MIDI notes)
    PORT_LAUNCHPAD_OUT = 2,  // Atom output (LED commands to Launchpad)
    PORT_AUDIO_OUT_L = 3,  // Audio output (left)
    PORT_AUDIO_OUT_R = 4,  // Audio output (right)
    PORT_PLAY_STATE  = 5,  // Control output to UI (0=stopped, 1=playing)
    PORT_CURRENT_STEP = 6, // Control output: current playhead step (0-15)
    PORT_NOTIFY_OUT = 7    // Atom output to UI (state notify)
} PortIndex;

// URIDs we need
typedef struct {
    LV2_URID midi_Event;
    LV2_URID atom_Sequence;
    LV2_URID atom_Blank;
    LV2_URID atom_Object;
    LV2_URID atom_Float;
    LV2_URID atom_Chunk;
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
    LV2_Atom_Sequence *notify_out;
    float *audio_out_l;
    float *audio_out_r;
    float *play_state_out;
    float *current_step_out;

    // URIDs
    QuadrangleURIDs urids;
    LV2_URID_Map *map;

    // Atom forges
    LV2_Atom_Forge forge;           // For MIDI notes
    LV2_Atom_Forge launchpad_forge; // For Launchpad LED commands
    LV2_Atom_Forge notify_forge;    // For UI notifications

    // Engine
    QuadrangleEngine engine;

    // Launchpad state
    uint8_t launchpad_initialized;
    uint32_t frame_counter;

} Quadrangle;

// ============================================================================
// Helpers
// ============================================================================

// Write a MIDI/SysEx message to Launchpad control output.
// If launchpad_out is not connected (size too small/null), fall back to midi_out
// so the Launchpad still lights when routed via the main MIDI port.
static inline void send_lp_msg(Quadrangle* self,
                               const uint8_t* data,
                               uint16_t size) {
    const bool lp_connected = self->launchpad_out &&
        self->launchpad_out->atom.size >= sizeof(LV2_Atom_Sequence_Body);

    if (lp_connected) {
        lv2_atom_forge_frame_time(&self->launchpad_forge, 0);
        lv2_atom_forge_atom(&self->launchpad_forge, size, self->urids.midi_Event);
        lv2_atom_forge_write(&self->launchpad_forge, data, size);
    } else {
        // Fallback: send via midi_out (may reach Launchpad if routed; instruments should ignore SysEx)
        lv2_atom_forge_frame_time(&self->forge, 0);
        lv2_atom_forge_atom(&self->forge, size, self->urids.midi_Event);
        lv2_atom_forge_write(&self->forge, data, size);
    }
}

// Seed a visible default palette so hardware shows life immediately
static void set_default_led_layout(Quadrangle* self) {
    // Leave grid pads off initially; light only helpers so the device isn't a solid wall
    grid_state_clear(&self->engine.grid_state);

    // Side buttons select drum voice 0 by default
    for (uint8_t i = 0; i < MAX_DRUM_VOICES && i < GRID_HEIGHT; ++i) {
        uint8_t color = (i == 0) ? COLOR_SELECTED : COLOR_DRUMS;
        grid_state_set_side_button(&self->engine.grid_state, i, 0, color);
    }

    // Top buttons: play/stop off, patterns A-D dim, clear dim red
    for (uint8_t i = 0; i < 9; ++i) {
        uint8_t color = COLOR_OFF;
        if (i >= 2 && i <= 5) {
            color = COLOR_YELLOW_DIM;  // pattern buttons A-D
        } else if (i == 7) {
            color = COLOR_RED_DIM;     // clear
        }
        grid_state_set_top_button(&self->engine.grid_state, i, 0, color);
    }
}

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
    self->urids.atom_Chunk = self->map->map(self->map->handle, LV2_ATOM__Chunk);
    self->urids.time_Position = self->map->map(self->map->handle, LV2_TIME__Position);
    self->urids.time_beatsPerMinute = self->map->map(self->map->handle, LV2_TIME__beatsPerMinute);
    self->urids.time_speed = self->map->map(self->map->handle, LV2_TIME__speed);

    // Initialize atom forges
    lv2_atom_forge_init(&self->forge, self->map);
    lv2_atom_forge_init(&self->launchpad_forge, self->map);
    lv2_atom_forge_init(&self->notify_forge, self->map);

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
        case PORT_NOTIFY_OUT:
            self->notify_out = (LV2_Atom_Sequence *)data;
            break;
        case PORT_AUDIO_OUT_L:
            self->audio_out_l = (float *)data;
            break;
        case PORT_AUDIO_OUT_R:
            self->audio_out_r = (float *)data;
            break;
        case PORT_PLAY_STATE:
            self->play_state_out = (float *)data;
            break;
        case PORT_CURRENT_STEP:
            self->current_step_out = (float *)data;
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

    // Start a fresh MIDI queue for this block before handling input
    quadrangle_begin_block(&self->engine);

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
        // Process transport position first (mirror Euclid: handle direct Position or Object/Blank wrapper)
        const LV2_Atom_Object* obj = NULL;
        if (ev->body.type == self->urids.time_Position) {
            obj = (const LV2_Atom_Object*)&ev->body;
        } else if (ev->body.type == self->urids.atom_Object ||
                   ev->body.type == self->urids.atom_Blank) {
            const LV2_Atom_Object* cand = (const LV2_Atom_Object*)&ev->body;
            if (cand->body.otype == self->urids.time_Position) {
                obj = cand;
            }
        }

        if (obj) {
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
            continue;
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
                    if (value > 0) {
                        fprintf(stderr, "quadrangle: Side button %u\n", index);
                        quadrangle_handle_side_button(&self->engine, index);
                    }
                } else if (is_top_button(cc, &index)) {
                    if (value > 0) {
                        fprintf(stderr, "quadrangle: Top button %u\n", index);
                        quadrangle_handle_top_button(&self->engine, index);
                    }
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
    const uint32_t notify_capacity = self->notify_out ? self->notify_out->atom.size : 0;
    if (self->notify_out && notify_capacity > 0) {
        lv2_atom_forge_set_buffer(&self->notify_forge,
                                   (uint8_t*)self->notify_out,
                                   notify_capacity);
    }

    // Start MIDI note sequence
    LV2_Atom_Forge_Frame midi_frame;
    lv2_atom_forge_sequence_head(&self->forge, &midi_frame, 0);

    // Start Launchpad control sequence
    LV2_Atom_Forge_Frame launchpad_frame;
    lv2_atom_forge_sequence_head(&self->launchpad_forge, &launchpad_frame, 0);
    LV2_Atom_Forge_Frame notify_frame;
    if (self->notify_out && notify_capacity > 0) {
        lv2_atom_forge_sequence_head(&self->notify_forge, &notify_frame, 0);
    }

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

        // Primary: Launchpad control output
        lv2_atom_forge_frame_time(&self->launchpad_forge, 0);
        lv2_atom_forge_atom(&self->launchpad_forge, sizeof(sysex), self->urids.midi_Event);
        lv2_atom_forge_write(&self->launchpad_forge, sysex, sizeof(sysex));
        // Fallback: also emit SysEx on midi_out in case host routes Launchpad there (instruments will ignore SysEx)
        lv2_atom_forge_frame_time(&self->forge, 0);
        lv2_atom_forge_atom(&self->forge, sizeof(sysex), self->urids.midi_Event);
        lv2_atom_forge_write(&self->forge, sysex, sizeof(sysex));

        self->launchpad_initialized = 1;
        fprintf(stderr, "quadrangle: Sent Programmer Mode to Launchpad output (and midi_out fallback)\n");

        // Force-clear LEDs after entering programmer mode to avoid stale states
        MidiMessage clear_msg;
        midi_build_clear_all(&clear_msg);
        send_lp_msg(self, clear_msg.data, clear_msg.size);

        // Seed a visible LED layout immediately so hardware shows activity
        set_default_led_layout(self);
    }

    // Run sequencer clock (adds step-triggered MIDI to the queue)
    quadrangle_process(&self->engine, n_samples);

    // Flush queued MIDI events to midi_out
    for (uint16_t i = 0; i < self->engine.midi_event_count; ++i) {
        const MidiOutEvent* ev = &self->engine.midi_events[i];
        lv2_atom_forge_frame_time(&self->forge, ev->frame);
        lv2_atom_forge_atom(&self->forge, ev->size, self->urids.midi_Event);
        lv2_atom_forge_write(&self->forge, ev->data, ev->size);
    }

    // Update LEDs AFTER processing so playhead reflects the new step
    self->frame_counter += n_samples;
    if (self->frame_counter >= 64) {
        self->frame_counter = 0;

        // Refresh grid state colors before emitting LED messages
        quadrangle_refresh_grid_state(&self->engine);

        // Bulk SysEx update for safety (sent both to Launchpad out and midi_out; instruments ignore SysEx)
        MidiMessage bulk;
        midi_update_grid(&bulk, &self->engine.grid_state);
        // Launchpad out
        lv2_atom_forge_frame_time(&self->launchpad_forge, 0);
        lv2_atom_forge_atom(&self->launchpad_forge, bulk.size, self->urids.midi_Event);
        lv2_atom_forge_write(&self->launchpad_forge, bulk.data, bulk.size);
        // midi_out fallback
        lv2_atom_forge_frame_time(&self->forge, 0);
        lv2_atom_forge_atom(&self->forge, bulk.size, self->urids.midi_Event);
        lv2_atom_forge_write(&self->forge, bulk.data, bulk.size);

        if (self->notify_out && notify_capacity > 0) {
            QuadrangleUiState state;
            memset(&state, 0, sizeof(state));
            state.magic = QUADRANGLE_UI_STATE_MAGIC;
            state.version = QUADRANGLE_UI_STATE_VERSION;
            for (uint8_t r = 0; r < 8; ++r) {
                for (uint8_t c = 0; c < 8; ++c) {
                    state.grid[r][c] = self->engine.grid_state.grid[r][c].color;
                }
            }
            for (uint8_t i = 0; i < 8; ++i) {
                state.side[i] = self->engine.grid_state.side[i].color;
            }
            for (uint8_t i = 0; i < 9; ++i) {
                state.top[i] = self->engine.grid_state.top[i].color;
            }
            state.selected_voice = self->engine.selected_voice;
            state.pattern = self->engine.grid_state.pattern;
            state.playing = self->engine.playing ? 1 : 0;
            state.current_step = (uint8_t)(self->engine.current_step & 0xFF);

            lv2_atom_forge_frame_time(&self->notify_forge, 0);
            lv2_atom_forge_atom(&self->notify_forge, sizeof(state), self->urids.atom_Chunk);
            lv2_atom_forge_write(&self->notify_forge, &state, sizeof(state));
        }
    }

    // Finalize forged sequences so hosts see valid atom sizes
    lv2_atom_forge_pop(&self->forge, &midi_frame);
    lv2_atom_forge_pop(&self->launchpad_forge, &launchpad_frame);
    if (self->notify_out && notify_capacity > 0) {
        lv2_atom_forge_pop(&self->notify_forge, &notify_frame);
    }

    // Publish play state to UI (simple control port)
    if (self->play_state_out) {
        *self->play_state_out = self->engine.playing ? 1.0f : 0.0f;
    }
    if (self->current_step_out) {
        *self->current_step_out = (float)(self->engine.current_step & 0xFF);
    }

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
