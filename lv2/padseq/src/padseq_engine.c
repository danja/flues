#include "../include/padseq_engine.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

// ============================================================================
// Initialization
// ============================================================================

void padseq_init(PadSeqEngine *engine, float sample_rate) {
    memset(engine, 0, sizeof(PadSeqEngine));

    engine->sample_rate = sample_rate;
    engine->playing = 0;
    engine->current_step = 0;
    engine->sample_counter = 0;
    engine->selected_voice = 0;
    engine->active_columns = 8;
    for (uint8_t p = 0; p < 2; ++p) {
        for (uint8_t i = 0; i < MAX_DRUM_VOICES; ++i) {
            engine->euclid_pulses[p][i] = 0;
            engine->euclid_offset[p][i] = 0;
        }
    }

    grid_state_init(&engine->grid_state);
    padseq_set_tempo(engine, 120);

    static const uint8_t default_drum_notes[MAX_DRUM_VOICES] = {
        36, 40, 39, 50, 42, 46, 53, 51
    };
    for (uint8_t i = 0; i < MAX_DRUM_VOICES; i++) {
        engine->drum_voices[i].active = 1;
        engine->drum_voices[i].note = default_drum_notes[i];
        engine->drum_voices[i].color = COLOR_DRUMS;
        memset(engine->drum_voices[i].steps, 0, SEQ_STEPS);
    }

    memset(engine->drum_patterns, 0, sizeof(engine->drum_patterns));

    padseq_reset(engine);
}

void padseq_reset(PadSeqEngine *engine) {
    engine->playing = 0;
    engine->current_step = 0;
    engine->sample_counter = 0;
    grid_state_clear(&engine->grid_state);
    engine->midi_event_count = 0;
}

static void apply_euclid(PadSeqEngine *engine, uint8_t voice);

// ============================================================================
// MIDI Queue Helpers
// ============================================================================

static void queue_note_event(PadSeqEngine *engine, uint8_t note, uint8_t vel,
                             uint8_t channel, uint32_t gate_frames) {
    if (engine->midi_event_count + 2 > MAX_MIDI_EVENTS) return;
    uint8_t status_on  = (uint8_t)(0x90 | ((channel - 1) & 0x0F));
    uint8_t status_off = (uint8_t)(0x80 | ((channel - 1) & 0x0F));
    engine->midi_events[engine->midi_event_count++] = (MidiOutEvent){0, 3, {status_on, note, vel}};
    engine->midi_events[engine->midi_event_count++] = (MidiOutEvent){gate_frames ? gate_frames : 1, 3, {status_off, note, 0}};
}

// ============================================================================
// Tempo + Playback
// ============================================================================

void padseq_set_tempo(PadSeqEngine *engine, uint16_t bpm) {
    if (bpm < 30) bpm = 30;
    if (bpm > 300) bpm = 300;
    float samples_per_beat = (engine->sample_rate * 60.0f) / (float)bpm;
    engine->samples_per_step = (uint32_t)(samples_per_beat / 4.0f);  // 16th notes
    if (engine->samples_per_step == 0) engine->samples_per_step = 1;
    engine->grid_state.tempo_bpm = (uint8_t)bpm;
}

uint32_t padseq_default_gate_frames(const PadSeqEngine *engine) {
    float gate_seconds = DEFAULT_GATE_MS / 1000.0f;
    uint32_t gate_frames = (uint32_t)(engine->sample_rate * gate_seconds);
    if (gate_frames == 0) gate_frames = 1;
    return gate_frames;
}

void padseq_start(PadSeqEngine *engine) {
    engine->playing = 1;
    engine->current_step = 0;
    engine->sample_counter = 0;
    engine->grid_state.playing = 1;
}

void padseq_stop(PadSeqEngine *engine) {
    engine->playing = 0;
    engine->grid_state.playing = 0;
}

// ============================================================================
// Input Handling
// ============================================================================

