/*
 * grid-seq - Grid-based MIDI sequencer LV2 plugin
 *
 * Copyright (C) 2025 Danny
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE.
 */

#include "grid_seq/common.h"
#include "state.h"
#include "sequencer.h"
#include "launchpad.h"

#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>
#include <lv2/midi/midi.h>
#include <lv2/time/time.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Launchpad SysEx lighting (bulk LED update)
#define LP_LIGHTING_SYSEX_HEADER_SIZE 6
static const uint8_t LP_LIGHTING_SYSEX_HEADER[LP_LIGHTING_SYSEX_HEADER_SIZE] = {
    0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D
};
#define LP_SYSEX_CMD_LED_LIGHTING 0x03
#define LP_SYSEX_END 0xF7
#define LP_LED_TYPE_STATIC 0x00
#define LP_SYSEX_MAX 512

static const uint8_t LP_GRID_NOTES[8][8] = {
    {11, 12, 13, 14, 15, 16, 17, 18},
    {21, 22, 23, 24, 25, 26, 27, 28},
    {31, 32, 33, 34, 35, 36, 37, 38},
    {41, 42, 43, 44, 45, 46, 47, 48},
    {51, 52, 53, 54, 55, 56, 57, 58},
    {61, 62, 63, 64, 65, 66, 67, 68},
    {71, 72, 73, 74, 75, 76, 77, 78},
    {81, 82, 83, 84, 85, 86, 87, 88}
};

static const uint8_t LP_SIDE_BUTTONS[8] = {19, 29, 39, 49, 59, 69, 79, 89};
static const uint8_t LP_TOP_BUTTONS[9] = {91, 92, 93, 94, 95, 96, 97, 98, 99};

static uint8_t lp_grid_note(uint8_t row, uint8_t col) {
    if (row >= 8 || col >= 8) {
        return 0;
    }
    return LP_GRID_NOTES[row][col];
}


typedef enum {
    PORT_MIDI_IN = 0,
    PORT_MIDI_OUT = 1,
    PORT_LAUNCHPAD_OUT = 2,
    PORT_GRID_X = 3,
    PORT_GRID_Y = 4,
    PORT_CURRENT_STEP = 5,
    PORT_GRID_CHANGED = 6,
    PORT_NOTIFY = 7,
    PORT_GRID_ROW_0 = 8,
    PORT_GRID_ROW_1 = 9,
    PORT_GRID_ROW_2 = 10,
    PORT_GRID_ROW_3 = 11,
    PORT_GRID_ROW_4 = 12,
    PORT_GRID_ROW_5 = 13,
    PORT_GRID_ROW_6 = 14,
    PORT_GRID_ROW_7 = 15,
    PORT_GRID_ROW_8 = 16,
    PORT_GRID_ROW_9 = 17,
    PORT_GRID_ROW_10 = 18,
    PORT_GRID_ROW_11 = 19,
    PORT_GRID_ROW_12 = 20,
    PORT_GRID_ROW_13 = 21,
    PORT_GRID_ROW_14 = 22,
    PORT_GRID_ROW_15 = 23,
    PORT_SEQUENCE_LENGTH = 24,
    PORT_MIDI_FILTER = 25,
    PORT_BEATS_PER_BAR = 26,
    PORT_MIDI_CHANNEL = 27
} PortIndex;

typedef struct {
    // Ports
    const LV2_Atom_Sequence* midi_in;
    LV2_Atom_Sequence* midi_out;
    LV2_Atom_Sequence* launchpad_out;
    LV2_Atom_Sequence* notify;
    const float* grid_x;
    const float* grid_y;
    float* current_step;
    float* grid_changed;
    float* grid_row[MAX_GRID_SIZE];
    const float* sequence_length;
    const float* midi_filter;
    float* beats_per_bar_port;
    const float* midi_channel;

    // Features
    LV2_URID_Map* map;

    // URIDs
    LV2_URID midi_MidiEvent;
    LV2_URID atom_Blank;
    LV2_URID atom_Object;
    LV2_URID atom_Int;
    LV2_URID time_Position;
    LV2_URID time_beatsPerMinute;
    LV2_URID time_speed;
    LV2_URID gridState;
    LV2_URID cellX;
    LV2_URID cellY;
    LV2_URID cellValue;

    // State
    GridSeqState state;
    SequencerURIDs seq_uris;

    // Atom forge
    LV2_Atom_Forge forge;

    // Previous grid control values
    float prev_grid_x;
    float prev_grid_y;

    // Launchpad state
    bool launchpad_mode_entered;
    bool launchpad_reset_pending;
    uint8_t prev_led_step;
    bool grid_dirty;

    // Separate forge for Launchpad
    LV2_Atom_Forge launchpad_forge;

    // Separate forge for UI notifications
    LV2_Atom_Forge notify_forge;

    // Grid change counter
    uint32_t grid_change_counter;

    // Track last toggled cell for UI notification
    int8_t last_toggled_x;
    int8_t last_toggled_y;
} GridSeq;

