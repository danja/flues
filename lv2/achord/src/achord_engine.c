#include "../include/achord_engine.h"
#include "../include/chord_map.h"
#include <math.h>
#include <string.h>

static uint8_t clamp_u8(int v, int lo, int hi) {
    if (v < lo) return (uint8_t)lo;
    if (v > hi) return (uint8_t)hi;
    return (uint8_t)v;
}

static int8_t clamp_i8(int v, int lo, int hi) {
    if (v < lo) return (int8_t)lo;
    if (v > hi) return (int8_t)hi;
    return (int8_t)v;
}

static void queue_raw_event(AchordEngine *engine,
                            uint32_t frame,
                            uint8_t status,
                            uint8_t data1,
                            uint8_t data2) {
    if (engine->midi_event_count >= ACHORD_MAX_MIDI_EVENTS) {
        return;
    }
    engine->midi_events[engine->midi_event_count++] =
        (MidiOutEvent){frame, 3, {status, data1, data2}};
}

static void retain_note(AchordEngine *engine, uint32_t frame, uint8_t note, uint8_t velocity) {
    if (note > 127) return;
    if (engine->note_refcount[note] == 0) {
        queue_raw_event(engine, frame, 0x90, note, velocity ? velocity : 1);
    }
    if (engine->note_refcount[note] < 255) {
        engine->note_refcount[note]++;
    }
}

static void release_note(AchordEngine *engine, uint32_t frame, uint8_t note) {
    if (note > 127) return;
    if (engine->note_refcount[note] == 0) return;
    engine->note_refcount[note]--;
    if (engine->note_refcount[note] == 0) {
        queue_raw_event(engine, frame, 0x80, note, 0);
    }
}

static void schedule_note_off(AchordEngine *engine, uint8_t note, uint32_t due_frame) {
    for (uint16_t i = 0; i < ACHORD_MAX_PENDING_NOTEOFFS; ++i) {
        AchordPendingNoteOff *p = &engine->pending_note_offs[i];
        if (!p->active) {
            p->active = 1;
            p->note = note;
            p->frames_left = due_frame;
            return;
        }
    }
}

static void process_pending_note_offs_at_frame(AchordEngine *engine, uint32_t frame) {
    for (uint16_t i = 0; i < ACHORD_MAX_PENDING_NOTEOFFS; ++i) {
        AchordPendingNoteOff *p = &engine->pending_note_offs[i];
        if (!p->active) continue;
        if (p->frames_left == frame) {
            release_note(engine, frame, p->note);
            p->active = 0;
        }
    }
}

static void normalize_pending_note_offs(AchordEngine *engine, uint32_t n_samples) {
    for (uint16_t i = 0; i < ACHORD_MAX_PENDING_NOTEOFFS; ++i) {
        AchordPendingNoteOff *p = &engine->pending_note_offs[i];
        if (!p->active) continue;
        if (p->frames_left >= n_samples) {
            p->frames_left -= n_samples;
        }
    }
}

static uint8_t note_velocity_for_index(const AchordEngine *engine,
                                       const AchordVoicing *voicing,
                                       uint8_t note_index,
                                       uint8_t base_velocity) {
    int velocity = base_velocity ? base_velocity : 96;
    if (engine->config.accent_enabled) {
        velocity += 18;
    }
    if (engine->config.bass_enabled && note_index < voicing->note_count && note_index == 0) {
        velocity -= 12;
    }
    return clamp_u8(velocity, 1, 127);
}

static void clear_pad_runtime(AchordPadState *pad) {
    pad->logic_active = 0;
    pad->sounding = 0;
    pad->current_note_count = 0;
    memset(pad->current_notes, 0, sizeof(pad->current_notes));
    memset(pad->current_note_on, 0, sizeof(pad->current_note_on));
    pad->pending_quantized = 0;
    pad->pending_step = 0;
    pad->strum_pending = 0;
    pad->strum_note_count = 0;
    memset(pad->strum_notes, 0, sizeof(pad->strum_notes));
    memset(pad->strum_velocities, 0, sizeof(pad->strum_velocities));
    pad->strum_next_frame = 0;
    pad->strum_spacing_frames = 0;
    pad->strum_index = 0;
    pad->strum_direction = 0;
    pad->last_repeat_step = 0;
}

static void reset_runtime(AchordEngine *engine) {
    memset(engine->pads, 0, sizeof(engine->pads));
    memset(engine->note_refcount, 0, sizeof(engine->note_refcount));
    memset(engine->pending_note_offs, 0, sizeof(engine->pending_note_offs));
    memset(engine->last_voicing_notes, 0, sizeof(engine->last_voicing_notes));
    engine->last_voicing_count = 0;
    engine->order_counter = 0;
    engine->midi_event_count = 0;
    engine->current_step16 = 0;
    engine->active_chord_count = 0;
    grid_state_clear(&engine->grid_state);
}

