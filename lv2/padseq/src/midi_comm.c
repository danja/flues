#include "../include/midi_comm.h"
#include <string.h>

// ============================================================================
// Helper Functions
// ============================================================================

static void reset_message(MidiMessage *msg) {
    msg->size = 0;
}

static void append_byte(MidiMessage *msg, uint8_t byte) {
    if (msg->size < MIDI_MAX_MSG_SIZE) {
        msg->data[msg->size++] = byte;
    }
}

static void append_sysex_header(MidiMessage *msg) {
    for (int i = 0; i < SYSEX_HEADER_SIZE; i++) {
        append_byte(msg, SYSEX_HEADER[i]);
    }
}

// ============================================================================
// SysEx Message Builders
// ============================================================================

void midi_build_set_mode(MidiMessage *msg, uint8_t mode) {
    reset_message(msg);
    append_sysex_header(msg);
    append_byte(msg, SYSEX_CMD_PROG_LIVE_TOGGLE);
    append_byte(msg, mode);
    append_byte(msg, SYSEX_END);
}

void midi_build_set_led_note(MidiMessage *msg, uint8_t note, uint8_t color,
                              uint8_t lighting_mode) {
    reset_message(msg);

    // Select MIDI channel based on lighting mode
    uint8_t status;
    switch (lighting_mode) {
        case 0: status = MIDI_NOTE_ON_CH1; break;  // Static
        case 1: status = MIDI_NOTE_ON_CH2; break;  // Flash
        case 2: status = MIDI_NOTE_ON_CH3; break;  // Pulse
        default: status = MIDI_NOTE_ON_CH1; break;
    }

    append_byte(msg, status);
    append_byte(msg, note);
    append_byte(msg, color);
}

void midi_build_set_led_cc(MidiMessage *msg, uint8_t cc, uint8_t color,
                            uint8_t lighting_mode) {
    reset_message(msg);

    // Select MIDI channel based on lighting mode
    uint8_t status;
    switch (lighting_mode) {
        case 0: status = MIDI_CC_CH1; break;  // Static
        case 1: status = MIDI_CC_CH2; break;  // Flash
        case 2: status = MIDI_CC_CH3; break;  // Pulse
        default: status = MIDI_CC_CH1; break;
    }

    append_byte(msg, status);
    append_byte(msg, cc);
    append_byte(msg, color);
}

void midi_build_led_bulk(MidiMessage *msg, const LaunchpadState *state) {
    reset_message(msg);
    append_sysex_header(msg);
    append_byte(msg, SYSEX_CMD_LED_LIGHTING);

    // Add color specs for all grid cells
    for (uint8_t r = 0; r < GRID_HEIGHT; r++) {
        for (uint8_t c = 0; c < GRID_WIDTH; c++) {
            uint8_t note = grid_to_note(r, c);
            uint8_t color = state->grid[r][c].color;

            // Type 0: Static color from palette
            append_byte(msg, LED_TYPE_STATIC);
            append_byte(msg, note);
            append_byte(msg, color);
        }
    }

    // Add side buttons
    for (uint8_t i = 0; i < GRID_HEIGHT; i++) {
        append_byte(msg, LED_TYPE_STATIC);
        append_byte(msg, SIDE_BUTTONS[i]);
        append_byte(msg, state->side[i].color);
    }

    // Add top buttons
    for (uint8_t i = 0; i < 9; i++) {
        append_byte(msg, LED_TYPE_STATIC);
        append_byte(msg, TOP_BUTTONS[i]);
        append_byte(msg, state->top[i].color);
    }

    append_byte(msg, SYSEX_END);
}

void midi_build_clear_all(MidiMessage *msg) {
    reset_message(msg);
    append_sysex_header(msg);
    append_byte(msg, SYSEX_CMD_LED_LIGHTING);

    // Send off message for all 81 LEDs
    for (uint8_t r = 0; r < GRID_HEIGHT; r++) {
        for (uint8_t c = 0; c < GRID_WIDTH; c++) {
            append_byte(msg, LED_TYPE_STATIC);
            append_byte(msg, grid_to_note(r, c));
            append_byte(msg, COLOR_OFF);
        }
    }

    append_byte(msg, SYSEX_END);
}

void midi_build_scroll_text(MidiMessage *msg, const char *text, uint8_t color,
                             uint8_t speed, uint8_t loop) {
    reset_message(msg);
    append_sysex_header(msg);
    append_byte(msg, SYSEX_CMD_TEXT_SCROLL);
    append_byte(msg, loop);
    append_byte(msg, speed);

    // Color spec: 0 = palette color, then color index
    append_byte(msg, 0);
    append_byte(msg, color);

    // Add text
    while (*text) {
        append_byte(msg, (uint8_t)*text);
        text++;
    }

    append_byte(msg, SYSEX_END);
}

void midi_build_set_brightness(MidiMessage *msg, uint8_t brightness) {
    reset_message(msg);
    append_sysex_header(msg);
    append_byte(msg, SYSEX_CMD_BRIGHTNESS);
    append_byte(msg, brightness);
    append_byte(msg, SYSEX_END);
}

// ============================================================================
// Message Parsing
// ============================================================================

int midi_parse_event(const uint8_t *data, uint16_t size, MidiEvent *event) {
    if (size < 3) return 0;  // Need at least 3 bytes for Note/CC

    uint8_t status = data[0];
    uint8_t channel = status & 0x0F;
    uint8_t msg_type = status & 0xF0;

    event->channel = channel;

    // Note On
    if (msg_type == 0x90) {
        event->type = (data[2] == 0) ? MIDI_EVENT_NOTE_OFF : MIDI_EVENT_NOTE_ON;
        event->note_or_cc = data[1];
        event->velocity_or_value = data[2];
        return 1;
    }

    // Note Off
    if (msg_type == 0x80) {
        event->type = MIDI_EVENT_NOTE_OFF;
        event->note_or_cc = data[1];
        event->velocity_or_value = data[2];
        return 1;
    }

    // Control Change
    if (msg_type == 0xB0) {
        event->type = MIDI_EVENT_CC;
        event->note_or_cc = data[1];
        event->velocity_or_value = data[2];
        return 1;
    }

    event->type = MIDI_EVENT_NONE;
    return 0;
}

// ============================================================================
// Grid Update
// ============================================================================

void midi_update_grid(MidiMessage *msg, const LaunchpadState *state) {
    midi_build_led_bulk(msg, state);
}

void midi_update_cell(MidiMessage *msg, uint8_t row, uint8_t col,
                      const GridCell *cell) {
    uint8_t note = grid_to_note(row, col);
    uint8_t lighting_mode = (cell->state & CELL_PLAYING) ? 2 : 0;  // Pulse if playing
    midi_build_set_led_note(msg, note, cell->color, lighting_mode);
}

void midi_update_side_button(MidiMessage *msg, uint8_t index, uint8_t color) {
    if (index >= GRID_HEIGHT) return;
    midi_build_set_led_cc(msg, SIDE_BUTTONS[index], color, 0);
}

void midi_update_top_button(MidiMessage *msg, uint8_t index, uint8_t color) {
    if (index >= 9) return;
    midi_build_set_led_cc(msg, TOP_BUTTONS[index], color, 0);
}