static LV2_Handle instantiate(
    const LV2_Descriptor* descriptor,
    double rate,
    const char* bundle_path,
    const LV2_Feature* const* features
) {
    (void)descriptor;
    (void)bundle_path;

    GridSeq* gs = (GridSeq*)calloc(1, sizeof(GridSeq));
    if (!gs) return NULL;

    // Get URID map feature
    for (int i = 0; features[i]; i++) {
        if (strcmp(features[i]->URI, LV2_URID__map) == 0) {
            gs->map = (LV2_URID_Map*)features[i]->data;
        }
    }

    if (!gs->map) {
        free(gs);
        return NULL;
    }

    // Map URIDs
    gs->midi_MidiEvent = gs->map->map(gs->map->handle, LV2_MIDI__MidiEvent);
    gs->atom_Blank = gs->map->map(gs->map->handle, LV2_ATOM__Blank);
    gs->atom_Object = gs->map->map(gs->map->handle, LV2_ATOM__Object);
    gs->atom_Int = gs->map->map(gs->map->handle, LV2_ATOM__Int);
    gs->time_Position = gs->map->map(gs->map->handle, LV2_TIME__Position);
    gs->time_beatsPerMinute = gs->map->map(gs->map->handle, LV2_TIME__beatsPerMinute);
    gs->time_speed = gs->map->map(gs->map->handle, LV2_TIME__speed);
    gs->gridState = gs->map->map(gs->map->handle, GRID_SEQ__gridState);
    gs->cellX = gs->map->map(gs->map->handle, GRID_SEQ__cellX);
    gs->cellY = gs->map->map(gs->map->handle, GRID_SEQ__cellY);
    gs->cellValue = gs->map->map(gs->map->handle, GRID_SEQ__cellValue);

    gs->seq_uris.midi_MidiEvent = gs->midi_MidiEvent;

    // Initialize state
    state_init(&gs->state, rate);

    // Initialize atom forges
    lv2_atom_forge_init(&gs->forge, gs->map);
    lv2_atom_forge_init(&gs->launchpad_forge, gs->map);
    lv2_atom_forge_init(&gs->notify_forge, gs->map);

    // Initialize grid state (empty - will be set by user or host state)
    // Grid is already zeroed by state_init() called above

    // Initialize previous control values
    gs->prev_grid_x = -1.0f;
    gs->prev_grid_y = -1.0f;

    // Initialize Launchpad state
    gs->launchpad_mode_entered = false;
    gs->launchpad_reset_pending = false;
    gs->prev_led_step = 0;
    gs->grid_dirty = true;

    // Initialize toggle tracking
    gs->last_toggled_x = -1;
    gs->last_toggled_y = -1;

    return (LV2_Handle)gs;
}

static void connect_port(
    LV2_Handle instance,
    uint32_t port,
    void* data
) {
    GridSeq* gs = (GridSeq*)instance;

    switch ((PortIndex)port) {
        case PORT_MIDI_IN:
            gs->midi_in = (const LV2_Atom_Sequence*)data;
            break;
        case PORT_MIDI_OUT:
            gs->midi_out = (LV2_Atom_Sequence*)data;
            break;
        case PORT_LAUNCHPAD_OUT:
            gs->launchpad_out = (LV2_Atom_Sequence*)data;
            break;
        case PORT_NOTIFY:
            gs->notify = (LV2_Atom_Sequence*)data;
            break;
        case PORT_GRID_X:
            gs->grid_x = (const float*)data;
            break;
        case PORT_GRID_Y:
            gs->grid_y = (const float*)data;
            break;
        case PORT_CURRENT_STEP:
            gs->current_step = (float*)data;
            break;
        case PORT_GRID_CHANGED:
            gs->grid_changed = (float*)data;
            break;
        case PORT_GRID_ROW_0:
        case PORT_GRID_ROW_1:
        case PORT_GRID_ROW_2:
        case PORT_GRID_ROW_3:
        case PORT_GRID_ROW_4:
        case PORT_GRID_ROW_5:
        case PORT_GRID_ROW_6:
        case PORT_GRID_ROW_7:
        case PORT_GRID_ROW_8:
        case PORT_GRID_ROW_9:
        case PORT_GRID_ROW_10:
        case PORT_GRID_ROW_11:
        case PORT_GRID_ROW_12:
        case PORT_GRID_ROW_13:
        case PORT_GRID_ROW_14:
        case PORT_GRID_ROW_15:
            gs->grid_row[port - PORT_GRID_ROW_0] = (float*)data;
            break;
        case PORT_SEQUENCE_LENGTH:
            gs->sequence_length = (const float*)data;
            break;
        case PORT_MIDI_FILTER:
            gs->midi_filter = (const float*)data;
            break;
        case PORT_BEATS_PER_BAR:
            gs->beats_per_bar_port = (float*)data;
            break;
        case PORT_MIDI_CHANNEL:
            gs->midi_channel = (const float*)data;
            break;
    }
}

static void update_grid_row_ports(GridSeq* gs) {
    // Pack current 8-note window (based on pitch_offset) into ports for UI display
    for (int x = 0; x < MAX_GRID_SIZE; x++) {
        if (gs->grid_row[x]) {
            uint8_t row_value = 0;
            for (int y = 0; y < GRID_VISIBLE_ROWS; y++) {
                uint8_t actual_note = gs->state.pitch_offset + (uint8_t)y;
                if (gs->state.grid[x][actual_note]) {
                    row_value |= (1 << y);
                }
            }
            *gs->grid_row[x] = (float)row_value;
        }
    }
}

static void read_grid_row_ports(GridSeq* gs) {
    // Read persisted port values into grid state
    fprintf(stderr, "grid-seq: Reading persisted grid state from ports:\n");
    for (int x = 0; x < MAX_GRID_SIZE; x++) {
        if (gs->grid_row[x]) {
            uint8_t row_value = (uint8_t)(*gs->grid_row[x]);
            if (row_value != 0) {
                fprintf(stderr, "  Column %d: value=%d (0x%02X)\n", x, row_value, row_value);
            }
            for (int y = 0; y < GRID_VISIBLE_ROWS; y++) {
                gs->state.grid[x][y] = (row_value & (1 << y)) != 0;
            }
        }
    }
}