static double frame_for_boundary(double abs_steps_start,
                                 double abs_steps_end,
                                 uint32_t nframes,
                                 int64_t boundary_step) {
    const double rel_steps = (double)boundary_step - abs_steps_start;
    const double denom = abs_steps_end - abs_steps_start + 1e-12;
    double t = rel_steps / denom;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    int frame = (int)floor(t * (double)nframes);
    if (frame < 0) frame = 0;
    if (frame >= (int)nframes) frame = (int)nframes - 1;
    return (double)frame;
}

static uint8_t current_base_velocity(const AchordPadState *pad) {
    return pad->velocity ? pad->velocity : 96;
}

static void build_pad_voicing(const AchordEngine *engine,
                              uint8_t row,
                              uint8_t col,
                              AchordVoicing *voicing) {
    AchordVoicingRequest request;
    memset(&request, 0, sizeof(request));
    request.tonic_note = engine->config.tonic_note;
    request.bank_offset = engine->config.bank_offset;
    request.scale_index = engine->config.scale_index;
    request.row = row;
    request.col = col;
    request.register_mode = engine->config.register_mode;
    request.bass_enabled = engine->config.bass_enabled;
    request.add9_enabled = engine->config.add9_enabled;
    request.sus_mode = engine->config.sus_mode;
    request.inversion_offset = engine->config.inversion_offset;
    request.spread_mode = engine->config.spread_mode;
    request.voice_lead_enabled = engine->config.voice_lead_enabled;

    achord_build_voicing(&request,
                         engine->last_voicing_count ? engine->last_voicing_notes : NULL,
                         engine->last_voicing_count,
                         voicing);
}

static void remember_voicing(AchordEngine *engine, const AchordVoicing *voicing) {
    engine->last_voicing_count = voicing->note_count;
    memcpy(engine->last_voicing_notes, voicing->notes, voicing->note_count);
}

static void deactivate_pad(AchordEngine *engine, uint8_t row, uint8_t col, uint32_t frame) {
    AchordPadState *pad = &engine->pads[row][col];
    for (uint8_t i = 0; i < pad->current_note_count; ++i) {
        if (pad->current_note_on[i]) {
            release_note(engine, frame, pad->current_notes[i]);
        }
    }
    clear_pad_runtime(pad);
}

static void emit_strum_note(AchordEngine *engine,
                            AchordPadState *pad,
                            uint32_t frame) {
    if (!pad->strum_pending || pad->strum_index >= pad->strum_note_count) return;

    const uint8_t note = pad->strum_notes[pad->strum_index];
    const uint8_t velocity = pad->strum_velocities[pad->strum_index];
    retain_note(engine, frame, note, velocity);

    if (pad->strum_index < pad->current_note_count) {
        pad->current_note_on[pad->strum_index] = 1;
    }
    pad->strum_index++;
    pad->sounding = 1;

    if (pad->strum_index >= pad->strum_note_count) {
        pad->strum_pending = 0;
        pad->strum_index = 0;
        pad->strum_next_frame = 0;
        return;
    }

    pad->strum_next_frame = frame + pad->strum_spacing_frames;
}

static void schedule_strum(AchordEngine *engine,
                           AchordPadState *pad,
                           const AchordVoicing *voicing,
                           uint8_t velocity,
                           uint32_t start_frame,
                           uint16_t spacing_frames,
                           uint8_t direction) {
    memset(pad->current_note_on, 0, sizeof(pad->current_note_on));
    pad->current_note_count = voicing->note_count;
    pad->strum_note_count = voicing->note_count;
    pad->strum_spacing_frames = spacing_frames;
    pad->strum_next_frame = start_frame;
    pad->strum_pending = voicing->note_count > 0 ? 1 : 0;
    pad->strum_index = 0;
    pad->strum_direction = direction;

    if (direction == 0) {
        for (uint8_t i = 0; i < voicing->note_count; ++i) {
            pad->current_notes[i] = voicing->notes[i];
            pad->strum_notes[i] = voicing->notes[i];
            pad->strum_velocities[i] = note_velocity_for_index(engine, voicing, i, velocity);
        }
    } else {
        for (uint8_t i = 0; i < voicing->note_count; ++i) {
            const uint8_t src = (uint8_t)(voicing->note_count - 1 - i);
            pad->current_notes[i] = voicing->notes[src];
            pad->strum_notes[i] = voicing->notes[src];
            pad->strum_velocities[i] =
                note_velocity_for_index(engine, voicing, src, velocity);
        }
    }
}

