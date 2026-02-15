#include "../include/arpiso_engine.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t k_scale_major[] = {0, 2, 4, 5, 7, 9, 11};
static const uint8_t k_scale_minor[] = {0, 2, 3, 5, 7, 8, 10};
static const uint8_t k_scale_dorian[] = {0, 2, 3, 5, 7, 9, 10};
static const uint8_t k_scale_pent[] = {0, 2, 4, 7, 9};

static const uint8_t k_cycle_lengths[] = {8, 12, 16, 24};
static const uint8_t k_clock_divisors[] = {1, 2, 4, 8};

static uint8_t clamp_u8(int v, int lo, int hi) {
    if (v < lo) return (uint8_t)lo;
    if (v > hi) return (uint8_t)hi;
    return (uint8_t)v;
}

static uint8_t quantize_scale(uint8_t semitone, uint8_t scale_index) {
    const uint8_t *scale = k_scale_major;
    uint8_t len = (uint8_t)(sizeof(k_scale_major) / sizeof(k_scale_major[0]));

    switch (scale_index & 3) {
        case 1:
            scale = k_scale_minor;
            len = (uint8_t)(sizeof(k_scale_minor) / sizeof(k_scale_minor[0]));
            break;
        case 2:
            scale = k_scale_dorian;
            len = (uint8_t)(sizeof(k_scale_dorian) / sizeof(k_scale_dorian[0]));
            break;
        case 3:
            scale = k_scale_pent;
            len = (uint8_t)(sizeof(k_scale_pent) / sizeof(k_scale_pent[0]));
            break;
        default:
            break;
    }

    uint8_t oct = semitone / 12;
    uint8_t deg = semitone % 12;
    uint8_t nearest = scale[0];
    uint8_t best = 12;
    for (uint8_t i = 0; i < len; ++i) {
        uint8_t d = (uint8_t)abs((int)deg - (int)scale[i]);
        if (d < best) {
            best = d;
            nearest = scale[i];
        }
    }
    return (uint8_t)(oct * 12 + nearest);
}

static uint8_t note_from_grid(const ArpIsoEngine *engine, uint8_t row, uint8_t col) {
    int semitone = (int)engine->root_note + (int)col + (int)row * 5;
    semitone = clamp_u8(semitone, 24, 108);
    return quantize_scale((uint8_t)semitone, engine->scale_index);
}

static uint8_t euclid_hit(uint8_t step, uint8_t pulses, uint8_t offset, uint8_t length) {
    if (length == 0 || pulses == 0) return 0;
    if (pulses >= length) return 1;
    uint8_t s = (uint8_t)((step + length - (offset % length)) % length);
    return (((uint16_t)s * (uint16_t)pulses) % length) < pulses;
}

static uint32_t compute_travel_samples(const ArpIsoEngine *engine, uint8_t a, uint8_t b) {
    if (a >= MAX_WELLS || b >= MAX_WELLS || !engine->wells[a].active || !engine->wells[b].active) {
        return engine->samples_per_tick;
    }

    int dr = abs((int)engine->wells[a].row - (int)engine->wells[b].row);
    int dc = abs((int)engine->wells[a].col - (int)engine->wells[b].col);
    int d = dr + dc;
    if (d < 1) d = 1;

    uint32_t base = engine->samples_per_tick;
    uint32_t dist_scale = (uint32_t)d;
    uint32_t user_scale = (uint32_t)(32 + engine->travel_scale); // 32..159
    uint32_t v = (base * dist_scale * user_scale) / 64u;
    if (v < 64) v = 64;
    return v;
}

static int find_well_index(const ArpIsoEngine *engine, uint8_t row, uint8_t col) {
    for (uint8_t i = 0; i < MAX_WELLS; ++i) {
        if (engine->wells[i].active && engine->wells[i].row == row && engine->wells[i].col == col) {
            return (int)i;
        }
    }
    return -1;
}

static uint8_t first_free_well(const ArpIsoEngine *engine) {
    for (uint8_t i = 0; i < MAX_WELLS; ++i) {
        if (!engine->wells[i].active) return i;
    }
    return 0xFF;
}

