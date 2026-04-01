#ifndef LAUNCHPAD_CONFIG_H
#define LAUNCHPAD_CONFIG_H

#include <stdint.h>

#define SYSEX_HEADER_SIZE 6
static const uint8_t SYSEX_HEADER[SYSEX_HEADER_SIZE] = {
    0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D
};

#define SYSEX_END 0xF7

#define SYSEX_CMD_SELECT_LAYOUT      0x00
#define SYSEX_CMD_DAW_FADER_SETUP    0x01
#define SYSEX_CMD_LED_LIGHTING       0x03
#define SYSEX_CMD_TEXT_SCROLL        0x07
#define SYSEX_CMD_BRIGHTNESS         0x08
#define SYSEX_CMD_SLEEP              0x09
#define SYSEX_CMD_LED_FEEDBACK       0x0A
#define SYSEX_CMD_PROG_LIVE_TOGGLE   0x0E
#define SYSEX_CMD_DAW_STANDALONE     0x10
#define SYSEX_CMD_CLEAR_DAW          0x12
#define SYSEX_CMD_SESSION_COLOUR     0x14

#define LAYOUT_SESSION       0x00
#define LAYOUT_CUSTOM_1      0x04
#define LAYOUT_CUSTOM_2      0x05
#define LAYOUT_CUSTOM_3      0x06
#define LAYOUT_DAW_FADERS    0x0D
#define LAYOUT_PROGRAMMER    0x7F

#define MODE_LIVE            0x00
#define MODE_PROGRAMMER      0x01
#define MODE_STANDALONE      0x00
#define MODE_DAW             0x01

#define MIDI_CH_STATIC       0x00
#define MIDI_CH_FLASH        0x01
#define MIDI_CH_PULSE        0x02

#define MIDI_NOTE_ON_CH1     0x90
#define MIDI_NOTE_ON_CH2     0x91
#define MIDI_NOTE_ON_CH3     0x92
#define MIDI_CC_CH1          0xB0
#define MIDI_CC_CH2          0xB1
#define MIDI_CC_CH3          0xB2

#define GRID_WIDTH  8
#define GRID_HEIGHT 8
#define GRID_SIZE   (GRID_WIDTH * GRID_HEIGHT)

static const uint8_t GRID_NOTES[GRID_HEIGHT][GRID_WIDTH] = {
    {11, 12, 13, 14, 15, 16, 17, 18},
    {21, 22, 23, 24, 25, 26, 27, 28},
    {31, 32, 33, 34, 35, 36, 37, 38},
    {41, 42, 43, 44, 45, 46, 47, 48},
    {51, 52, 53, 54, 55, 56, 57, 58},
    {61, 62, 63, 64, 65, 66, 67, 68},
    {71, 72, 73, 74, 75, 76, 77, 78},
    {81, 82, 83, 84, 85, 86, 87, 88}
};

static const uint8_t SIDE_BUTTONS[GRID_HEIGHT] = {
    19, 29, 39, 49, 59, 69, 79, 89
};

static const uint8_t TOP_BUTTONS[9] = {
    91, 92, 93, 94, 95, 96, 97, 98, 99
};

#define COLOR_OFF           0
#define COLOR_GRAY_DIM      1
#define COLOR_GRAY_MED      2
#define COLOR_WHITE         3
#define COLOR_RED_DIM       4
#define COLOR_RED           5
#define COLOR_RED_BRIGHT    6
#define COLOR_ORANGE_DIM    7
#define COLOR_ORANGE        9
#define COLOR_ORANGE_BRIGHT 84
#define COLOR_YELLOW_DIM    13
#define COLOR_YELLOW        13
#define COLOR_YELLOW_BRIGHT 14
#define COLOR_GREEN_DIM     20
#define COLOR_GREEN         21
#define COLOR_GREEN_BRIGHT  22
#define COLOR_CYAN_DIM      33
#define COLOR_CYAN          37
#define COLOR_CYAN_BRIGHT   38
#define COLOR_BLUE_DIM      44
#define COLOR_BLUE          45
#define COLOR_BLUE_BRIGHT   46
#define COLOR_PURPLE_DIM    48
#define COLOR_PURPLE        53
#define COLOR_PURPLE_BRIGHT 54
#define COLOR_PINK_DIM      56
#define COLOR_PINK          57
#define COLOR_PINK_BRIGHT   58

#define LED_TYPE_STATIC     0x00
#define LED_TYPE_FLASH      0x01
#define LED_TYPE_PULSE      0x02
#define LED_TYPE_RGB        0x03

static inline uint8_t grid_to_note(uint8_t row, uint8_t col) {
    if (row >= GRID_HEIGHT || col >= GRID_WIDTH) return 0;
    return GRID_NOTES[row][col];
}

static inline int note_to_grid(uint8_t note, uint8_t *row, uint8_t *col) {
    for (uint8_t r = 0; r < GRID_HEIGHT; r++) {
        for (uint8_t c = 0; c < GRID_WIDTH; c++) {
            if (GRID_NOTES[r][c] == note) {
                *row = r;
                *col = c;
                return 1;
            }
        }
    }
    return 0;
}

static inline int is_side_button(uint8_t cc, uint8_t *index) {
    for (uint8_t i = 0; i < GRID_HEIGHT; i++) {
        if (SIDE_BUTTONS[i] == cc) {
            *index = i;
            return 1;
        }
    }
    return 0;
}

static inline int is_top_button(uint8_t cc, uint8_t *index) {
    for (uint8_t i = 0; i < 9; i++) {
        if (TOP_BUTTONS[i] == cc) {
            *index = i;
            return 1;
        }
    }
    return 0;
}

#endif