static void activate_sustained_pad(AchordEngine *engine,
                                   uint8_t row,
                                   uint8_t col,
                                   uint32_t frame) {
    AchordPadState *pad = &engine->pads[row][col];
    AchordVoicing voicing;
    build_pad_voicing(engine, row, col, &voicing);
    if (voicing.note_count == 0) return;

    remember_voicing(engine, &voicing);
    pad->current_note_count = voicing.note_count;
    memcpy(pad->current_notes, voicing.notes, voicing.note_count);
    memset(pad->current_note_on, 0, sizeof(pad->current_note_on));
    pad->sounding = 0;

    const uint8_t velocity = current_base_velocity(pad);

    if (engine->config.trigger_mode == ACHORD_TRIGGER_STRUM_DOWN) {
        schedule_strum(engine, pad, &voicing, velocity, frame, 180, 0);
        return;
    }
    if (engine->config.trigger_mode == ACHORD_TRIGGER_STRUM_UP) {
        schedule_strum(engine, pad, &voicing, velocity, frame, 180, 1);
        return;
    }
    if (engine->config.accent_enabled) {
        schedule_strum(engine, pad, &voicing, velocity, frame, 96, 0);
        return;
    }

    for (uint8_t i = 0; i < voicing.note_count; ++i) {
        const uint8_t vel = note_velocity_for_index(engine, &voicing, i, velocity);
        retain_note(engine, frame, voicing.notes[i], vel);
        pad->current_note_on[i] = 1;
    }
    pad->sounding = 1;
}

static uint32_t repeat_gate_frames(const AchordEngine *engine) {
    double bpm = engine->bpm ? (double)engine->bpm : 120.0;
    if (bpm < 1.0) bpm = 120.0;
    const double frames_per_16th = (60.0 * engine->sample_rate) / (bpm * 4.0);
    uint32_t gate = (uint32_t)floor(frames_per_16th * 1.2);
    if (gate < 1) gate = 1;
    return gate;
}

static void trigger_repeat_pulse(AchordEngine *engine,
                                 uint8_t row,
                                 uint8_t col,
                                 uint32_t frame) {
    AchordPadState *pad = &engine->pads[row][col];
    AchordVoicing voicing;
    build_pad_voicing(engine, row, col, &voicing);
    if (voicing.note_count == 0) return;

    remember_voicing(engine, &voicing);
    const uint8_t velocity = current_base_velocity(pad);
    const uint32_t gate = repeat_gate_frames(engine);

    for (uint8_t i = 0; i < voicing.note_count; ++i) {
        const uint8_t vel = note_velocity_for_index(engine, &voicing, i, velocity);
        retain_note(engine, frame, voicing.notes[i], vel);
        schedule_note_off(engine, voicing.notes[i], frame + gate);
    }
}

static int64_t next_quantize_step(const AchordEngine *engine, uint32_t frame) {
    const double abs_steps = engine->block_abs_steps_start +
                             (double)frame * engine->block_steps_per_frame;
    return (int64_t)floor(abs_steps + 1e-9) + 1;
}

static int64_t next_repeat_step(const AchordEngine *engine, uint32_t frame) {
    int64_t step = next_quantize_step(engine, frame);
    if (step & 1) step++;
    return step;
}

static void activate_pad(AchordEngine *engine, uint8_t row, uint8_t col, uint32_t frame) {
    AchordPadState *pad = &engine->pads[row][col];
    clear_pad_runtime(pad);
    pad->logic_active = 1;
    pad->velocity = pad->velocity ? pad->velocity : 96;

    if (engine->config.trigger_mode == ACHORD_TRIGGER_DIRECT ||
        engine->config.trigger_mode == ACHORD_TRIGGER_STRUM_DOWN ||
        engine->config.trigger_mode == ACHORD_TRIGGER_STRUM_UP ||
        !engine->host_time_valid || !engine->host_playing) {
        activate_sustained_pad(engine, row, col, frame);
        return;
    }

    pad->pending_quantized = 1;
    if (engine->config.trigger_mode == ACHORD_TRIGGER_REPEAT) {
        pad->pending_step = (uint64_t)next_repeat_step(engine, frame);
    } else {
        pad->pending_step = (uint64_t)next_quantize_step(engine, frame);
    }
}

