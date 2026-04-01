#include "../include/midi_comm.h"
#include <string.h>

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
    uint8_t status = MIDI_NOTE_ON_CH1;
    if (lighting_mode == 1) status = MIDI_NOTE_ON_CH2;
    else if (lighting_mode == 2) status = MIDI_NOTE_ON_CH3;
    append_byte(msg, status);
    append_byte(msg, note);
    append_byte(msg, color);
}

void midi_build_set_led_cc(MidiMessage *msg, uint8_t cc, uint8_t color,
                           uint8_t lighting_mode) {
    reset_message(msg);
    uint8_t status = MIDI_CC_CH1;
    if (lighting_mode == 1) status = MIDI_CC_CH2;
    else if (lighting_mode == 2) status = MIDI_CC_CH3;
    append_byte(msg, status);
    append_byte(msg, cc);
    append_byte(msg, color);
}

void midi_build_led_bulk(MidiMessage *msg, const LaunchpadState *state) {
    reset_message(msg);
    append_sysex_header(msg);
    append_byte(msg, SYSEX_CMD_LED_LIGHTING);

    for (uint8_t r = 0; r < GRID_HEIGHT; r++) {
        for (uint8_t c = 0; c < GRID_WIDTH; c++) {
            const GridCell *cell = &state->grid[r][c];
            append_byte(msg, (cell->state & CELL_PLAYING) ? LED_TYPE_PULSE :
                             (cell->state & CELL_SELECTED) ? LED_TYPE_FLASH :
                             LED_TYPE_STATIC);
            append_byte(msg, grid_to_note(r, c));
            append_byte(msg, cell->color);
        }
    }

    for (uint8_t i = 0; i < GRID_HEIGHT; i++) {
        append_byte(msg, LED_TYPE_STATIC);
        append_byte(msg, SIDE_BUTTONS[i]);
        append_byte(msg, state->side[i].color);
    }

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
    for (uint8_t r = 0; r < GRID_HEIGHT; r++) {
        for (uint8_t c = 0; c < GRID_WIDTH; c++) {
            append_byte(msg, LED_TYPE_STATIC);
            append_byte(msg, grid_to_note(r, c));
            append_byte(msg, COLOR_OFF);
        }
    }
    for (uint8_t i = 0; i < GRID_HEIGHT; ++i) {
        append_byte(msg, LED_TYPE_STATIC);
        append_byte(msg, SIDE_BUTTONS[i]);
        append_byte(msg, COLOR_OFF);
    }
    for (uint8_t i = 0; i < 9; ++i) {
        append_byte(msg, LED_TYPE_STATIC);
        append_byte(msg, TOP_BUTTONS[i]);
        append_byte(msg, COLOR_OFF);
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
    append_byte(msg, 0);
    append_byte(msg, color);
    while (*text) {
        append_byte(msg, (uint8_t)*text++);
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

int midi_parse_event(const uint8_t *data, uint16_t size, MidiEvent *event) {
    if (size < 3) return 0;

    const uint8_t status = data[0];
    event->channel = status & 0x0F;

    if ((status & 0xF0) == 0x90) {
        event->type = (data[2] == 0) ? MIDI_EVENT_NOTE_OFF : MIDI_EVENT_NOTE_ON;
        event->note_or_cc = data[1];
        event->velocity_or_value = data[2];
        return 1;
    }
    if ((status & 0xF0) == 0x80) {
        event->type = MIDI_EVENT_NOTE_OFF;
        event->note_or_cc = data[1];
        event->velocity_or_value = data[2];
        return 1;
    }
    if ((status & 0xF0) == 0xB0) {
        event->type = MIDI_EVENT_CC;
        event->note_or_cc = data[1];
        event->velocity_or_value = data[2];
        return 1;
    }

    event->type = MIDI_EVENT_NONE;
    return 0;
}

void midi_update_grid(MidiMessage *msg, const LaunchpadState *state) {
    midi_build_led_bulk(msg, state);
}

void midi_update_cell(MidiMessage *msg, uint8_t row, uint8_t col,
                      const GridCell *cell) {
    const uint8_t lighting_mode = (cell->state & CELL_PLAYING) ? 2 :
                                  (cell->state & CELL_SELECTED) ? 1 : 0;
    midi_build_set_led_note(msg, grid_to_note(row, col), cell->color, lighting_mode);
}

void midi_update_side_button(MidiMessage *msg, uint8_t index, uint8_t color) {
    if (index >= GRID_HEIGHT) return;
    midi_build_set_led_cc(msg, SIDE_BUTTONS[index], color, 0);
}

void midi_update_top_button(MidiMessage *msg, uint8_t index, uint8_t color) {
    if (index >= 9) return;
    midi_build_set_led_cc(msg, TOP_BUTTONS[index], color, 0);
}