void padseq_handle_pad_press(PadSeqEngine *engine, uint8_t row, uint8_t col, uint8_t velocity) {
    if (row >= GRID_HEIGHT || col >= GRID_WIDTH) return;

    uint8_t voice = engine->selected_voice;
    if (voice >= MAX_DRUM_VOICES) voice = 0;

    uint8_t step = (uint8_t)(row * GRID_WIDTH + col);  // 0-63
    if (step >= SEQ_STEPS) return;

    if (engine->drum_voices[voice].steps[step] == 0) {
        engine->drum_voices[voice].steps[step] = velocity;
        grid_state_set_led(&engine->grid_state, row, col, COLOR_YELLOW_DIM);
    } else {
        engine->drum_voices[voice].steps[step] = 0;
        grid_state_set_led(&engine->grid_state, row, col, COLOR_OFF);
    }
}

void padseq_handle_pad_release(PadSeqEngine *engine, uint8_t row, uint8_t col) {
    (void)engine;
    (void)row;
    (void)col;
}

void padseq_handle_side_button(PadSeqEngine *engine, uint8_t index) {
    if (index >= MAX_DRUM_VOICES) return;
    engine->selected_voice = index;

    for (uint8_t i = 0; i < MAX_DRUM_VOICES; i++) {
        uint8_t color = (i == index) ? COLOR_SELECTED : COLOR_DRUMS;
        grid_state_set_side_button(&engine->grid_state, i, 0, color);
    }
}

void padseq_handle_top_button(PadSeqEngine *engine, uint8_t index) {
    switch (index) {
        case 0: { // Euclid pulses up
            uint8_t total_steps = (uint8_t)(engine->active_columns * GRID_WIDTH);
            if (total_steps == 0) total_steps = 1;
            uint8_t voice = engine->selected_voice;
            if (voice >= MAX_DRUM_VOICES) voice = 0;
            uint8_t pattern = engine->grid_state.pattern & 1;
            engine->euclid_pulses[pattern][voice] =
                (uint8_t)((engine->euclid_pulses[pattern][voice] + 1) % (total_steps + 1));
            apply_euclid(engine, voice);
            break;
        }
        case 1: { // Euclid offset up
            uint8_t total_steps = (uint8_t)(engine->active_columns * GRID_WIDTH);
            if (total_steps == 0) total_steps = 1;
            uint8_t voice = engine->selected_voice;
            if (voice >= MAX_DRUM_VOICES) voice = 0;
            uint8_t pattern = engine->grid_state.pattern & 1;
            engine->euclid_offset[pattern][voice] =
                (uint8_t)((engine->euclid_offset[pattern][voice] + 1) % total_steps);
            apply_euclid(engine, voice);
            break;
        }
        case 2:  // Columns down
            if (engine->active_columns > 1) {
                engine->active_columns--;
            }
            if ((engine->current_step % GRID_WIDTH) >= engine->active_columns) {
                engine->current_step = (uint16_t)((engine->current_step / GRID_WIDTH) * GRID_WIDTH);
            }
            uint8_t total_steps = (uint8_t)(engine->active_columns * GRID_WIDTH);
            if (total_steps == 0) total_steps = 1;
            for (uint8_t p = 0; p < 2; ++p) {
                for (uint8_t v = 0; v < MAX_DRUM_VOICES; ++v) {
                    if (engine->euclid_pulses[p][v] > total_steps) {
                        engine->euclid_pulses[p][v] = total_steps;
                    }
                    engine->euclid_offset[p][v] %= total_steps;
                }
            }
            for (uint8_t v = 0; v < MAX_DRUM_VOICES; ++v) {
                if (engine->euclid_pulses[engine->grid_state.pattern & 1][v] > 0) {
                    apply_euclid(engine, v);
                }
            }
            break;
        case 3:  // Columns up
            if (engine->active_columns < GRID_WIDTH) {
                engine->active_columns++;
            }
            for (uint8_t v = 0; v < MAX_DRUM_VOICES; ++v) {
                if (engine->euclid_pulses[engine->grid_state.pattern & 1][v] > 0) {
                    apply_euclid(engine, v);
                }
            }
            break;
        case 4:  // Pattern A
            padseq_set_pattern(engine, 0);
            break;
        case 5:  // Pattern B
            padseq_set_pattern(engine, 1);
            break;
        case 6: { // Clear current voice
            uint8_t voice = engine->selected_voice;
            if (voice >= MAX_DRUM_VOICES) voice = 0;
            memset(engine->drum_voices[voice].steps, 0, SEQ_STEPS);
            memset(engine->drum_patterns[engine->grid_state.pattern & 1][voice], 0, SEQ_STEPS);
            engine->euclid_pulses[engine->grid_state.pattern & 1][voice] = 0;
            engine->euclid_offset[engine->grid_state.pattern & 1][voice] = 0;
            grid_state_set_top_button(&engine->grid_state, 6, 0, COLOR_RED_BRIGHT);
            break;
        }
        case 7: { // Clear pattern + Euclid
            uint8_t p = engine->grid_state.pattern & 1;
            for (uint8_t v = 0; v < MAX_DRUM_VOICES; ++v) {
                memset(engine->drum_voices[v].steps, 0, SEQ_STEPS);
                memset(engine->drum_patterns[p][v], 0, SEQ_STEPS);
                engine->euclid_pulses[p][v] = 0;
                engine->euclid_offset[p][v] = 0;
            }
            grid_state_set_top_button(&engine->grid_state, 7, 0, COLOR_RED_BRIGHT);
            break;
        }
        case 8:  // Play/Stop
            if (engine->playing) {
                padseq_stop(engine);
                grid_state_set_top_button(&engine->grid_state, 8, 0, COLOR_OFF);
            } else {
                padseq_start(engine);
                grid_state_set_top_button(&engine->grid_state, 8, 0, COLOR_GREEN_BRIGHT);
            }
            break;
        default:
            break;
    }
}