static void queue_note_event(ArpIsoEngine *engine,
                             uint8_t note,
                             uint8_t velocity,
                             uint32_t gate_frames) {
    if (engine->midi_event_count + 2 > MAX_MIDI_EVENTS) {
        return;
    }

    uint8_t vel = velocity;
    if (engine->velocity_curve > 0) {
        float t = (float)velocity / 127.0f;
        float shaped = powf(t, 1.25f - ((float)engine->velocity_curve / 254.0f));
        vel = clamp_u8((int)(shaped * 127.0f), 1, 127);
    }

    engine->midi_events[engine->midi_event_count++] =
        (MidiOutEvent){0, 3, {0x90, note, vel}};
    engine->midi_events[engine->midi_event_count++] =
        (MidiOutEvent){gate_frames ? gate_frames : 1, 3, {0x80, note, 0}};
}

static uint8_t cycle_length(const ArpIsoEngine *engine) {
    return k_cycle_lengths[engine->cycle_length_index & 3];
}

static void update_held_count(ArpIsoEngine *engine) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < MAX_WELLS; ++i) {
        if (engine->wells[i].active) n++;
    }
    engine->held_count = n;
}

static uint8_t choose_target_well(const ArpIsoEngine *engine, uint8_t source) {
    if (source >= MAX_WELLS || !engine->wells[source].active || engine->held_count <= 1) {
        return source;
    }

    uint8_t best = source;
    int best_dist = 999;

    for (uint8_t i = 0; i < MAX_WELLS; ++i) {
        if (!engine->wells[i].active || i == source) continue;
        int dr = abs((int)engine->wells[source].row - (int)engine->wells[i].row);
        int dc = abs((int)engine->wells[source].col - (int)engine->wells[i].col);
        int d = dr + dc;

        if (engine->motion_mode == 1) {
            d = 16 - d; // prefer far jumps
        }

        if (d < best_dist) {
            best_dist = d;
            best = i;
        }
    }

    return best;
}

static void refresh_well_mapping(ArpIsoEngine *engine, uint8_t idx) {
    if (idx >= MAX_WELLS || !engine->wells[idx].active) return;

    ArpIsoWell *w = &engine->wells[idx];
    w->note = note_from_grid(engine, w->row, w->col);

    uint8_t row_density = (uint8_t)(2 + (w->row * 6) / 7); // 2..8
    int pulses = (int)row_density + (int)engine->density_bias / 32 - 2;
    pulses = clamp_u8(pulses, 1, cycle_length(engine));
    w->pulses = (uint8_t)pulses;

    w->offset = (uint8_t)((w->col + (engine->phase_bias / 16)) % cycle_length(engine));
}

void arpiso_set_tempo(ArpIsoEngine *engine, uint16_t bpm) {
    if (bpm < 30) bpm = 30;
    if (bpm > 300) bpm = 300;

    float samples_per_beat = (engine->sample_rate * 60.0f) / (float)bpm;
    uint8_t div = k_clock_divisors[engine->clock_division_index & 3];
    uint32_t s = (uint32_t)(samples_per_beat / (4.0f * (float)div));
    if (s == 0) s = 1;

    engine->samples_per_tick = s;
    engine->grid_state.tempo_bpm = (uint8_t)bpm;
}

uint32_t arpiso_default_gate_frames(const ArpIsoEngine *engine) {
    uint32_t g = (engine->samples_per_tick * (uint32_t)engine->gate_percent) / 100u;
    if (g == 0) g = 1;
    return g;
}

void arpiso_init(ArpIsoEngine *engine, float sample_rate) {
    memset(engine, 0, sizeof(*engine));

    engine->sample_rate = sample_rate;
    engine->root_note = 48;
    engine->scale_index = 0;
    engine->density_bias = 32;
    engine->phase_bias = 0;
    engine->gravity_strength = 64;
    engine->travel_scale = 64;
    engine->gate_percent = 50;
    engine->velocity_curve = 32;
    engine->humanize = 0;
    engine->clock_division_index = 0;
    engine->cycle_length_index = 2;
    engine->motion_mode = 0;
    engine->pattern_slot = 0;

    grid_state_init(&engine->grid_state);
    arpiso_set_tempo(engine, 120);

    arpiso_set_pattern(engine, 0);
    arpiso_reset(engine);
}

void arpiso_reset(ArpIsoEngine *engine) {
    engine->playing = 0;
    engine->current_step = 0;
    engine->sample_counter = 0;
    engine->midi_event_count = 0;
    engine->rejected_press_flash = 0;

    memset(engine->wells, 0, sizeof(engine->wells));
    memset(engine->playheads, 0, sizeof(engine->playheads));

    update_held_count(engine);
    grid_state_clear(&engine->grid_state);
}