static void count_active_chords(AchordEngine *engine) {
    uint8_t count = 0;
    for (uint8_t r = 0; r < GRID_HEIGHT; ++r) {
        for (uint8_t c = 0; c < GRID_WIDTH; ++c) {
            if (engine->pads[r][c].logic_active) count++;
        }
    }
    engine->active_chord_count = count;
}

static void reconcile_logical_pads(AchordEngine *engine, uint32_t frame) {
    uint8_t desired[GRID_HEIGHT][GRID_WIDTH];
    memset(desired, 0, sizeof(desired));

    if (engine->config.hold_mode == ACHORD_HOLD_MOMENTARY) {
        uint32_t best_order = 0;
        uint8_t best_r = 0;
        uint8_t best_c = 0;
        int found = 0;
        for (uint8_t r = 0; r < GRID_HEIGHT; ++r) {
            for (uint8_t c = 0; c < GRID_WIDTH; ++c) {
                const AchordPadState *pad = &engine->pads[r][c];
                if (pad->is_down && (!found || pad->order > best_order)) {
                    best_order = pad->order;
                    best_r = r;
                    best_c = c;
                    found = 1;
                }
            }
        }
        if (found) desired[best_r][best_c] = 1;
    } else if (engine->config.hold_mode == ACHORD_HOLD_LATCH) {
        uint32_t best_order = 0;
        uint8_t best_r = 0;
        uint8_t best_c = 0;
        int found = 0;
        for (uint8_t r = 0; r < GRID_HEIGHT; ++r) {
            for (uint8_t c = 0; c < GRID_WIDTH; ++c) {
                const AchordPadState *pad = &engine->pads[r][c];
                if (pad->is_latched && (!found || pad->order > best_order)) {
                    best_order = pad->order;
                    best_r = r;
                    best_c = c;
                    found = 1;
                }
            }
        }
        if (found) desired[best_r][best_c] = 1;
    } else {
        typedef struct {
            uint8_t row;
            uint8_t col;
            uint32_t order;
        } Candidate;
        Candidate candidates[GRID_SIZE];
        uint8_t cand_count = 0;
        for (uint8_t r = 0; r < GRID_HEIGHT; ++r) {
            for (uint8_t c = 0; c < GRID_WIDTH; ++c) {
                const AchordPadState *pad = &engine->pads[r][c];
                if (pad->is_latched && cand_count < GRID_SIZE) {
                    candidates[cand_count++] = (Candidate){r, c, pad->order};
                }
            }
        }
        for (uint8_t pass = 0; pass < cand_count; ++pass) {
            for (uint8_t i = 0; i + 1 < cand_count; ++i) {
                if (candidates[i].order < candidates[i + 1].order) {
                    Candidate tmp = candidates[i];
                    candidates[i] = candidates[i + 1];
                    candidates[i + 1] = tmp;
                }
            }
        }
        for (uint8_t i = 0; i < cand_count && i < 4; ++i) {
            desired[candidates[i].row][candidates[i].col] = 1;
        }
    }

    for (uint8_t r = 0; r < GRID_HEIGHT; ++r) {
        for (uint8_t c = 0; c < GRID_WIDTH; ++c) {
            AchordPadState *pad = &engine->pads[r][c];
            if (desired[r][c] && !pad->logic_active) {
                activate_pad(engine, r, c, frame);
            } else if (!desired[r][c] && pad->logic_active) {
                deactivate_pad(engine, r, c, frame);
            }
        }
    }

    count_active_chords(engine);
}

static void clear_all_latches(AchordEngine *engine) {
    for (uint8_t r = 0; r < GRID_HEIGHT; ++r) {
        for (uint8_t c = 0; c < GRID_WIDTH; ++c) {
            engine->pads[r][c].is_latched = 0;
        }
    }
}

void achord_init(AchordEngine *engine, float sample_rate) {
    memset(engine, 0, sizeof(*engine));
    engine->sample_rate = sample_rate;
    engine->config.tonic_note = 48;
    engine->config.scale_index = ACHORD_SCALE_MAJOR;
    engine->config.register_mode = ACHORD_REGISTER_8;
    engine->config.trigger_mode = ACHORD_TRIGGER_DIRECT;
    engine->config.hold_mode = ACHORD_HOLD_MOMENTARY;
    engine->config.bass_enabled = 0;
    engine->config.add9_enabled = 0;
    engine->config.sus_mode = ACHORD_SUS_OFF;
    engine->config.inversion_offset = 0;
    engine->config.spread_mode = ACHORD_SPREAD_CLOSE;
    engine->config.accent_enabled = 0;
    engine->config.voice_lead_enabled = 1;
    engine->bpm = 120;
    grid_state_init(&engine->grid_state);
    achord_reset(engine);
}

