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

#include "launchpad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>

struct LaunchpadController {
    int fd;
    char device_path[256];
};

LaunchpadController* launchpad_init(int card_num) {
    LaunchpadController* lp = (LaunchpadController*)calloc(1, sizeof(LaunchpadController));
    if (!lp) return NULL;

    // Try to open the MIDI device
    snprintf(lp->device_path, sizeof(lp->device_path), "/dev/snd/midiC%dD0", card_num);

    lp->fd = open(lp->device_path, O_RDWR | O_NONBLOCK);
    if (lp->fd < 0) {
        free(lp);
        return NULL;
    }

    return lp;
}

void launchpad_cleanup(LaunchpadController* lp) {
    if (!lp) return;

    if (lp->fd >= 0) {
        launchpad_exit_programmer_mode(lp);
        close(lp->fd);
    }

    free(lp);
}

bool launchpad_enter_programmer_mode(LaunchpadController* lp) {
    if (!lp || lp->fd < 0) return false;

    // SysEx: F0 00 20 29 02 0D 0E 01 F7
    uint8_t sysex[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, 0x01, 0xF7};

    ssize_t written = write(lp->fd, sysex, sizeof(sysex));
    return written == sizeof(sysex);
}

bool launchpad_exit_programmer_mode(LaunchpadController* lp) {
    if (!lp || lp->fd < 0) return false;

    // SysEx: F0 00 20 29 02 0D 0E 00 F7
    uint8_t sysex[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, 0x00, 0xF7};

    ssize_t written = write(lp->fd, sysex, sizeof(sysex));
    return written == sizeof(sysex);
}

bool launchpad_set_led(LaunchpadController* lp, uint8_t note, uint8_t color) {
    if (!lp || lp->fd < 0) return false;

    // Note On message: 0x90 (channel 1), note, velocity (color)
    uint8_t msg[3] = {0x90, note, color};

    ssize_t written = write(lp->fd, msg, sizeof(msg));
    return written == sizeof(msg);
}

bool launchpad_update_grid(LaunchpadController* lp, const GridSeqState* state) {
    if (!lp || !state) return false;

    for (uint8_t x = 0; x < GRID_SIZE; x++) {
        for (uint8_t y = 0; y < GRID_SIZE; y++) {
            uint8_t note = lp_grid_to_note(x, y);
            uint8_t color;

            if (x == state->current_step) {
                // Current step - yellow if active, dim yellow if not
                color = state->grid[x][y] ? LP_COLOR_YELLOW : LP_COLOR_GREEN_DIM;
            } else {
                // Other steps - green if active, off if not
                color = state->grid[x][y] ? LP_COLOR_GREEN : LP_COLOR_OFF;
            }

            if (!launchpad_set_led(lp, note, color)) {
                return false;
            }
        }
    }

    return true;
}

bool launchpad_poll_input(LaunchpadController* lp, GridSeqState* state) {
    if (!lp || !state || lp->fd < 0) return false;

    uint8_t buffer[256];
    ssize_t bytes_read = read(lp->fd, buffer, sizeof(buffer));

    if (bytes_read <= 0) {
        return false; // No data or error
    }

    bool grid_changed = false;

    // Process MIDI messages
    for (ssize_t i = 0; i < bytes_read; ) {
        uint8_t status = buffer[i];

        // Note On (0x90) or Note Off (0x80)
        if ((status & 0xF0) == 0x90 || (status & 0xF0) == 0x80) {
            if (i + 2 < bytes_read) {
                uint8_t note = buffer[i + 1];
                uint8_t velocity = buffer[i + 2];

                // Only process Note On with velocity > 0 (button press)
                if ((status & 0xF0) == 0x90 && velocity > 0) {
                    // Check if it's a grid button (notes 11-88)
                    if (note >= 11 && note <= 88) {
                        uint8_t x, y;
                        lp_note_to_grid(note, &x, &y);

                        if (x < GRID_SIZE && y < GRID_SIZE) {
                            state_toggle_step(state, x, y);
                            grid_changed = true;
                        }
                    }
                }

                i += 3;
            } else {
                break;
            }
        }
        // Control Change (0xB0)
        else if ((status & 0xF0) == 0xB0) {
            if (i + 2 < bytes_read) {
                // uint8_t cc = buffer[i + 1];
                // uint8_t value = buffer[i + 2];
                // TODO: Handle top row and scene buttons
                i += 3;
            } else {
                break;
            }
        }
        else {
            // Skip unknown message
            i++;
        }
    }

    return grid_changed;
}