void arpiso_start(ArpIsoEngine *engine) {
    engine->playing = 1;
    engine->grid_state.playing = 1;
}

void arpiso_stop(ArpIsoEngine *engine) {
    engine->playing = 0;
    engine->grid_state.playing = 0;
}

void arpiso_begin_block(ArpIsoEngine *engine) {
    engine->midi_event_count = 0;
}

void arpiso_handle_pad_press(ArpIsoEngine *engine, uint8_t row, uint8_t col, uint8_t velocity) {
    if (row >= GRID_HEIGHT || col >= GRID_WIDTH) {
        return;
    }

    int existing = find_well_index(engine, row, col);
    if (existing >= 0) {
        engine->wells[existing].velocity = velocity;
        refresh_well_mapping(engine, (uint8_t)existing);
        return;
    }

    update_held_count(engine);
    if (engine->held_count >= MAX_WELLS) {
        engine->rejected_press_flash = 24;
        return;
    }

    uint8_t slot = first_free_well(engine);
    if (slot == 0xFF) {
        engine->rejected_press_flash = 24;
        return;
    }

    ArpIsoWell *w = &engine->wells[slot];
    w->active = 1;
    w->row = row;
    w->col = col;
    w->velocity = velocity;
    refresh_well_mapping(engine, slot);

    ArpIsoPlayhead *ph = &engine->playheads[slot];
    ph->active = 1;
    ph->source_well = slot;
    ph->target_well = choose_target_well(engine, slot);
    ph->phase_samples = 0;
    ph->travel_samples = compute_travel_samples(engine, ph->source_well, ph->target_well);
    ph->euclid_step = 0;
    ph->latched_note = w->note;

    update_held_count(engine);
}

void arpiso_handle_pad_release(ArpIsoEngine *engine, uint8_t row, uint8_t col) {
    int idx = find_well_index(engine, row, col);
    if (idx < 0) return;

    engine->wells[idx].active = 0;
    engine->playheads[idx].active = 0;

    for (uint8_t i = 0; i < MAX_PLAYHEADS; ++i) {
        if (!engine->playheads[i].active) continue;
        if (engine->playheads[i].source_well == (uint8_t)idx) {
            engine->playheads[i].source_well = choose_target_well(engine, (uint8_t)idx);
        }
        if (engine->playheads[i].target_well == (uint8_t)idx) {
            engine->playheads[i].target_well = choose_target_well(engine, engine->playheads[i].source_well);
        }
    }

    update_held_count(engine);
}

void arpiso_set_pattern(ArpIsoEngine *engine, uint8_t pattern) {
    uint8_t old = engine->pattern_slot & 1;
    engine->patterns[old].density_bias = engine->density_bias;
    engine->patterns[old].phase_bias = engine->phase_bias;
    engine->patterns[old].gravity_strength = engine->gravity_strength;
    engine->patterns[old].travel_scale = engine->travel_scale;
    engine->patterns[old].gate_percent = engine->gate_percent;
    engine->patterns[old].velocity_curve = engine->velocity_curve;
    engine->patterns[old].humanize = engine->humanize;
    engine->patterns[old].root_note = engine->root_note;
    engine->patterns[old].scale_index = engine->scale_index;
    engine->patterns[old].motion_mode = engine->motion_mode;
    engine->patterns[old].clock_division_index = engine->clock_division_index;
    engine->patterns[old].cycle_length_index = engine->cycle_length_index;

    engine->pattern_slot = (uint8_t)(pattern & 1);
    const ArpIsoPatternState *s = &engine->patterns[engine->pattern_slot];
    if (s->root_note == 0) {
        return;
    }

    engine->density_bias = s->density_bias;
    engine->phase_bias = s->phase_bias;
    engine->gravity_strength = s->gravity_strength;
    engine->travel_scale = s->travel_scale;
    engine->gate_percent = s->gate_percent;
    engine->velocity_curve = s->velocity_curve;
    engine->humanize = s->humanize;
    engine->root_note = s->root_note;
    engine->scale_index = s->scale_index;
    engine->motion_mode = s->motion_mode;
    engine->clock_division_index = s->clock_division_index;
    engine->cycle_length_index = s->cycle_length_index;

    arpiso_set_tempo(engine, engine->grid_state.tempo_bpm ? engine->grid_state.tempo_bpm : 120);

    for (uint8_t i = 0; i < MAX_WELLS; ++i) {
        refresh_well_mapping(engine, i);
    }
}