static void send_sysex_programmer_mode(GridSeq* gs, LV2_Atom_Forge* forge, bool enter) {
    // SysEx: F0 00 20 29 02 0D 0E [01/00] F7
    uint8_t sysex[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, enter ? 0x01 : 0x00, 0xF7};

    fprintf(stderr, "grid-seq: Sending SysEx to %s Programmer Mode\n", enter ? "ENTER" : "EXIT");
    fprintf(stderr, "  SysEx bytes: ");
    for (size_t i = 0; i < sizeof(sysex); i++) {
        fprintf(stderr, "%02X ", sysex[i]);
    }
    fprintf(stderr, "\n");

    lv2_atom_forge_frame_time(forge, 0);
    lv2_atom_forge_atom(forge, sizeof(sysex), gs->midi_MidiEvent);
    lv2_atom_forge_write(forge, sysex, sizeof(sysex));
}

static size_t build_launchpad_clear_sysex(uint8_t* buffer, size_t max);
static size_t build_launchpad_led_sysex(GridSeq* gs, uint8_t* buffer, size_t max);
static void send_launchpad_sysex(GridSeq* gs, const uint8_t* data, uint16_t size);
static void update_launchpad_leds_note_cc(GridSeq* gs);

static void send_launchpad_full_init(GridSeq* gs) {
    if (!gs) {
        return;
    }

    // Enter Programmer Mode on both outputs (padseq-style init).
    send_sysex_programmer_mode(gs, &gs->forge, true);
    if (gs->launchpad_out) {
        send_sysex_programmer_mode(gs, &gs->launchpad_forge, true);
    }

    // Clear LEDs and push a full grid state update via SysEx.
    uint8_t clear_sysex[LP_SYSEX_MAX];
    size_t clear_size = build_launchpad_clear_sysex(clear_sysex, sizeof(clear_sysex));
    if (clear_size > 0) {
        send_launchpad_sysex(gs, clear_sysex, (uint16_t)clear_size);
    }

    uint8_t grid_sysex[LP_SYSEX_MAX];
    size_t grid_size = build_launchpad_led_sysex(gs, grid_sysex, sizeof(grid_sysex));
    if (grid_size > 0) {
        send_launchpad_sysex(gs, grid_sysex, (uint16_t)grid_size);
    }

    // CC updates help some hosts/devices latch state after SysEx.
    update_launchpad_leds_note_cc(gs);
}

static void append_sysex_byte(uint8_t* buffer, size_t* pos, size_t max, uint8_t byte) {
    if (*pos < max) {
        buffer[*pos] = byte;
        (*pos)++;
    }
}

static size_t build_launchpad_led_sysex(GridSeq* gs, uint8_t* buffer, size_t max) {
    size_t pos = 0;
    if (max < LP_LIGHTING_SYSEX_HEADER_SIZE + 2) {
        return 0;
    }

    for (size_t i = 0; i < LP_LIGHTING_SYSEX_HEADER_SIZE; i++) {
        append_sysex_byte(buffer, &pos, max, LP_LIGHTING_SYSEX_HEADER[i]);
    }
    append_sysex_byte(buffer, &pos, max, LP_SYSEX_CMD_LED_LIGHTING);

    // Calculate which steps to show based on current hardware page
    uint8_t page_offset = gs->state.hardware_page * 8;

    static int debug_count = 0;
    if (debug_count < 3) {
        fprintf(stderr, "grid-seq: LED update - pitch_offset=%d, hardware_page=%d\n",
                gs->state.pitch_offset, gs->state.hardware_page);
        debug_count++;
    }

    for (uint8_t x = 0; x < 8; x++) {
        for (uint8_t y = 0; y < 8; y++) {
            uint8_t note = lp_grid_note(y, x);
            uint8_t actual_step = page_offset + x;
            uint8_t actual_note = gs->state.pitch_offset + y;
            uint8_t color;

            // If this column is beyond sequence length, turn it off
            if (actual_step >= gs->state.sequence_length) {
                color = LP_COLOR_OFF;
            } else if (actual_step == gs->state.current_step) {
                color = gs->state.grid[actual_step][actual_note] ? LP_COLOR_RED : LP_COLOR_GREEN;
            } else {
                color = gs->state.grid[actual_step][actual_note] ? LP_COLOR_YELLOW : LP_COLOR_OFF;
            }

            append_sysex_byte(buffer, &pos, max, LP_LED_TYPE_STATIC);
            append_sysex_byte(buffer, &pos, max, note);
            append_sysex_byte(buffer, &pos, max, color);

            if (debug_count < 3 && color != LP_COLOR_OFF) {
                fprintf(stderr, "  LED[%d,%d] note=%d color=%d (grid[%d][%d]=%d)\n",
                        x, y, note, color, actual_step, actual_note, gs->state.grid[actual_step][actual_note]);
            }
        }
    }

    // Light up side/top buttons for scale + beats per bar + pitch range
    for (uint8_t i = 0; i < 8; i++) {
        append_sysex_byte(buffer, &pos, max, LP_LED_TYPE_STATIC);
        append_sysex_byte(buffer, &pos, max, LP_SIDE_BUTTONS[i]);
        uint8_t side_color = (i == (gs->state.scale_index % state_scale_count())) ? LP_COLOR_WHITE : LP_COLOR_OFF;
        append_sysex_byte(buffer, &pos, max, side_color);
    }

    uint8_t top_colors[9];
    for (uint8_t i = 0; i < 9; i++) {
        top_colors[i] = LP_COLOR_OFF;
    }
    top_colors[2] = (gs->state.hardware_page > 0) ? LP_COLOR_WHITE : LP_COLOR_OFF;
    top_colors[3] = (gs->state.sequence_length > 8 && gs->state.hardware_page == 0) ? LP_COLOR_WHITE : LP_COLOR_OFF;
    top_colors[4] = (gs->state.beats_per_bar > MIN_BEATS_PER_BAR) ? LP_COLOR_WHITE : LP_COLOR_OFF;
    top_colors[5] = (gs->state.beats_per_bar < MAX_BEATS_PER_BAR) ? LP_COLOR_WHITE : LP_COLOR_OFF;
    top_colors[0] = (gs->state.pitch_offset > 0) ? LP_COLOR_WHITE : LP_COLOR_OFF;
    top_colors[1] = (gs->state.pitch_offset < (GRID_PITCH_RANGE - GRID_VISIBLE_ROWS)) ? LP_COLOR_WHITE : LP_COLOR_OFF;

    for (uint8_t i = 0; i < 9; i++) {
        append_sysex_byte(buffer, &pos, max, LP_LED_TYPE_STATIC);
        append_sysex_byte(buffer, &pos, max, LP_TOP_BUTTONS[i]);
        append_sysex_byte(buffer, &pos, max, top_colors[i]);
    }

    append_sysex_byte(buffer, &pos, max, LP_SYSEX_END);
    return pos;
}

