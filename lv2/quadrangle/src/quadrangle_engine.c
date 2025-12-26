#include "../include/quadrangle_engine.h"
#include <string.h>
#include <math.h>

// ============================================================================
// Scale Definitions
// ============================================================================

static const uint8_t SCALE_INTERVALS[][12] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},       // Chromatic
    {0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19},   // Major
    {0, 2, 3, 5, 7, 8, 10, 12, 14, 15, 17, 19},   // Minor
    {0, 2, 4, 7, 9, 12, 14, 16, 19, 21, 24, 26},  // Pentatonic
    {0, 3, 5, 6, 7, 10, 12, 15, 17, 18, 19, 22},  // Blues
    {0, 2, 3, 5, 7, 9, 10, 12, 14, 15, 17, 19}    // Dorian
};

// ============================================================================
// Initialization
// ============================================================================

void quadrangle_init(QuadrangleEngine *engine, float sample_rate) {
    memset(engine, 0, sizeof(QuadrangleEngine));

    engine->sample_rate = sample_rate;
    engine->playing = 0;
    engine->current_step = 0;
    engine->sample_counter = 0;
    engine->selected_voice = 0;
    engine->selected_quadrant = 0;

    // Initialize grid state
    grid_state_init(&engine->grid_state);

    // Set default tempo (120 BPM, 16th notes)
    quadrangle_set_tempo(engine, 120);

    // Initialize drum voices
    for (uint8_t i = 0; i < MAX_DRUM_VOICES; i++) {
        engine->drum_voices[i].active = 1;
        engine->drum_voices[i].note = 36 + i;  // C2-G2 (GM drum notes)
        engine->drum_voices[i].color = COLOR_DRUMS;
        memset(engine->drum_voices[i].steps, 0, SEQ_STEPS);
    }

    // Initialize melody sequencer
    engine->melody.active = 1;
    engine->melody.root_note = 60;  // C4
    engine->melody.scale = SCALE_MAJOR;
    memset(engine->melody.notes, 0, SEQ_STEPS);
    memset(engine->melody.velocities, 0, SEQ_STEPS);

    // Initialize live pads
    for (uint8_t i = 0; i < LIVE_PADS; i++) {
        engine->live_pads[i].note = 60 + i;
        engine->live_pads[i].velocity = 100;
        engine->live_pads[i].mode = 0;  // Oneshot
        engine->live_pads[i].state = 0;
    }

    // Initialize parameters
    const char *param_names[PARAM_CONTROLS] = {
        "Filter", "Resonance", "Drive", "Rev Size",
        "Rev Mix", "Delay", "Feedback", "Master",
        "Tune", "Attack", "Release", "Swing",
        "Tempo", "Length", "Custom1", "Custom2"
    };

    for (uint8_t i = 0; i < PARAM_CONTROLS; i++) {
        engine->params[i].type = (ParameterType)i;
        engine->params[i].value = 64;  // Center
        engine->params[i].name = param_names[i];
    }

    quadrangle_reset(engine);
}

void quadrangle_reset(QuadrangleEngine *engine) {
    engine->playing = 0;
    engine->current_step = 0;
    engine->sample_counter = 0;
    grid_state_clear(&engine->grid_state);
    engine->midi_event_count = 0;
}

// Queue a note on/off pair with configurable gate (frame offsets)
static void queue_note_event(QuadrangleEngine *engine, uint8_t note, uint8_t vel, uint8_t channel, uint32_t gate_frames) {
    if (engine->midi_event_count + 2 > MAX_MIDI_EVENTS) return;
    uint8_t status_on  = (uint8_t)(0x90 | ((channel - 1) & 0x0F));
    uint8_t status_off = (uint8_t)(0x80 | ((channel - 1) & 0x0F));
    engine->midi_events[engine->midi_event_count++] = (MidiOutEvent){0, 3, {status_on, note, vel}};
    engine->midi_events[engine->midi_event_count++] = (MidiOutEvent){gate_frames ? gate_frames : 1, 3, {status_off, note, 0}};
}