void arpiso_clear_pattern(ArpIsoEngine *engine) {
    memset(engine->wells, 0, sizeof(engine->wells));
    memset(engine->playheads, 0, sizeof(engine->playheads));
    update_held_count(engine);
}

void arpiso_handle_side_button(ArpIsoEngine *engine, uint8_t index) {
    switch (index) {
        case 0: engine->density_bias = (uint8_t)((engine->density_bias + 16) & 0x7F); break;
        case 1: engine->phase_bias = (uint8_t)((engine->phase_bias + 16) & 0x7F); break;
        case 2: engine->gravity_strength = (uint8_t)((engine->gravity_strength + 16) & 0x7F); break;
        case 3: engine->travel_scale = (uint8_t)((engine->travel_scale + 16) & 0x7F); break;
        case 4: {
            uint8_t v = (uint8_t)(engine->gate_percent + 10);
            engine->gate_percent = (v > 95) ? 25 : v;
            break;
        }
        case 5: engine->velocity_curve = (uint8_t)((engine->velocity_curve + 24) & 0x7F); break;
        case 6: engine->humanize = (uint8_t)((engine->humanize + 16) & 0x7F); break;
        case 7: arpiso_set_pattern(engine, (uint8_t)(engine->pattern_slot ^ 1)); break;
        default: break;
    }

    for (uint8_t i = 0; i < MAX_WELLS; ++i) {
        refresh_well_mapping(engine, i);
    }
}

void arpiso_handle_top_button(ArpIsoEngine *engine, uint8_t index) {
    switch (index) {
        case 0:
            if (engine->playing) arpiso_stop(engine); else arpiso_start(engine);
            break;
        case 1:
            engine->clock_division_index = (uint8_t)((engine->clock_division_index + 1) & 3);
            arpiso_set_tempo(engine, engine->grid_state.tempo_bpm ? engine->grid_state.tempo_bpm : 120);
            break;
        case 2:
            engine->cycle_length_index = (uint8_t)((engine->cycle_length_index + 1) & 3);
            break;
        case 3:
            engine->root_note = (uint8_t)(engine->root_note + 2);
            if (engine->root_note > 72) engine->root_note = 36;
            break;
        case 4:
            engine->scale_index = (uint8_t)((engine->scale_index + 1) & 3);
            break;
        case 5:
            engine->motion_mode = (uint8_t)((engine->motion_mode + 1) % 3);
            break;
        case 6:
            arpiso_clear_pattern(engine);
            break;
        case 7:
        case 8:
            // Panic: stop and clear held wells.
            arpiso_stop(engine);
            arpiso_clear_pattern(engine);
            break;
        default:
            break;
    }

    for (uint8_t i = 0; i < MAX_WELLS; ++i) {
        refresh_well_mapping(engine, i);
    }
}

void arpiso_process(ArpIsoEngine *engine, uint32_t n_samples) {
    if (!engine->playing || engine->held_count == 0) {
        return;
    }

    uint32_t gate_frames = arpiso_default_gate_frames(engine);
    if (gate_frames >= n_samples) {
        gate_frames = (n_samples > 1) ? (n_samples - 1) : 1;
    }

    for (uint32_t i = 0; i < n_samples; ++i) {
        engine->sample_counter++;
        if (engine->sample_counter < engine->samples_per_tick) {
            continue;
        }

        engine->sample_counter = 0;
        engine->current_step = (uint16_t)((engine->current_step + 1) % STEP_COUNT);
        engine->grid_state.step_counter = engine->current_step;

        uint8_t cyc_len = cycle_length(engine);

        for (uint8_t p = 0; p < MAX_PLAYHEADS; ++p) {
            ArpIsoPlayhead *ph = &engine->playheads[p];
            if (!ph->active || ph->source_well >= MAX_WELLS || !engine->wells[ph->source_well].active) {
                continue;
            }

            ArpIsoWell *src = &engine->wells[ph->source_well];
            ph->euclid_step = (uint8_t)((ph->euclid_step + 1) % cyc_len);

            if (euclid_hit(ph->euclid_step, src->pulses, src->offset, cyc_len)) {
                uint8_t vel = src->velocity ? src->velocity : 96;
                queue_note_event(engine, src->note, vel, gate_frames);
                ph->latched_note = src->note;
            }

            ph->phase_samples += engine->samples_per_tick;
            if (ph->phase_samples >= ph->travel_samples) {
                ph->phase_samples = 0;
                ph->source_well = ph->target_well;
                ph->target_well = choose_target_well(engine, ph->source_well);
                ph->travel_samples = compute_travel_samples(engine, ph->source_well, ph->target_well);
            }
        }
    }
}

