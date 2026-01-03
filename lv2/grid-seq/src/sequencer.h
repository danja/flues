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

#ifndef GRID_SEQ_SEQUENCER_H
#define GRID_SEQ_SEQUENCER_H

#include "state.h"
#include <lv2/atom/forge.h>

typedef struct {
    LV2_URID midi_MidiEvent;
} SequencerURIDs;

/**
 * Process one step of the sequencer.
 * Generates MIDI note events for active steps in the current column.
 *
 * @param state Pointer to state structure
 * @param forge LV2 atom forge for writing MIDI events
 * @param uris URID mappings
 * @param frame_offset Frame offset for this event
 */
void sequencer_process_step(
    GridSeqState* state,
    LV2_Atom_Forge* forge,
    const SequencerURIDs* uris,
    uint32_t frame_offset
);

/**
 * Process Note Off events for active notes (called at 50% of step).
 *
 * @param state Pointer to state structure
 * @param forge Atom forge for writing MIDI events
 * @param uris URIDs structure
 * @param frame_offset Frame offset for MIDI events
 */
void sequencer_process_note_offs(
    GridSeqState* state,
    LV2_Atom_Forge* forge,
    const SequencerURIDs* uris,
    uint32_t frame_offset
);

/**
 * Advance the sequencer by n_samples.
 * Returns true if a step boundary was crossed.
 *
 * @param state Pointer to state structure
 * @param n_samples Number of samples to advance
 * @return true if step changed, false otherwise
 */
bool sequencer_advance(GridSeqState* state, uint32_t n_samples);

#endif // GRID_SEQ_SEQUENCER_H