static size_t build_launchpad_clear_sysex(uint8_t* buffer, size_t max) {
    size_t pos = 0;
    if (max < LP_LIGHTING_SYSEX_HEADER_SIZE + 2) {
        return 0;
    }

    for (size_t i = 0; i < LP_LIGHTING_SYSEX_HEADER_SIZE; i++) {
        append_sysex_byte(buffer, &pos, max, LP_LIGHTING_SYSEX_HEADER[i]);
    }
    append_sysex_byte(buffer, &pos, max, LP_SYSEX_CMD_LED_LIGHTING);

    for (uint8_t y = 0; y < 8; y++) {
        for (uint8_t x = 0; x < 8; x++) {
            uint8_t note = lp_grid_note(y, x);
            append_sysex_byte(buffer, &pos, max, LP_LED_TYPE_STATIC);
            append_sysex_byte(buffer, &pos, max, note);
            append_sysex_byte(buffer, &pos, max, LP_COLOR_OFF);
        }
    }

    append_sysex_byte(buffer, &pos, max, LP_LED_TYPE_STATIC);
    append_sysex_byte(buffer, &pos, max, LP_TOP_BUTTONS[2]);
    append_sysex_byte(buffer, &pos, max, LP_COLOR_OFF);
    append_sysex_byte(buffer, &pos, max, LP_LED_TYPE_STATIC);
    append_sysex_byte(buffer, &pos, max, LP_TOP_BUTTONS[3]);
    append_sysex_byte(buffer, &pos, max, LP_COLOR_OFF);
    append_sysex_byte(buffer, &pos, max, LP_LED_TYPE_STATIC);
    append_sysex_byte(buffer, &pos, max, LP_TOP_BUTTONS[0]);
    append_sysex_byte(buffer, &pos, max, LP_COLOR_OFF);
    append_sysex_byte(buffer, &pos, max, LP_LED_TYPE_STATIC);
    append_sysex_byte(buffer, &pos, max, LP_TOP_BUTTONS[1]);
    append_sysex_byte(buffer, &pos, max, LP_COLOR_OFF);

    append_sysex_byte(buffer, &pos, max, LP_SYSEX_END);
    return pos;
}

static void send_launchpad_sysex(GridSeq* gs, const uint8_t* data, uint16_t size) {
    if (!gs || !data || size == 0) {
        return;
    }

    // Primary: Launchpad control output.
    if (gs->launchpad_out) {
        lv2_atom_forge_frame_time(&gs->launchpad_forge, 0);
        lv2_atom_forge_atom(&gs->launchpad_forge, size, gs->midi_MidiEvent);
        lv2_atom_forge_write(&gs->launchpad_forge, data, size);
    }

    // Fallback: SysEx only via midi_out (safe for instruments).
    if (gs->midi_out) {
        lv2_atom_forge_frame_time(&gs->forge, 0);
        lv2_atom_forge_atom(&gs->forge, size, gs->midi_MidiEvent);
        lv2_atom_forge_write(&gs->forge, data, size);
    }
}

static void send_launchpad_msg(GridSeq* gs, uint8_t status, uint8_t data1, uint8_t data2) {
    uint8_t msg[3] = {status, data1, data2};

    if (gs->launchpad_out) {
        lv2_atom_forge_frame_time(&gs->launchpad_forge, 0);
        lv2_atom_forge_atom(&gs->launchpad_forge, 3, gs->midi_MidiEvent);
        lv2_atom_forge_write(&gs->launchpad_forge, msg, 3);
    }
}