void arpiso_refresh_grid_state(ArpIsoEngine *engine) {
    for (uint8_t r = 0; r < GRID_HEIGHT; ++r) {
        for (uint8_t c = 0; c < GRID_WIDTH; ++c) {
            grid_state_set_led(&engine->grid_state, r, c, COLOR_OFF);
        }
    }

    for (uint8_t i = 0; i < MAX_WELLS; ++i) {
        if (!engine->wells[i].active) continue;
        uint8_t color = COLOR_BLUE_DIM;
        if (engine->wells[i].velocity > 96) color = COLOR_BLUE_BRIGHT;
        else if (engine->wells[i].velocity > 0) color = COLOR_BLUE;
        grid_state_set_led(&engine->grid_state, engine->wells[i].row, engine->wells[i].col, color);
    }

    for (uint8_t i = 0; i < MAX_PLAYHEADS; ++i) {
        if (!engine->playheads[i].active) continue;
        uint8_t wi = engine->playheads[i].source_well;
        if (wi >= MAX_WELLS || !engine->wells[wi].active) continue;
        uint8_t row = engine->wells[wi].row;
        uint8_t col = engine->wells[wi].col;
        uint8_t pulse = (uint8_t)((engine->current_step + i) % cycle_length(engine));
        uint8_t hit = euclid_hit(pulse,
                                 engine->wells[wi].pulses,
                                 engine->wells[wi].offset,
                                 cycle_length(engine));
        grid_state_set_led(&engine->grid_state, row, col, hit ? COLOR_GREEN_BRIGHT : COLOR_GREEN_DIM);
    }

    // Side controls (top->bottom order on device).
    grid_state_set_side_button(&engine->grid_state, 0, 0, COLOR_CYAN_DIM);
    grid_state_set_side_button(&engine->grid_state, 1, 0, COLOR_CYAN_DIM);
    grid_state_set_side_button(&engine->grid_state, 2, 0, COLOR_YELLOW_DIM);
    grid_state_set_side_button(&engine->grid_state, 3, 0, COLOR_BLUE_DIM);
    grid_state_set_side_button(&engine->grid_state, 4, 0, COLOR_ORANGE_DIM);
    grid_state_set_side_button(&engine->grid_state, 5, 0, COLOR_PURPLE_DIM);
    grid_state_set_side_button(&engine->grid_state, 6, 0, COLOR_PINK_DIM);
    grid_state_set_side_button(&engine->grid_state, 7, 0,
                               engine->pattern_slot ? COLOR_WHITE : COLOR_GRAY_MED);

    // Top controls.
    grid_state_set_top_button(&engine->grid_state, 0, 0,
                              engine->playing ? COLOR_GREEN_BRIGHT : COLOR_GREEN_DIM);
    grid_state_set_top_button(&engine->grid_state, 1, 0, COLOR_CYAN_DIM);
    grid_state_set_top_button(&engine->grid_state, 2, 0, COLOR_BLUE_DIM);
    grid_state_set_top_button(&engine->grid_state, 3, 0, COLOR_YELLOW_DIM);
    grid_state_set_top_button(&engine->grid_state, 4, 0, COLOR_PURPLE_DIM);
    grid_state_set_top_button(&engine->grid_state, 5, 0, COLOR_PINK_DIM);
    grid_state_set_top_button(&engine->grid_state, 6, 0, COLOR_RED_DIM);
    grid_state_set_top_button(&engine->grid_state, 7, 0,
                              engine->rejected_press_flash ? COLOR_RED_BRIGHT : COLOR_RED_DIM);
    grid_state_set_top_button(&engine->grid_state, 8, 0,
                              engine->pattern_slot ? COLOR_WHITE : COLOR_GRAY_MED);

    if (engine->rejected_press_flash > 0) {
        engine->rejected_press_flash--;
    }

    // Keep compatibility fields used by UI state packet.
    engine->grid_state.pattern = engine->pattern_slot;
}