// ============================================================================
// Sequencer Processing
// ============================================================================

void padseq_begin_block(PadSeqEngine *engine) {
    engine->midi_event_count = 0;
}

void padseq_process(PadSeqEngine *engine, uint32_t n_samples) {
    if (!engine->playing) return;

    for (uint32_t i = 0; i < n_samples; i++) {
        engine->sample_counter++;

        if (engine->sample_counter >= engine->samples_per_step) {
            engine->sample_counter = 0;
            uint8_t row = (uint8_t)(engine->current_step / GRID_WIDTH);
            uint8_t col = (uint8_t)(engine->current_step % GRID_WIDTH);
            if (col >= engine->active_columns) {
                col = 0;
            }
            if ((uint8_t)(col + 1) >= engine->active_columns) {
                col = 0;
                row = (uint8_t)((row + 1) % GRID_HEIGHT);
            } else {
                col++;
            }
            engine->current_step = (uint16_t)(row * GRID_WIDTH + col);

            uint32_t gate_frames = engine->samples_per_step > 1
                ? (engine->samples_per_step / 2)
                : 1;
            if (gate_frames >= n_samples) {
                gate_frames = (n_samples > 1) ? (n_samples - 1) : 1;
            }

            for (uint8_t v = 0; v < MAX_DRUM_VOICES; v++) {
                if (engine->drum_voices[v].active &&
                    engine->drum_voices[v].steps[engine->current_step] > 0) {
                    uint8_t note = engine->drum_voices[v].note;
                    uint8_t vel = engine->drum_voices[v].steps[engine->current_step];
                    queue_note_event(engine, note, vel, 10, gate_frames);
                }
            }

            engine->grid_state.step_counter = engine->current_step;
        }
    }
}

// ============================================================================
// LED State Refresh
// ============================================================================

static int euclid_hit(uint8_t step, uint8_t pulses, uint8_t offset, uint8_t length) {
    if (length == 0 || pulses == 0) return 0;
    if (pulses >= length) return 1;
    uint8_t base_step = (uint8_t)((step + length - (offset % length)) % length);
    return ((base_step * pulses) % length) < pulses;
}