static void update_launchpad_leds_note_cc(GridSeq* gs) {
    uint8_t page_offset = gs->state.hardware_page * 8;

    for (uint8_t x = 0; x < 8; x++) {
        for (uint8_t y = 0; y < 8; y++) {
            uint8_t note = lp_grid_note(y, x);
            uint8_t actual_step = page_offset + x;
            uint8_t actual_note = gs->state.pitch_offset + y;
            uint8_t color;

            if (actual_step >= gs->state.sequence_length) {
                color = LP_COLOR_OFF;
            } else if (actual_step == gs->state.current_step) {
                color = gs->state.grid[actual_step][actual_note] ? LP_COLOR_RED : LP_COLOR_GREEN;
            } else {
                color = gs->state.grid[actual_step][actual_note] ? LP_COLOR_YELLOW : LP_COLOR_OFF;
            }

            send_launchpad_msg(gs, 0x90, note, color);
        }
    }

    uint8_t left_color = (gs->state.hardware_page > 0) ? LP_COLOR_WHITE : LP_COLOR_OFF;
    uint8_t right_color = (gs->state.sequence_length > 8 && gs->state.hardware_page == 0) ? LP_COLOR_WHITE : LP_COLOR_OFF;
    uint8_t down_color = (gs->state.pitch_offset > 0) ? LP_COLOR_WHITE : LP_COLOR_OFF;
    uint8_t up_color = (gs->state.pitch_offset < (GRID_PITCH_RANGE - GRID_VISIBLE_ROWS)) ? LP_COLOR_WHITE : LP_COLOR_OFF;
    uint8_t beats_down_color = (gs->state.beats_per_bar > MIN_BEATS_PER_BAR) ? LP_COLOR_WHITE : LP_COLOR_OFF;
    uint8_t beats_up_color = (gs->state.beats_per_bar < MAX_BEATS_PER_BAR) ? LP_COLOR_WHITE : LP_COLOR_OFF;

    for (uint8_t i = 0; i < 8; i++) {
        uint8_t side_color = (i == (gs->state.scale_index % state_scale_count())) ? LP_COLOR_WHITE : LP_COLOR_OFF;
        send_launchpad_msg(gs, 0xB0, LP_SIDE_BUTTONS[i], side_color);
    }

    send_launchpad_msg(gs, 0xB0, 93, left_color);
    send_launchpad_msg(gs, 0xB0, 94, right_color);
    send_launchpad_msg(gs, 0xB0, 91, down_color);
    send_launchpad_msg(gs, 0xB0, 92, up_color);
    send_launchpad_msg(gs, 0xB0, 95, beats_down_color);
    send_launchpad_msg(gs, 0xB0, 96, beats_up_color);
}

static void activate(LV2_Handle instance) {
    GridSeq* gs = (GridSeq*)instance;

    // Don't clear grid state on activate - preserve pattern across transport start/stop
    // (Grid state is only cleared explicitly by user via Clear button)

    // Read any persisted grid state from ports (e.g., from host project file)
    // COMMENTED OUT to test if persisted data is the problem
    // read_grid_row_ports(gs);

    // Reset launchpad mode flag so SysEx is sent again on next run()
    gs->launchpad_mode_entered = false;

    gs->state.playing = true;
    gs->state.frame_counter = 0;
    gs->state.current_step = 0;
    gs->state.previous_step = GRID_SIZE - 1;  // Set to last step so first step triggers
    gs->state.first_run = true;
    gs->grid_dirty = true;  // Force LED update on first run
}