// Queue a CC event (channel 1-16)
static void queue_cc_event(QuadrangleEngine *engine, uint8_t cc, uint8_t value, uint8_t channel) {
    if (engine->midi_event_count + 1 > MAX_MIDI_EVENTS) return;
    uint8_t status = (uint8_t)(0xB0 | ((channel - 1) & 0x0F));
    engine->midi_events[engine->midi_event_count++] = (MidiOutEvent){0, 3, {status, cc, value}};
}
// ============================================================================
// Tempo Control
// ============================================================================

void quadrangle_set_tempo(QuadrangleEngine *engine, uint16_t bpm) {
    if (bpm < 60) bpm = 60;
    if (bpm > 240) bpm = 240;

    engine->grid_state.tempo_bpm = bpm;

    // Calculate samples per step (16th note at given BPM)
    // BPM = quarter notes per minute
    // 16th note = 1/4 of quarter note
    // samples_per_16th = (60 / BPM) * sample_rate / 4
    float seconds_per_16th = 60.0f / (float)bpm / 4.0f;
    engine->samples_per_step = (uint32_t)(seconds_per_16th * engine->sample_rate);
}

// Default gate length in frames (~DEFAULT_GATE_MS)
uint32_t quadrangle_default_gate_frames(const QuadrangleEngine *engine) {
    return (uint32_t)(engine->sample_rate * (DEFAULT_GATE_MS / 1000.0f));
}

// ============================================================================
// Transport Control
// ============================================================================

void quadrangle_start(QuadrangleEngine *engine) {
    engine->playing = 1;
    engine->current_step = 0;
    engine->sample_counter = 0;
    engine->grid_state.playing = 1;
}

void quadrangle_stop(QuadrangleEngine *engine) {
    engine->playing = 0;
    engine->grid_state.playing = 0;
}

// ============================================================================
// Pad Press Handling
// ============================================================================

void quadrangle_handle_pad_press(QuadrangleEngine *engine, uint8_t row, uint8_t col, uint8_t velocity) {
    Quadrant quad;
    uint8_t local_row, local_col;

    if (!grid_state_get_quadrant(row, col, &quad, &local_row, &local_col)) {
        return;
    }

    switch (quad) {
        case QUADRANT_DRUMS: {
            // Drum sequencer: 4 rows × 4 visible steps (16 total steps, 4x4 grid)
            // Each row = one drum voice (0-3 visible, can select 0-7)
            // Each column = step position (0-3 visible of 0-15 total)
            // Use side buttons to select voice bank and step page

            uint8_t voice = engine->selected_voice;
            if (voice >= MAX_DRUM_VOICES) voice = 0;

            uint8_t step = local_col + (local_row * 4);  // 0-15
            if (step >= SEQ_STEPS) break;

            // Toggle step
            if (engine->drum_voices[voice].steps[step] == 0) {
                engine->drum_voices[voice].steps[step] = velocity;
                grid_state_set_led(&engine->grid_state, row, col, COLOR_STEP_ON);
            } else {
                engine->drum_voices[voice].steps[step] = 0;
                grid_state_set_led(&engine->grid_state, row, col, COLOR_STEP_OFF);
            }
            break;
        }

        case QUADRANT_MELODY: {
            // Melody sequencer: vertical = pitch, horizontal = time
            // 4 rows × 4 cols = 16 steps visible
            uint8_t step = local_col + (local_row * 4);
            if (step >= SEQ_STEPS) break;

            // Map row to scale degree (higher row = higher pitch)
            uint8_t scale_degree = (3 - local_row) + (local_col / 4) * 4;
            uint8_t note = engine->melody.root_note +
                          SCALE_INTERVALS[engine->melody.scale][scale_degree % 12] +
                          (scale_degree / 12) * 12;

            if (engine->melody.velocities[step] > 0 && engine->melody.notes[step] == note) {
                engine->melody.notes[step] = 0;
                engine->melody.velocities[step] = 0;
                grid_state_set_led(&engine->grid_state, row, col, COLOR_OFF);
            } else {
                engine->melody.notes[step] = note;
                engine->melody.velocities[step] = velocity;
                grid_state_set_led(&engine->grid_state, row, col, COLOR_MELODY);
                // Immediate preview tap
                queue_note_event(engine, note, velocity, 1, quadrangle_default_gate_frames(engine));
            }
            break;
        }

        case QUADRANT_LIVE: {
            // Live pads: 4×4 = 16 pads
            uint8_t pad_index = local_row * 4 + local_col;
            if (pad_index >= LIVE_PADS) break;

            quadrangle_trigger_live_pad(engine, pad_index, velocity);
            grid_state_set_led(&engine->grid_state, row, col, COLOR_LIVE);
            queue_note_event(engine, engine->live_pads[pad_index].note, velocity, 2, quadrangle_default_gate_frames(engine));
            break;
        }

        case QUADRANT_PARAMS: {
            // Parameter controls: two rows of 2-bit CCs per column
            // Bottom row (rows 0-1): CC 74, 71, 1, 27
            // Top row (rows 2-3):   CC 73, 72, 28, 30
            static const uint8_t bottom_ccs[4] = {74, 71, 1, 27};
            static const uint8_t top_ccs[4] = {73, 72, 28, 30};

            if (local_col >= 4) break;

            uint8_t is_top = (local_row >= 2);
            uint8_t row_base = is_top ? 2 : 0;
            uint8_t bit = (uint8_t)(local_row - row_base);  // 0 or 1
            uint8_t index = is_top ? (local_col + 4) : local_col;  // store state in params[0..7]

            uint8_t state = engine->params[index].value & 0x03;
            state ^= (uint8_t)(1u << bit);
            engine->params[index].value = state;

            uint8_t value = (uint8_t)((state * 127) / 3);
            uint8_t cc = is_top ? top_ccs[local_col] : bottom_ccs[local_col];

            queue_cc_event(engine, cc, value, 1);
            break;
        }
    }
}