void achord_reset(AchordEngine *engine) {
    reset_runtime(engine);
    engine->host_time_valid = 0;
    engine->host_playing = 0;
    engine->host_bar = 0.0;
    engine->host_bar_beat = 0.0;
    engine->beats_per_bar = 4.0;
    engine->grid_state.tempo_bpm = (uint8_t)engine->bpm;
    engine->grid_state.playing = 0;
}

void achord_panic(AchordEngine *engine, uint32_t frame) {
    for (uint16_t note = 0; note < 128; ++note) {
        if (engine->note_refcount[note]) {
            queue_raw_event(engine, frame, 0x80, (uint8_t)note, 0);
            engine->note_refcount[note] = 0;
        }
    }
    queue_raw_event(engine, frame, 0xB0, 123, 0);
    memset(engine->pending_note_offs, 0, sizeof(engine->pending_note_offs));
    memset(engine->last_voicing_notes, 0, sizeof(engine->last_voicing_notes));
    engine->last_voicing_count = 0;
    for (uint8_t r = 0; r < GRID_HEIGHT; ++r) {
        for (uint8_t c = 0; c < GRID_WIDTH; ++c) {
            engine->pads[r][c].is_down = 0;
            engine->pads[r][c].is_latched = 0;
            clear_pad_runtime(&engine->pads[r][c]);
        }
    }
    count_active_chords(engine);
}

void achord_begin_block(AchordEngine *engine,
                        uint32_t n_samples,
                        uint8_t host_time_valid,
                        uint8_t host_playing,
                        double bar,
                        double bar_beat,
                        double beats_per_bar,
                        double bpm) {
    engine->midi_event_count = 0;
    engine->block_n_samples = n_samples;
    engine->host_time_valid = host_time_valid;
    engine->host_playing = host_playing;
    engine->host_bar = bar;
    engine->host_bar_beat = bar_beat;
    engine->beats_per_bar = beats_per_bar > 0.0 ? beats_per_bar : 4.0;
    if (bpm > 0.0) {
        engine->bpm = (uint16_t)clamp_u8((int)lrint(bpm), 30, 240);
    }
    engine->grid_state.tempo_bpm = (uint8_t)engine->bpm;
    engine->grid_state.playing = host_playing ? 1 : 0;

    if (host_time_valid && host_playing && bpm > 0.0 && engine->beats_per_bar > 0.0) {
        const double abs_beats = bar * engine->beats_per_bar + bar_beat;
        engine->block_abs_steps_start = abs_beats * 4.0;
        engine->block_steps_per_frame = (bpm / (60.0 * engine->sample_rate)) * 4.0;
        engine->block_abs_steps_end =
            engine->block_abs_steps_start + engine->block_steps_per_frame * (double)n_samples;
        engine->current_step16 = (uint8_t)(((int64_t)floor(engine->block_abs_steps_start + 1e-9)) & 15);
    } else {
        engine->block_abs_steps_start = 0.0;
        engine->block_abs_steps_end = 0.0;
        engine->block_steps_per_frame = 0.0;
        engine->current_step16 = 0;
    }
}

void achord_handle_pad_press(AchordEngine *engine,
                             uint8_t row,
                             uint8_t col,
                             uint8_t velocity,
                             uint32_t frame) {
    if (row >= GRID_HEIGHT || col >= GRID_WIDTH) return;
    AchordPadState *pad = &engine->pads[row][col];
    pad->velocity = velocity ? velocity : 96;
    pad->order = ++engine->order_counter;
    pad->is_down = 1;

    if (engine->config.hold_mode == ACHORD_HOLD_MOMENTARY) {
        reconcile_logical_pads(engine, frame);
        return;
    }

    if (engine->config.hold_mode == ACHORD_HOLD_LATCH) {
        if (pad->is_latched) {
            pad->is_latched = 0;
        } else {
            clear_all_latches(engine);
            pad->is_latched = 1;
        }
    } else {
        if (pad->is_latched) {
            pad->is_latched = 0;
        } else {
            uint8_t active_latched = 0;
            for (uint8_t r = 0; r < GRID_HEIGHT; ++r) {
                for (uint8_t c = 0; c < GRID_WIDTH; ++c) {
                    if (engine->pads[r][c].is_latched) active_latched++;
                }
            }
            if (active_latched >= 4) {
                uint32_t oldest_order = 0xFFFFFFFFu;
                uint8_t old_r = 0;
                uint8_t old_c = 0;
                int found = 0;
                for (uint8_t r = 0; r < GRID_HEIGHT; ++r) {
                    for (uint8_t c = 0; c < GRID_WIDTH; ++c) {
                        if (engine->pads[r][c].is_latched &&
                            (!found || engine->pads[r][c].order < oldest_order)) {
                            oldest_order = engine->pads[r][c].order;
                            old_r = r;
                            old_c = c;
                            found = 1;
                        }
                    }
                }
                if (found) {
                    engine->pads[old_r][old_c].is_latched = 0;
                }
            }
            pad->is_latched = 1;
        }
    }

    reconcile_logical_pads(engine, frame);
}