static void run(LV2_Handle instance, uint32_t n_samples) {
    GridSeq* gs = (GridSeq*)instance;

    // Read sequence length from port and update state
    if (gs->sequence_length) {
        uint8_t new_length = (uint8_t)(*gs->sequence_length);
        if (new_length >= MIN_SEQUENCE_LENGTH && new_length <= MAX_SEQUENCE_LENGTH) {
            gs->state.sequence_length = new_length;
            state_update_tempo(&gs->state, gs->state.bpm);
        }
    }

    // Process incoming MIDI and Time position
    LV2_ATOM_SEQUENCE_FOREACH(gs->midi_in, ev) {
        // Check for time position (tempo/BPM)
        if (ev->body.type == gs->atom_Object || ev->body.type == gs->atom_Blank) {
            const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;

            if (obj->body.otype == gs->time_Position) {
                // Extract BPM
                const LV2_Atom* bpm_atom = NULL;
                const LV2_Atom* speed_atom = NULL;

                lv2_atom_object_get(obj,
                    gs->time_beatsPerMinute, &bpm_atom,
                    gs->time_speed, &speed_atom,
                    0);

                // Update BPM
                if (bpm_atom && bpm_atom->type == gs->map->map(gs->map->handle, LV2_ATOM__Float)) {
                    float bpm = ((const LV2_Atom_Float*)bpm_atom)->body;
                    if (bpm > 0) {
                        state_update_tempo(&gs->state, bpm);
                    }
                }

                // Update transport state (playing/stopped)
                if (speed_atom && speed_atom->type == gs->map->map(gs->map->handle, LV2_ATOM__Float)) {
                    float speed = ((const LV2_Atom_Float*)speed_atom)->body;
                    bool was_playing = gs->state.playing;
                    gs->state.playing = (speed > 0.0f);

                    if (!was_playing && gs->state.playing) {
                        // Started playing - reset frame counter
                        gs->state.frame_counter = 0;
                        gs->state.current_step = 0;
                    }
                }
            }
        }

        if (ev->body.type == gs->midi_MidiEvent) {
            const uint8_t* msg = (const uint8_t*)(ev + 1);

            // Note On (0x90)
            if ((msg[0] & 0xF0) == 0x90 && msg[2] > 0) {
                uint8_t note = msg[1];

                // Check if it's a grid button
                fprintf(stderr, "grid-seq: Received Note On - note=%d, checking if grid button\n", note);
                if (note >= 11 && note <= 88) {
                    uint8_t x, y;
                    lp_note_to_grid(note, &x, &y);

                    fprintf(stderr, "grid-seq: Launchpad pad pressed - MIDI note=%d -> grid x=%d y=%d\n",
                            note, x, y);

                    if (x < 8 && y < 8) {
                        // Calculate actual grid position based on hardware page and pitch offset
                        uint8_t actual_x = x + (gs->state.hardware_page * 8);
                        uint8_t actual_y = y + gs->state.pitch_offset;
                        if (actual_x < gs->state.sequence_length && actual_y < GRID_PITCH_RANGE) {
                            fprintf(stderr, "  -> Toggling grid[%d][%d], page=%d, pitch_offset=%d, new_value=%d\n",
                                    actual_x, actual_y, gs->state.hardware_page, gs->state.pitch_offset,
                                    !gs->state.grid[actual_x][actual_y]);
                            state_toggle_step(&gs->state, actual_x, actual_y);
                            gs->grid_dirty = true;
                            gs->grid_change_counter++;
                            gs->last_toggled_x = actual_x;
                            gs->last_toggled_y = actual_y;
                        }
                    }
                }
            }
            // Control Change (0xB0) - for Launchpad arrow buttons
            else if ((msg[0] & 0xF0) == 0xB0) {
                uint8_t cc = msg[1];
                uint8_t value = msg[2];

                // Debug: Log ALL CC messages to discover arrow button CCs
                static int cc_log_count = 0;
                if (cc_log_count < 50 || (cc >= 80 && cc <= 100)) {  // Log first 50 or arrows range
                    fprintf(stderr, "grid-seq: Launchpad CC received - CC=%d value=%d\n", cc, value);
                    cc_log_count++;
                }

                // Handle top-row controls (beats per bar, pitch shift) and side buttons (scale)
                if (value > 0) {
                    if (cc == 93) {  // Beats per bar down
                        if (gs->state.hardware_page > 0) {
                            gs->state.hardware_page--;
                            gs->grid_dirty = true;
                            fprintf(stderr, "grid-seq: Left arrow - switched to page 0 (steps 0-7)\n");
                        }
                    }
                    else if (cc == 94) {  // Right arrow (CC 94)
                        if (gs->state.sequence_length > 8 && gs->state.hardware_page == 0) {
                            gs->state.hardware_page = 1;
                            gs->grid_dirty = true;
                            fprintf(stderr, "grid-seq: Right arrow - switched to page 1 (steps 8-15)\n");
                        }
                    }
                    else if (cc == 95) {  // Beats per bar down
                        if (gs->state.beats_per_bar > MIN_BEATS_PER_BAR) {
                            gs->state.beats_per_bar--;
                            state_update_tempo(&gs->state, gs->state.bpm);
                            gs->grid_dirty = true;
                            fprintf(stderr, "grid-seq: Beats per bar down to %d\n", gs->state.beats_per_bar);
                        }
                    }
                    else if (cc == 96) {  // Beats per bar up
                        if (gs->state.beats_per_bar < MAX_BEATS_PER_BAR) {
                            gs->state.beats_per_bar++;
                            state_update_tempo(&gs->state, gs->state.bpm);
                            gs->grid_dirty = true;
                            fprintf(stderr, "grid-seq: Beats per bar up to %d\n", gs->state.beats_per_bar);
                        }
                    }
                    else if (cc == 91) {  // Shift pitch DOWN
                        if (gs->state.pitch_offset > 0) {
                            gs->state.pitch_offset--;
                            gs->grid_dirty = true;
                            fprintf(stderr, "grid-seq: Pitch shifted DOWN to %d (MIDI notes %d-%d)\n",
                                    gs->state.pitch_offset,
                                    gs->state.pitch_offset,
                                    gs->state.pitch_offset + GRID_VISIBLE_ROWS - 1);
                        }
                    }
                    else if (cc == 92) {  // Shift pitch UP
                        if (gs->state.pitch_offset < (GRID_PITCH_RANGE - GRID_VISIBLE_ROWS)) {
                            gs->state.pitch_offset++;
                            gs->grid_dirty = true;
                            fprintf(stderr, "grid-seq: Pitch shifted UP to %d (MIDI notes %d-%d)\n",
                                    gs->state.pitch_offset,
                                    gs->state.pitch_offset,
                                    gs->state.pitch_offset + GRID_VISIBLE_ROWS - 1);
                        }
                    }
                    else {
                        for (uint8_t i = 0; i < 8; i++) {
                            if (cc == LP_SIDE_BUTTONS[i]) {
                                gs->state.scale_index = i;
                                gs->grid_dirty = true;
                                fprintf(stderr, "grid-seq: Scale set to %s (index %u)\n",
                                        state_scale_name(gs->state.scale_index),
                                        gs->state.scale_index);
                                break;
                            }
                        }
                    }
                    // Top row buttons could be used for other functions if needed
                }
            }
        }
    }

    // Check for grid toggle from UI or hardware reset signal
    if (gs->grid_x && gs->grid_y) {
        float x = *gs->grid_x;
        float y = *gs->grid_y;

        // Check for device query signal (x == -200)
        if (x == -200.0f && x != gs->prev_grid_x) {
            fprintf(stderr, "\n=== DEVICE QUERY REQUESTED ===\n");

            // Send Device Inquiry SysEx: F0 7E 7F 06 01 F7
            uint8_t inquiry[] = {0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7};
            fprintf(stderr, "Sending Universal Device Inquiry: ");
            for (size_t i = 0; i < sizeof(inquiry); i++) {
                fprintf(stderr, "%02X ", inquiry[i]);
            }
            fprintf(stderr, "\n");

            lv2_atom_forge_frame_time(&gs->forge, 0);
            lv2_atom_forge_atom(&gs->forge, sizeof(inquiry), gs->midi_MidiEvent);
            lv2_atom_forge_write(&gs->forge, inquiry, sizeof(inquiry));

            lv2_atom_forge_frame_time(&gs->launchpad_forge, 0);
            lv2_atom_forge_atom(&gs->launchpad_forge, sizeof(inquiry), gs->midi_MidiEvent);
            lv2_atom_forge_write(&gs->launchpad_forge, inquiry, sizeof(inquiry));

            fprintf(stderr, "Sent to both outputs. Watch MIDI input for response.\n");
            fprintf(stderr, "Expected response starts with: F0 7E 00 06 02...\n");
            fprintf(stderr, "==============================\n\n");

            gs->prev_grid_x = x;
            return;
        }

        // Check for hardware reset signal (x == -100)
        if (x == -100.0f && x != gs->prev_grid_x) {
            fprintf(stderr, "\n=== HARDWARE RESET REQUESTED ===\n");
            fprintf(stderr, "Scheduling full Launchpad initialization SysEx...\n");
            gs->launchpad_reset_pending = true;
            gs->launchpad_mode_entered = false;
            gs->grid_dirty = true;

            fprintf(stderr, "Launchpad reset sequence initiated. Will re-init this cycle.\n");
            fprintf(stderr, "================================\n\n");

            gs->prev_grid_x = x;
        }

        // Check for clear pattern signal (x == -300)
        if (x == -300.0f && x != gs->prev_grid_x) {
            fprintf(stderr, "\n=== CLEAR PATTERN REQUESTED ===\n");

            // Clear all grid cells
            for (int i = 0; i < MAX_GRID_SIZE; i++) {
                for (int j = 0; j < GRID_PITCH_RANGE; j++) {
                    gs->state.grid[i][j] = false;
                }
            }

            // Force LED update
            gs->grid_dirty = true;
            gs->grid_change_counter++;

            fprintf(stderr, "Pattern cleared. All grid cells set to false.\n");
            fprintf(stderr, "================================\n\n");

            gs->prev_grid_x = x;
            return;
        }

        // Check for re-center signal (x == -400)
        if (x == -400.0f && x != gs->prev_grid_x) {
            fprintf(stderr, "\n=== RE-CENTER PITCH REQUESTED ===\n");

            // Reset pitch offset to default (C2 = MIDI 36)
            gs->state.pitch_offset = DEFAULT_PITCH_OFFSET;
            gs->grid_dirty = true;

            fprintf(stderr, "Pitch offset reset to %d (MIDI notes %d-%d)\n",
                    DEFAULT_PITCH_OFFSET,
                    DEFAULT_PITCH_OFFSET,
                    DEFAULT_PITCH_OFFSET + GRID_VISIBLE_ROWS - 1);
            fprintf(stderr, "================================\n\n");

            gs->prev_grid_x = x;
            return;
        }

        // If values changed and are valid, toggle the grid cell
        // UI sends window-relative coordinates (0-7), map through scale to MIDI note
        if ((x != gs->prev_grid_x || y != gs->prev_grid_y) &&
            x >= 0 && x < MAX_GRID_SIZE && y >= 0 && y < GRID_VISIBLE_ROWS) {
            uint8_t absolute_note = gs->state.pitch_offset + (uint8_t)y;
            if (absolute_note < GRID_PITCH_RANGE) {
                fprintf(stderr, "grid-seq: Plugin toggling cell [%d,%d] (window row %d -> MIDI note %d), new value: %d\n",
                        (int)x, absolute_note, (int)y, absolute_note,
                        !gs->state.grid[(int)x][absolute_note]);
                state_toggle_step(&gs->state, (uint8_t)x, absolute_note);

                gs->prev_grid_x = x;
                gs->prev_grid_y = y;
                gs->grid_dirty = true;
                gs->grid_change_counter++;
                fprintf(stderr, "grid-seq: Set grid_dirty=true after toggle\n");
                gs->last_toggled_x = (int8_t)x;
                gs->last_toggled_y = absolute_note;
            }
        }
    }

    // Setup forge for MIDI notes output
    const bool midi_out_connected = (gs->midi_out != NULL);
    if (midi_out_connected) {
        const uint32_t out_capacity = gs->midi_out->atom.size;
        lv2_atom_forge_set_buffer(&gs->forge,
                                  (uint8_t*)gs->midi_out,
                                  out_capacity);
    }

    // Setup forge for Launchpad control output
    const bool launchpad_connected = (gs->launchpad_out != NULL);
    if (launchpad_connected) {
        const uint32_t lp_capacity = gs->launchpad_out->atom.size;
        lv2_atom_forge_set_buffer(&gs->launchpad_forge,
                                  (uint8_t*)gs->launchpad_out,
                                  lp_capacity);
    }

    // Setup forge for UI notifications
    const bool notify_connected = (gs->notify != NULL);
    if (notify_connected) {
        const uint32_t notify_capacity = gs->notify->atom.size;
        lv2_atom_forge_set_buffer(&gs->notify_forge,
                                  (uint8_t*)gs->notify,
                                  notify_capacity);
    }


    // Update output ports BEFORE processing
    if (gs->current_step) {
        *gs->current_step = (float)gs->state.current_step;
    }
    if (gs->grid_changed) {
        *gs->grid_changed = (float)(gs->grid_change_counter % 1000000);
    }
    if (gs->beats_per_bar_port) {
        *gs->beats_per_bar_port = (float)gs->state.beats_per_bar;
    }

    // Update grid row ports with current state
    update_grid_row_ports(gs);

    // Start MIDI note sequence
    LV2_Atom_Forge_Frame frame;
    if (midi_out_connected) {
        lv2_atom_forge_sequence_head(&gs->forge, &frame, 0);
    }

    // Start Launchpad control sequence
    LV2_Atom_Forge_Frame lp_frame;
    if (launchpad_connected) {
        lv2_atom_forge_sequence_head(&gs->launchpad_forge, &lp_frame, 0);
    }

    // Start UI notification sequence (use frame time like MIDI and Launchpad)
    LV2_Atom_Forge_Frame notify_frame;
    if (notify_connected) {
        lv2_atom_forge_sequence_head(&gs->notify_forge, &notify_frame, 0);
    }

    if (gs->launchpad_reset_pending) {
        fprintf(stderr, "grid-seq: Sending Launchpad full init SysEx now\n");
        send_launchpad_full_init(gs);
        gs->launchpad_reset_pending = false;
        gs->launchpad_mode_entered = true;
        gs->grid_dirty = true;
    }

    // Enter Programmer Mode on first run
    // IMPORTANT: Send to BOTH midi_out and launchpad_out to ensure it reaches the device
    if (!gs->launchpad_mode_entered) {
        if (midi_out_connected) {
            send_sysex_programmer_mode(gs, &gs->forge, true);
        }
        if (launchpad_connected) {
            send_sysex_programmer_mode(gs, &gs->launchpad_forge, true);
        }
        uint8_t clear_sysex[LP_SYSEX_MAX];
        size_t clear_size = build_launchpad_clear_sysex(clear_sysex, sizeof(clear_sysex));
        if (clear_size > 0) {
            send_launchpad_sysex(gs, clear_sysex, (uint16_t)clear_size);
        }
        gs->launchpad_mode_entered = true;
        gs->grid_dirty = true;
        fprintf(stderr, "grid-seq: Sent Programmer Mode SysEx to both outputs\n");
    }

    // Calculate step position before advancing
    uint64_t old_frame = gs->state.frame_counter;
    uint64_t old_step_frame = old_frame % gs->state.frames_per_step;
    bool was_before_half = old_step_frame < (gs->state.frames_per_step / 2);

    uint8_t midi_channel = 1;
    if (gs->midi_channel) {
        uint8_t channel_value = (uint8_t)(*gs->midi_channel);
        if (channel_value >= 1 && channel_value <= 16) {
            midi_channel = channel_value;
        }
    }

    // Always trigger first step on first run
    if (gs->state.first_run) {
        if (midi_out_connected) {
            sequencer_process_step(&gs->state, &gs->forge, &gs->seq_uris, 0, midi_channel);
        }
        gs->state.first_run = false;
    }
    // Check if we crossed a step boundary
    else if (sequencer_advance(&gs->state, n_samples)) {
        if (midi_out_connected) {
            sequencer_process_step(&gs->state, &gs->forge, &gs->seq_uris, 0, midi_channel);
        }
        gs->grid_dirty = true;  // Update LEDs when step changes
    }

    // Check if we crossed the 50% point (for Note Off)
    uint64_t new_frame = gs->state.frame_counter;
    uint64_t new_step_frame = new_frame % gs->state.frames_per_step;
    bool is_after_half = new_step_frame >= (gs->state.frames_per_step / 2);

    if (was_before_half && is_after_half) {
        // Calculate frame offset to the 50% point
        uint64_t half_point = (gs->state.frame_counter / gs->state.frames_per_step) * gs->state.frames_per_step
                             + (gs->state.frames_per_step / 2);
        uint32_t offset = (uint32_t)(half_point - old_frame);

        // Only send Note Offs if MIDI filter is disabled
        bool filter_enabled = (gs->midi_filter && *gs->midi_filter > 0.5f);
        if (!filter_enabled) {
            if (midi_out_connected) {
                sequencer_process_note_offs(&gs->state, &gs->forge, &gs->seq_uris, offset, midi_channel);
            }
        }
    }

    // Update Launchpad LEDs if grid changed or step changed
    if (gs->grid_dirty || gs->state.current_step != gs->prev_led_step) {
        fprintf(stderr, "grid-seq: Calling update_launchpad_leds (grid_dirty=%d, step=%d)\n",
                gs->grid_dirty, gs->state.current_step);
        uint8_t sysex[LP_SYSEX_MAX];
        size_t sysex_size = build_launchpad_led_sysex(gs, sysex, sizeof(sysex));
        if (sysex_size > 0) {
            send_launchpad_sysex(gs, sysex, (uint16_t)sysex_size);
        }
        update_launchpad_leds_note_cc(gs);
        gs->grid_dirty = false;
        gs->prev_led_step = gs->state.current_step;
    }

    // Send full grid state to UI if anything changed
    if (gs->last_toggled_x >= 0 && gs->last_toggled_y >= 0) {
        // Prepare grid data
        uint8_t grid_data[64];
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 8; y++) {
                uint8_t actual_note = gs->state.pitch_offset + (uint8_t)y;
                grid_data[x * 8 + y] = gs->state.grid[x][actual_note] ? 1 : 0;
            }
        }

        // Write using frame_time (frames, not beats) to match working MIDI pattern
        if (notify_connected) {
            lv2_atom_forge_frame_time(&gs->notify_forge, 0);
            lv2_atom_forge_atom(&gs->notify_forge, 64, gs->gridState);
            lv2_atom_forge_write(&gs->notify_forge, grid_data, 64);
        }

        gs->last_toggled_x = -1;
        gs->last_toggled_y = -1;
    }

    // End MIDI note sequence after LED updates so fallback writes still land.
    if (midi_out_connected) {
        lv2_atom_forge_pop(&gs->forge, &frame);
    }

    // End Launchpad control sequence
    if (launchpad_connected) {
        lv2_atom_forge_pop(&gs->launchpad_forge, &lp_frame);
    }

    // End UI notification sequence
    if (notify_connected) {
        lv2_atom_forge_pop(&gs->notify_forge, &notify_frame);
    }
}

static void deactivate(LV2_Handle instance) {
    GridSeq* gs = (GridSeq*)instance;
    gs->state.playing = false;
}

static void cleanup(LV2_Handle instance) {
    free(instance);
}

static const LV2_Descriptor descriptor = {
    .URI = PLUGIN_URI,
    .instantiate = instantiate,
    .connect_port = connect_port,
    .activate = activate,
    .run = run,
    .deactivate = deactivate,
    .cleanup = cleanup,
    .extension_data = NULL
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return (index == 0) ? &descriptor : NULL;
}