void quadrangle_handle_pad_release(QuadrangleEngine *engine, uint8_t row, uint8_t col) {
    // For now, only live pads respond to release
    Quadrant quad;
    uint8_t local_row, local_col;

    if (!grid_state_get_quadrant(row, col, &quad, &local_row, &local_col)) {
        return;
    }

    if (quad == QUADRANT_LIVE) {
        uint8_t pad_index = local_row * 4 + local_col;
        if (pad_index >= LIVE_PADS) return;

        // Note off for momentary/toggle release
        queue_note_event(engine, engine->live_pads[pad_index].note, 0, 2, 1);
        engine->live_pads[pad_index].state = 0;
        grid_state_set_led(&engine->grid_state, row, col, COLOR_OFF);
    }
}

void quadrangle_handle_side_button(QuadrangleEngine *engine, uint8_t index) {
    // Side buttons (0-7):
    // 0-7: Select drum voice for editing
    if (index < MAX_DRUM_VOICES) {
        engine->selected_voice = index;

        // Update side button LEDs
        for (uint8_t i = 0; i < MAX_DRUM_VOICES; i++) {
            uint8_t color = (i == index) ? COLOR_SELECTED : COLOR_DRUMS;
            grid_state_set_side_button(&engine->grid_state, i, 0, color);
        }
    }
}

void quadrangle_handle_top_button(QuadrangleEngine *engine, uint8_t index) {
    // Top buttons (0-8):
    // 0: Play/Stop
    // 1: Record
    // 2-5: Pattern select (A-D)
    // 6: Tap tempo
    // 7: Clear pattern
    // 8: Logo (unused)

    switch (index) {
        case 0:  // Play/Stop
            if (engine->playing) {
                quadrangle_stop(engine);
                grid_state_set_top_button(&engine->grid_state, 0, 0, COLOR_OFF);
            } else {
                quadrangle_start(engine);
                grid_state_set_top_button(&engine->grid_state, 0, 0, COLOR_GREEN_BRIGHT);
            }
            break;

        case 2:
        case 3:
        case 4:
        case 5: {
            // Pattern select
            uint8_t pattern = index - 2;  // 0-3
            quadrangle_set_pattern(engine, pattern);

            // Update pattern button LEDs
            for (uint8_t i = 2; i <= 5; i++) {
                uint8_t color = (i == index) ? COLOR_SELECTED : COLOR_YELLOW_DIM;
                grid_state_set_top_button(&engine->grid_state, i, 0, color);
            }
            break;
        }

        case 7:  // Clear pattern
            quadrangle_clear_pattern(engine);
            grid_state_set_top_button(&engine->grid_state, 7, 0, COLOR_RED_BRIGHT);
            break;
    }
}