void achord_handle_pad_release(AchordEngine *engine,
                               uint8_t row,
                               uint8_t col,
                               uint32_t frame) {
    if (row >= GRID_HEIGHT || col >= GRID_WIDTH) return;
    engine->pads[row][col].is_down = 0;
    if (engine->config.hold_mode == ACHORD_HOLD_MOMENTARY) {
        reconcile_logical_pads(engine, frame);
    }
}

void achord_handle_side_button(AchordEngine *engine, uint8_t index, uint32_t frame) {
    (void)frame;
    switch (index) {
        case 0:
            engine->config.bass_enabled ^= 1;
            break;
        case 1:
            engine->config.add9_enabled ^= 1;
            break;
        case 2:
            engine->config.sus_mode = (uint8_t)((engine->config.sus_mode + 1) % 3);
            break;
        case 3:
            engine->config.inversion_offset = clamp_i8(engine->config.inversion_offset - 1, -3, 3);
            break;
        case 4:
            engine->config.inversion_offset = clamp_i8(engine->config.inversion_offset + 1, -3, 3);
            break;
        case 5:
            engine->config.spread_mode = (uint8_t)((engine->config.spread_mode + 1) % 3);
            break;
        case 6:
            engine->config.accent_enabled ^= 1;
            break;
        case 7:
            engine->config.voice_lead_enabled ^= 1;
            break;
        default:
            break;
    }
}

void achord_handle_top_button(AchordEngine *engine, uint8_t index, uint32_t frame) {
    switch (index) {
        case 0:
            engine->config.bank_offset = clamp_i8(engine->config.bank_offset - 1, -12, 12);
            break;
        case 1:
            engine->config.bank_offset = clamp_i8(engine->config.bank_offset + 1, -12, 12);
            break;
        case 2:
            engine->config.tonic_note = clamp_u8((int)engine->config.tonic_note - 12, 24, 84);
            break;
        case 3:
            engine->config.tonic_note = clamp_u8((int)engine->config.tonic_note + 12, 24, 84);
            break;
        case 4:
            engine->config.scale_index = (uint8_t)((engine->config.scale_index + 1) % ACHORD_SCALE_COUNT);
            break;
        case 5:
            engine->config.register_mode = (uint8_t)((engine->config.register_mode + 1) % 4);
            break;
        case 6:
            engine->config.trigger_mode = (uint8_t)((engine->config.trigger_mode + 1) % 5);
            break;
        case 7: {
            const uint8_t prev_hold = engine->config.hold_mode;
            engine->config.hold_mode = (uint8_t)((engine->config.hold_mode + 1) % 3);
            if (engine->config.hold_mode == ACHORD_HOLD_MOMENTARY) {
                clear_all_latches(engine);
            } else if (prev_hold == ACHORD_HOLD_MOMENTARY) {
                uint32_t best_order = 0;
                uint8_t best_r = 0;
                uint8_t best_c = 0;
                int found = 0;
                for (uint8_t r = 0; r < GRID_HEIGHT; ++r) {
                    for (uint8_t c = 0; c < GRID_WIDTH; ++c) {
                        const AchordPadState *pad = &engine->pads[r][c];
                        if (pad->is_down && (!found || pad->order > best_order)) {
                            best_order = pad->order;
                            best_r = r;
                            best_c = c;
                            found = 1;
                        }
                    }
                }
                clear_all_latches(engine);
                if (found) {
                    engine->pads[best_r][best_c].is_latched = 1;
                }
            }
            reconcile_logical_pads(engine, frame);
            break;
        }
        case 8:
            achord_panic(engine, frame);
            break;
        default:
            break;
    }
}

static uint8_t base_row_dim_color(uint8_t row) {
    static const uint8_t dims[8] = {
        COLOR_ORANGE_DIM, COLOR_GRAY_MED, COLOR_GREEN_DIM, COLOR_BLUE_DIM,
        COLOR_RED_DIM, COLOR_PURPLE_DIM, COLOR_CYAN_DIM, COLOR_BLUE_DIM
    };
    return dims[row & 7];
}

