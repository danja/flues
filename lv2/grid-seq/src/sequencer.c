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

#include "sequencer.h"
#include <stdio.h>

static void s_send_midi_message(
    LV2_Atom_Forge* forge,
    const SequencerURIDs* uris,
    uint32_t frame_offset,
    uint8_t status,
    uint8_t note,
    uint8_t velocity
) {
    uint8_t midi_data[3] = {status, note, velocity};

    fprintf(stderr, "grid-seq: MIDI OUTPUT - status=0x%02X note=%d vel=%d\n", status, note, velocity);

    lv2_atom_forge_frame_time(forge, frame_offset);
    lv2_atom_forge_atom(forge, 3, uris->midi_MidiEvent);
    lv2_atom_forge_write(forge, midi_data, 3);
}

void sequencer_process_step(
    GridSeqState* state,
    LV2_Atom_Forge* forge,
    const SequencerURIDs* uris,
    uint32_t frame_offset
) {
    if (!state || !forge || !uris) return;

    // Send Note On for current step
    uint8_t x = state->current_step;

    // Play all active notes across full MIDI range
    for (uint8_t note = 0; note < GRID_PITCH_RANGE; note++) {
        if (state->grid[x][note]) {
            fprintf(stderr, "grid-seq: Step %d - SENDING NOTE ON: %d from grid[%d][%d]\n",
                    x, note, x, note);
            s_send_midi_message(forge, uris, frame_offset, 0x90, note, 100);
            state->active_notes[note] = true;
        }
    }

    // Update previous step
    state->previous_step = state->current_step;
}

void sequencer_process_note_offs(
    GridSeqState* state,
    LV2_Atom_Forge* forge,
    const SequencerURIDs* uris,
    uint32_t frame_offset
) {
    if (!state || !forge || !uris) return;

    // Send Note Off for all currently active notes
    for (uint8_t note = 0; note < 128; note++) {
        if (state->active_notes[note]) {
            s_send_midi_message(forge, uris, frame_offset, 0x80, note, 0);
            state->active_notes[note] = false;
        }
    }
}

bool sequencer_advance(GridSeqState* state, uint32_t n_samples) {
    if (!state || !state->playing) return false;

    uint64_t old_step = state->frame_counter / state->frames_per_step;
    state->frame_counter += n_samples;
    uint64_t new_step = state->frame_counter / state->frames_per_step;

    if (new_step != old_step) {
        state->current_step = (uint8_t)(new_step % state->sequence_length);
        return true;
    }

    return false;
}