// ============================================================================
// Audio Processing (Sequencer Clock)
// ============================================================================

void quadrangle_begin_block(QuadrangleEngine *engine) {
    engine->midi_event_count = 0;
}

void quadrangle_process(QuadrangleEngine *engine, uint32_t n_samples) {
    if (!engine->playing) return;

    for (uint32_t i = 0; i < n_samples; i++) {
        engine->sample_counter++;

        // Check if we've reached next step
        if (engine->sample_counter >= engine->samples_per_step) {
            engine->sample_counter = 0;
            engine->current_step = (engine->current_step + 1) % SEQ_STEPS;

            // Trigger drum voices for this step
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

            // Trigger melody note for this step
            if (engine->melody.active &&
                engine->melody.velocities[engine->current_step] > 0) {
                uint8_t note = engine->melody.notes[engine->current_step];
                uint8_t vel = engine->melody.velocities[engine->current_step];
                queue_note_event(engine, note, vel, 1, gate_frames);
            }

            // Update playhead visualization
            engine->grid_state.step_counter = engine->current_step;
        }
    }
}

// ============================================================================
// LED State Refresh (used by plugin to drive Launchpad/UI)
// ============================================================================

void quadrangle_refresh_grid_state(QuadrangleEngine *engine) {
    // Drum quadrant (rows 4-7, cols 0-3) for selected voice
    uint8_t voice = engine->selected_voice;
    if (voice >= MAX_DRUM_VOICES) voice = 0;
    for (uint8_t step = 0; step < SEQ_STEPS; step++) {
        uint8_t row = step / 4 + 4;  // rows 4-7
        uint8_t col = step % 4;      // cols 0-3
        uint8_t color = engine->drum_voices[voice].steps[step] > 0 ? COLOR_STEP_ON : COLOR_STEP_OFF;
        if (engine->playing && step == engine->current_step) {
            color = COLOR_PLAYHEAD;
        }
        grid_state_set_led(&engine->grid_state, row, col, color);
    }

    // Melody quadrant (rows 4-7, cols 4-7)
    for (uint8_t step = 0; step < SEQ_STEPS; step++) {
        uint8_t row = step / 4 + 4;
        uint8_t col = (step % 4) + 4;
        uint8_t color = engine->melody.velocities[step] > 0 ? COLOR_MELODY : COLOR_OFF;
        if (engine->playing && step == engine->current_step) {
            color = COLOR_PLAYHEAD;
        }
        grid_state_set_led(&engine->grid_state, row, col, color);
    }

    // Live quadrant (rows 0-3, cols 0-3)
    for (uint8_t r = 0; r < 4; r++) {
        for (uint8_t c = 0; c < 4; c++) {
            uint8_t idx = r * 4 + c;
            uint8_t color = (idx < LIVE_PADS && engine->live_pads[idx].state) ? COLOR_LIVE : COLOR_OFF;
            grid_state_set_led(&engine->grid_state, r, c, color);
        }
    }

    // Params quadrant (rows 0-3, cols 4-7) - 2-bit CC LEDs
    for (uint8_t c = 0; c < 4; c++) {
        uint8_t bottom_state = engine->params[c].value & 0x03;
        uint8_t top_state = engine->params[c + 4].value & 0x03;

        // bottom row bits (rows 0-1)
        for (uint8_t b = 0; b < 2; b++) {
            uint8_t on = (bottom_state & (1u << b)) ? 1 : 0;
            grid_state_set_led(&engine->grid_state, b, c + 4, on ? COLOR_PARAMS : COLOR_OFF);
        }
        // top row bits (rows 2-3)
        for (uint8_t b = 0; b < 2; b++) {
            uint8_t on = (top_state & (1u << b)) ? 1 : 0;
            grid_state_set_led(&engine->grid_state, b + 2, c + 4, on ? COLOR_PARAMS : COLOR_OFF);
        }
    }

    // Side buttons (drum voice select)
    for (uint8_t i = 0; i < MAX_DRUM_VOICES && i < GRID_HEIGHT; ++i) {
        uint8_t color = (i == engine->selected_voice) ? COLOR_SELECTED : COLOR_DRUMS;
        grid_state_set_side_button(&engine->grid_state, i, 0, color);
    }

    // Top buttons: play state + pattern A-D (buttons 2-5)
    grid_state_set_top_button(&engine->grid_state, 0, 0,
                              engine->playing ? COLOR_GREEN_BRIGHT : COLOR_OFF);
    for (uint8_t i = 2; i <= 5; i++) {
        uint8_t pattern = i - 2;
        uint8_t color = (pattern == engine->grid_state.pattern) ? COLOR_SELECTED : COLOR_YELLOW_DIM;
        grid_state_set_top_button(&engine->grid_state, i, 0, color);
    }
    // Clear button (7) dim red
    grid_state_set_top_button(&engine->grid_state, 7, 0, COLOR_RED_DIM);
}