static uint8_t base_row_bright_color(uint8_t row) {
    static const uint8_t brights[8] = {
        COLOR_ORANGE_BRIGHT, COLOR_WHITE, COLOR_GREEN_BRIGHT, COLOR_BLUE_BRIGHT,
        COLOR_RED_BRIGHT, COLOR_PURPLE_BRIGHT, COLOR_CYAN_BRIGHT, COLOR_CYAN_BRIGHT
    };
    return brights[row & 7];
}

void achord_refresh_grid_state(AchordEngine *engine) {
    const uint8_t tonic_pc = (uint8_t)(engine->config.tonic_note % 12);
    grid_state_clear(&engine->grid_state);
    engine->grid_state.tempo_bpm = (uint8_t)engine->bpm;
    engine->grid_state.playing = engine->host_playing ? 1 : 0;
    engine->grid_state.step_counter = engine->current_step16;

    for (uint8_t r = 0; r < GRID_HEIGHT; ++r) {
        for (uint8_t c = 0; c < GRID_WIDTH; ++c) {
            const AchordPadState *pad = &engine->pads[r][c];
            const uint8_t root_pc = achord_column_root_pc(engine->config.tonic_note,
                                                          engine->config.bank_offset,
                                                          c);
            const int in_scale = achord_scale_contains_pc(engine->config.scale_index,
                                                          tonic_pc,
                                                          root_pc);
            uint8_t color = in_scale ? base_row_bright_color(r) : base_row_dim_color(r);
            uint8_t state = CELL_EMPTY;
            if (c == 3) {
                color = base_row_bright_color(r);
            }
            if (pad->pending_quantized) {
                state |= CELL_SELECTED;
                color = COLOR_WHITE;
            }
            if (pad->logic_active || pad->is_latched || pad->is_down) {
                state |= CELL_PLAYING;
                color = base_row_bright_color(r);
            }
            if (pad->logic_active) {
                state |= CELL_ARMED;
            }
            grid_state_set_cell(&engine->grid_state, r, c, state, 0, color);
        }
    }

    grid_state_set_side_button(&engine->grid_state, 0, 0,
                               engine->config.bass_enabled ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_DIM);
    grid_state_set_side_button(&engine->grid_state, 1, 0,
                               engine->config.add9_enabled ? COLOR_CYAN_BRIGHT : COLOR_CYAN_DIM);
    grid_state_set_side_button(&engine->grid_state, 2, 0,
                               engine->config.sus_mode == ACHORD_SUS_OFF ? COLOR_GRAY_DIM :
                               engine->config.sus_mode == ACHORD_SUS_2 ? COLOR_YELLOW :
                               COLOR_YELLOW_BRIGHT);
    grid_state_set_side_button(&engine->grid_state, 3, 0,
                               engine->config.inversion_offset < 0 ? COLOR_BLUE_BRIGHT : COLOR_BLUE_DIM);
    grid_state_set_side_button(&engine->grid_state, 4, 0,
                               engine->config.inversion_offset > 0 ? COLOR_BLUE_BRIGHT : COLOR_BLUE_DIM);
    grid_state_set_side_button(&engine->grid_state, 5, 0,
                               engine->config.spread_mode == ACHORD_SPREAD_CLOSE ? COLOR_PURPLE_DIM :
                               engine->config.spread_mode == ACHORD_SPREAD_OPEN ? COLOR_PURPLE :
                               COLOR_PURPLE_BRIGHT);
    grid_state_set_side_button(&engine->grid_state, 6, 0,
                               engine->config.accent_enabled ? COLOR_RED_BRIGHT : COLOR_RED_DIM);
    grid_state_set_side_button(&engine->grid_state, 7, 0,
                               engine->config.voice_lead_enabled ? COLOR_GREEN_BRIGHT : COLOR_GREEN_DIM);

    grid_state_set_top_button(&engine->grid_state, 0, 0,
                              engine->config.bank_offset < 0 ? COLOR_CYAN_BRIGHT : COLOR_CYAN_DIM);
    grid_state_set_top_button(&engine->grid_state, 1, 0,
                              engine->config.bank_offset > 0 ? COLOR_CYAN_BRIGHT : COLOR_CYAN_DIM);
    grid_state_set_top_button(&engine->grid_state, 2, 0,
                              engine->config.tonic_note < 48 ? COLOR_BLUE_BRIGHT : COLOR_BLUE_DIM);
    grid_state_set_top_button(&engine->grid_state, 3, 0,
                              engine->config.tonic_note > 48 ? COLOR_BLUE_BRIGHT : COLOR_BLUE_DIM);
    grid_state_set_top_button(&engine->grid_state, 4, 0,
                              COLOR_PURPLE_DIM + (engine->config.scale_index ? 5 : 0));
    grid_state_set_top_button(&engine->grid_state, 5, 0,
                              COLOR_ORANGE_DIM + (engine->config.register_mode ? 2 : 0));
    grid_state_set_top_button(&engine->grid_state, 6, 0,
                              engine->config.trigger_mode == ACHORD_TRIGGER_REPEAT ? COLOR_GREEN_BRIGHT :
                              engine->config.trigger_mode == ACHORD_TRIGGER_QUANTIZED ? COLOR_GREEN :
                              COLOR_GREEN_DIM);
    grid_state_set_top_button(&engine->grid_state, 7, 0,
                              engine->config.hold_mode == ACHORD_HOLD_STACK ? COLOR_YELLOW_BRIGHT :
                              engine->config.hold_mode == ACHORD_HOLD_LATCH ? COLOR_YELLOW :
                              COLOR_YELLOW_DIM);
    grid_state_set_top_button(&engine->grid_state, 8, 0, COLOR_RED_BRIGHT);
}