static void apply_euclid(PadSeqEngine *engine, uint8_t voice) {
    if (voice >= MAX_DRUM_VOICES) voice = 0;
    uint8_t total_steps = (uint8_t)(engine->active_columns * GRID_HEIGHT);
    if (total_steps == 0) return;
    uint8_t pattern = engine->grid_state.pattern & 1;
    for (uint8_t row = 0; row < GRID_HEIGHT; ++row) {
        for (uint8_t col = 0; col < GRID_WIDTH; ++col) {
            uint8_t step_index = (uint8_t)(row * GRID_WIDTH + col);
            if (col < engine->active_columns) {
                uint8_t logical_step = (uint8_t)(row * engine->active_columns + col);
                engine->drum_voices[voice].steps[step_index] =
                    euclid_hit(logical_step,
                               engine->euclid_pulses[pattern][voice],
                               engine->euclid_offset[pattern][voice],
                               total_steps) ? 96 : 0;
            } else {
                engine->drum_voices[voice].steps[step_index] = 0;
            }
        }
    }
}

void padseq_refresh_grid_state(PadSeqEngine *engine) {
    uint8_t voice = engine->selected_voice;
    if (voice >= MAX_DRUM_VOICES) voice = 0;

    for (uint8_t step = 0; step < SEQ_STEPS; step++) {
        uint8_t row = step / GRID_WIDTH;
        uint8_t col = step % GRID_WIDTH;
        uint8_t color = engine->drum_voices[voice].steps[step] > 0 ? COLOR_YELLOW_DIM : COLOR_OFF;
        if (col >= engine->active_columns) {
            color = COLOR_GRAY_DIM;
        }
        if (engine->playing && step == engine->current_step) {
            color = COLOR_PLAYHEAD;
        }
        grid_state_set_led(&engine->grid_state, row, col, color);
    }

    for (uint8_t i = 0; i < MAX_DRUM_VOICES && i < GRID_HEIGHT; ++i) {
        uint8_t color = (i == engine->selected_voice) ? COLOR_SELECTED : COLOR_DRUMS;
        grid_state_set_side_button(&engine->grid_state, i, 0, color);
    }

    grid_state_set_top_button(&engine->grid_state, 8, 0,
                              engine->playing ? COLOR_GREEN_BRIGHT : COLOR_OFF);
    grid_state_set_top_button(&engine->grid_state, 0, 0, COLOR_CYAN_DIM);
    grid_state_set_top_button(&engine->grid_state, 1, 0, COLOR_CYAN_DIM);
    grid_state_set_top_button(&engine->grid_state, 2, 0, COLOR_BLUE_DIM);
    grid_state_set_top_button(&engine->grid_state, 3, 0, COLOR_BLUE_DIM);
    grid_state_set_top_button(&engine->grid_state, 4, 0,
                              engine->grid_state.pattern == 0 ? COLOR_SELECTED : COLOR_YELLOW_DIM);
    grid_state_set_top_button(&engine->grid_state, 5, 0,
                              engine->grid_state.pattern == 1 ? COLOR_SELECTED : COLOR_YELLOW_DIM);
    grid_state_set_top_button(&engine->grid_state, 6, 0, COLOR_RED_DIM);
    grid_state_set_top_button(&engine->grid_state, 7, 0, COLOR_RED_DIM);
}

// ============================================================================
// Pattern Management
// ============================================================================

static void save_current_pattern(PadSeqEngine *engine) {
    uint8_t p = engine->grid_state.pattern & 1;
    for (uint8_t v = 0; v < MAX_DRUM_VOICES; ++v) {
        memcpy(engine->drum_patterns[p][v], engine->drum_voices[v].steps, SEQ_STEPS);
    }
}

static void load_pattern(PadSeqEngine *engine, uint8_t pattern) {
    uint8_t p = pattern & 1;
    for (uint8_t v = 0; v < MAX_DRUM_VOICES; ++v) {
        memcpy(engine->drum_voices[v].steps, engine->drum_patterns[p][v], SEQ_STEPS);
    }
}

void padseq_set_pattern(PadSeqEngine *engine, uint8_t pattern) {
    save_current_pattern(engine);
    engine->grid_state.pattern = (uint8_t)(pattern & 1);
    load_pattern(engine, engine->grid_state.pattern);
}

void padseq_clear_pattern(PadSeqEngine *engine) {
    for (uint8_t v = 0; v < MAX_DRUM_VOICES; v++) {
        memset(engine->drum_voices[v].steps, 0, SEQ_STEPS);
        memset(engine->drum_patterns[engine->grid_state.pattern & 1][v], 0, SEQ_STEPS);
    }
}
