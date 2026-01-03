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

#ifndef GRID_SEQ_COMMON_H
#define GRID_SEQ_COMMON_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_GRID_SIZE 16
#define GRID_SIZE 8  // Default size (for backward compatibility)
#define GRID_PITCH_RANGE 128  // Full MIDI range (0-127)
#define GRID_VISIBLE_ROWS 8   // Number of rows shown at once
#define DEFAULT_PITCH_OFFSET 36  // C2 - default base note
#define MIN_SEQUENCE_LENGTH 2
#define MAX_SEQUENCE_LENGTH 16
#define DEFAULT_SEQUENCE_LENGTH 8

#define PLUGIN_URI "http://github.com/danny/grid-seq"
#define GRID_SEQ_URI PLUGIN_URI "#"
#define GRID_SEQ__gridState GRID_SEQ_URI "gridState"
#define GRID_SEQ__cellX GRID_SEQ_URI "cellX"
#define GRID_SEQ__cellY GRID_SEQ_URI "cellY"
#define GRID_SEQ__cellValue GRID_SEQ_URI "cellValue"

typedef enum {
    GS_OK = 0,
    GS_ERROR_NULL_POINTER,
    GS_ERROR_INVALID_PARAM,
    GS_ERROR_OUT_OF_MEMORY
} GridSeqError;

#endif // GRID_SEQ_COMMON_H
