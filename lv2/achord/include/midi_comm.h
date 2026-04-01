#ifndef MIDI_COMM_H
#define MIDI_COMM_H

#include <stdint.h>
#include "launchpad_config.h"
#include "grid_state.h"

#define MIDI_MAX_MSG_SIZE 512

typedef struct {
    uint8_t data[MIDI_MAX_MSG_SIZE];
    uint16_t size;
} MidiMessage;

typedef enum {
    MIDI_EVENT_NONE,
    MIDI_EVENT_NOTE_ON,
    MIDI_EVENT_NOTE_OFF,
    MIDI_EVENT_CC
} MidiEventType;

typedef struct {
    MidiEventType type;
    uint8_t channel;
    uint8_t note_or_cc;
    uint8_t velocity_or_value;
} MidiEvent;

#ifdef __cplusplus
extern "C" {
#endif

void midi_build_set_mode(MidiMessage *msg, uint8_t mode);
void midi_build_set_led_note(MidiMessage *msg, uint8_t note, uint8_t color,
                             uint8_t lighting_mode);
void midi_build_set_led_cc(MidiMessage *msg, uint8_t cc, uint8_t color,
                           uint8_t lighting_mode);
void midi_build_led_bulk(MidiMessage *msg, const LaunchpadState *state);
void midi_build_clear_all(MidiMessage *msg);
void midi_build_scroll_text(MidiMessage *msg, const char *text, uint8_t color,
                            uint8_t speed, uint8_t loop);
void midi_build_set_brightness(MidiMessage *msg, uint8_t brightness);
int midi_parse_event(const uint8_t *data, uint16_t size, MidiEvent *event);
void midi_update_grid(MidiMessage *msg, const LaunchpadState *state);
void midi_update_cell(MidiMessage *msg, uint8_t row, uint8_t col,
                      const GridCell *cell);
void midi_update_side_button(MidiMessage *msg, uint8_t index, uint8_t color);
void midi_update_top_button(MidiMessage *msg, uint8_t index, uint8_t color);

#ifdef __cplusplus
}
#endif

#endif