// ============================================================================
// LED Update
// ============================================================================

void quadrangle_update_leds(QuadrangleEngine *engine, MidiMessage *msg) {
    // Update entire grid based on current state

    // Drum quadrant: show current voice pattern
    uint8_t voice = engine->selected_voice;
    for (uint8_t step = 0; step < SEQ_STEPS; step++) {
        uint8_t row = step / 4 + 4;  // Rows 4-7
        uint8_t col = step % 4;
        uint8_t color = COLOR_STEP_OFF;

        if (engine->drum_voices[voice].steps[step] > 0) {
            color = (step == engine->current_step && engine->playing) ?
                    COLOR_PLAYHEAD : COLOR_STEP_ON;
        }

        grid_state_set_led(&engine->grid_state, row, col, color);
    }

    // Send bulk update
    midi_update_grid(msg, &engine->grid_state);
}

// ============================================================================
// Pattern Management
// ============================================================================

void quadrangle_set_pattern(QuadrangleEngine *engine, uint8_t pattern) {
    engine->grid_state.pattern = pattern;
    // TODO: Load pattern from storage
}

void quadrangle_copy_pattern(QuadrangleEngine *engine, uint8_t src, uint8_t dst) {
    // TODO: Implement pattern copy
    (void)engine;
    (void)src;
    (void)dst;
}

void quadrangle_clear_pattern(QuadrangleEngine *engine) {
    // Clear current pattern
    for (uint8_t v = 0; v < MAX_DRUM_VOICES; v++) {
        memset(engine->drum_voices[v].steps, 0, SEQ_STEPS);
    }
    memset(engine->melody.notes, 0, SEQ_STEPS);
    memset(engine->melody.velocities, 0, SEQ_STEPS);
}

// ============================================================================
// Quadrant-Specific Functions
// ============================================================================

void quadrangle_set_drum_step(QuadrangleEngine *engine, uint8_t voice, uint8_t step, uint8_t velocity) {
    if (voice >= MAX_DRUM_VOICES || step >= SEQ_STEPS) return;
    engine->drum_voices[voice].steps[step] = velocity;
}

void quadrangle_set_melody_note(QuadrangleEngine *engine, uint8_t step, uint8_t note, uint8_t velocity) {
    if (step >= SEQ_STEPS) return;
    engine->melody.notes[step] = note;
    engine->melody.velocities[step] = velocity;
}

void quadrangle_trigger_live_pad(QuadrangleEngine *engine, uint8_t pad_index, uint8_t velocity) {
    if (pad_index >= LIVE_PADS) return;

    LivePad *pad = &engine->live_pads[pad_index];
    pad->velocity = velocity;

    switch (pad->mode) {
        case 0:  // Oneshot
            pad->state = 1;
            // TODO: Trigger MIDI note
            break;

        case 1:  // Toggle
            pad->state = !pad->state;
            break;

        case 2:  // Momentary
            pad->state = 1;
            break;
    }
}

void quadrangle_set_parameter(QuadrangleEngine *engine, uint8_t param_index, uint8_t value) {
    if (param_index >= PARAM_CONTROLS) return;
    engine->params[param_index].value = value;
    // TODO: Send parameter to DSP engine
}