void achord_process(AchordEngine *engine, uint32_t n_samples) {
    int64_t boundary_step = 0;
    int64_t boundary_end = -1;
    uint32_t boundary_frame = 0;
    int have_boundary = 0;

    if (engine->host_time_valid && engine->host_playing && engine->block_steps_per_frame > 0.0) {
        boundary_step = (int64_t)floor(engine->block_abs_steps_start + 1e-9) + 1;
        boundary_end = (int64_t)floor(engine->block_abs_steps_end + 1e-9);
        if (boundary_step <= boundary_end) {
            boundary_frame = (uint32_t)frame_for_boundary(engine->block_abs_steps_start,
                                                          engine->block_abs_steps_end,
                                                          n_samples,
                                                          boundary_step);
            have_boundary = 1;
        }
    }

    for (uint32_t frame = 0; frame < n_samples; ++frame) {
        process_pending_note_offs_at_frame(engine, frame);

        for (uint8_t r = 0; r < GRID_HEIGHT; ++r) {
            for (uint8_t c = 0; c < GRID_WIDTH; ++c) {
                AchordPadState *pad = &engine->pads[r][c];
                if (pad->strum_pending && pad->strum_next_frame == frame) {
                    emit_strum_note(engine, pad, frame);
                }
            }
        }

        while (have_boundary && frame == boundary_frame) {
            engine->current_step16 = (uint8_t)(boundary_step & 15);

            for (uint8_t r = 0; r < GRID_HEIGHT; ++r) {
                for (uint8_t c = 0; c < GRID_WIDTH; ++c) {
                    AchordPadState *pad = &engine->pads[r][c];
                    if (!pad->logic_active) continue;

                    if (pad->pending_quantized && pad->pending_step <= (uint64_t)boundary_step) {
                        pad->pending_quantized = 0;
                        if (engine->config.trigger_mode == ACHORD_TRIGGER_REPEAT) {
                            trigger_repeat_pulse(engine, r, c, frame);
                            pad->last_repeat_step = (uint64_t)boundary_step;
                        } else {
                            activate_sustained_pad(engine, r, c, frame);
                        }
                    } else if (engine->config.trigger_mode == ACHORD_TRIGGER_REPEAT &&
                               !pad->pending_quantized &&
                               (((uint64_t)boundary_step & 1u) == 0u) &&
                               (uint64_t)boundary_step > pad->last_repeat_step) {
                        trigger_repeat_pulse(engine, r, c, frame);
                        pad->last_repeat_step = (uint64_t)boundary_step;
                    }
                }
            }

            boundary_step++;
            if (boundary_step <= boundary_end) {
                boundary_frame = (uint32_t)frame_for_boundary(engine->block_abs_steps_start,
                                                              engine->block_abs_steps_end,
                                                              n_samples,
                                                              boundary_step);
                have_boundary = 1;
            } else {
                have_boundary = 0;
            }
        }
    }

    normalize_pending_note_offs(engine, n_samples);
    for (uint8_t r = 0; r < GRID_HEIGHT; ++r) {
        for (uint8_t c = 0; c < GRID_WIDTH; ++c) {
            AchordPadState *pad = &engine->pads[r][c];
            if (pad->strum_pending && pad->strum_next_frame >= n_samples) {
                pad->strum_next_frame -= n_samples;
            }
        }
    }
}
