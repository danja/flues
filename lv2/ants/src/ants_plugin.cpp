#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>
#include <lv2/midi/midi.h>
#include <lv2/time/time.h>

#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>

#define ANTS_URI "https://danja.github.io/flues/plugins/ants"

static constexpr int kGridSize = 16;
static constexpr int kMaxAnts = 8;
static constexpr int kMaxOffs = 64;

enum PortIndex {
    PORT_CONTROL = 0,
    PORT_MIDI_OUT = 1,
    PORT_ANTS = 2,
    PORT_VOICES = 3,
    PORT_SCALE = 4,
    PORT_ROOT = 5,
    PORT_STEPS = 6,
    PORT_SPEED = 7,
    PORT_RANDOM = 8,
    PORT_TRAIL = 9,
    PORT_DECAY = 10,
    PORT_NOTE_LEN = 11,
    PORT_GAP = 12,
    PORT_VELOCITY = 13,
    PORT_DENSITY = 14
};

struct AntsURIDs {
    LV2_URID atom_Object = 0;
    LV2_URID atom_Sequence = 0;
    LV2_URID atom_Float = 0;
    LV2_URID atom_Int = 0;
    LV2_URID time_Position = 0;
    LV2_URID time_speed = 0;
    LV2_URID time_bar = 0;
    LV2_URID time_barBeat = 0;
    LV2_URID time_beatsPerBar = 0;
    LV2_URID time_beatsPerMinute = 0;
    LV2_URID midi_Event = 0;
};

struct Ant {
    int x = 0;
    int y = 0;
};

struct NoteOffEvent {
    uint64_t frame = 0;
    uint8_t note = 0;
    uint8_t channel = 0;
    bool active = false;
};

struct Ants {
    const LV2_Atom_Sequence* control = nullptr;
    LV2_Atom_Sequence* midi_out = nullptr;

    const float* ants_port = nullptr;
    const float* voices_port = nullptr;
    const float* scale_port = nullptr;
    const float* root_port = nullptr;
    const float* steps_port = nullptr;
    const float* speed_port = nullptr;
    const float* random_port = nullptr;
    const float* trail_port = nullptr;
    const float* decay_port = nullptr;
    const float* note_len_port = nullptr;
    const float* gap_port = nullptr;
    const float* velocity_port = nullptr;
    const float* density_port = nullptr;

    LV2_URID_Map* map = nullptr;
    AntsURIDs urids{};

    double sample_rate = 48000.0;

    float grid[kGridSize][kGridSize] = {{0.0f}};
    Ant ants[kMaxAnts];
    int ant_count = 4;

    uint32_t rng_state = 0x12345678u;

    NoteOffEvent offs[kMaxOffs] = {};
    double next_emit_time[kMaxAnts] = {};
    int rr_index = 0;
    double next_tick_beat = 0.0;
    double tick_interval = 1.0;
    bool tick_ready = false;
    double last_beat_start = 0.0;
    bool playing_state = false;
    float last_bpm = 120.0f;
    double last_beats_per_bar = 4.0;
    double last_beat_pos = 0.0;
    bool have_last_pos = false;
    double last_tick_time = -1.0;
};

static const int kScaleCount = 10;
static const int kScaleSizes[kScaleCount] = {
    12, 7, 7, 7, 7, 5, 5, 6, 7, 7
};

static const int kScales[kScaleCount][12] = {
    {0,1,2,3,4,5,6,7,8,9,10,11},
    {0,2,4,5,7,9,11},
    {0,2,3,5,7,8,10},
    {0,2,3,5,7,8,11},
    {0,2,3,5,7,9,11},
    {0,2,4,7,9},
    {0,3,5,7,10},
    {0,3,5,6,7,10},
    {0,2,3,5,7,9,10},
    {0,2,4,5,7,9,10}
};

static inline float rand_float(uint32_t* state) {
    *state = (*state * 1664525u) + 1013904223u;
    return static_cast<float>(*state & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

static inline int clampi(int v, int min_v, int max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

static void append_midi(LV2_Atom_Sequence* seq,
                        uint32_t capacity,
                        LV2_URID midi_event_urid,
                        uint32_t frame,
                        const uint8_t* msg,
                        uint32_t size) {
    LV2_Atom_Event ev;
    ev.time.frames = frame;
    ev.body.type = midi_event_urid;
    ev.body.size = size;

    LV2_Atom_Event* appended = lv2_atom_sequence_append_event(seq, capacity, &ev);
    if (appended) {
        uint8_t* body = (uint8_t*)(appended + 1);
        std::memcpy(body, msg, size);
    }
}

static int note_for_row(int row, int root, int scale_index) {
    scale_index = clampi(scale_index, 0, kScaleCount - 1);
    int scale_len = kScaleSizes[scale_index];
    int degree = row % scale_len;
    int octave = row / scale_len;
    int base = 48 + (root % 12); // C3 base
    int note = base + (octave * 12) + kScales[scale_index][degree];
    return clampi(note, 0, 127);
}

static void decay_grid(Ants* self, float decay) {
    for (int y = 0; y < kGridSize; ++y) {
        for (int x = 0; x < kGridSize; ++x) {
            self->grid[y][x] *= decay;
        }
    }
}

static void move_ant(Ants* self, Ant& ant, float random_w, float trail_w) {
    const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    float best_score = -1.0f;
    int best_dir = 0;

    for (int i = 0; i < 4; ++i) {
        int nx = (ant.x + dirs[i][0] + kGridSize) % kGridSize;
        int ny = (ant.y + dirs[i][1] + kGridSize) % kGridSize;
        float pher = self->grid[ny][nx];
        float score = trail_w * pher + random_w * rand_float(&self->rng_state);
        if (score > best_score) {
            best_score = score;
            best_dir = i;
        }
    }

    ant.x = (ant.x + dirs[best_dir][0] + kGridSize) % kGridSize;
    ant.y = (ant.y + dirs[best_dir][1] + kGridSize) % kGridSize;
}

static void emit_note_on(Ants* self, uint32_t frame, uint8_t note, uint8_t velocity) {
    uint8_t msg[3] = {0x90, note, velocity};
    append_midi(self->midi_out, 8192, self->urids.midi_Event, frame, msg, 3);
}

static void emit_note_off(Ants* self, uint32_t frame, uint8_t note) {
    uint8_t msg[3] = {0x80, note, 0};
    append_midi(self->midi_out, 8192, self->urids.midi_Event, frame, msg, 3);
}

static LV2_Handle instantiate(const LV2_Descriptor* /*descriptor*/,
                              double rate,
                              const char* /*bundle_path*/,
                              const LV2_Feature* const* features) {
    Ants* self = new Ants();
    if (!self) return nullptr;

    self->sample_rate = rate;

    for (int i = 0; features[i]; ++i) {
        if (!std::strcmp(features[i]->URI, LV2_URID__map)) {
            self->map = static_cast<LV2_URID_Map*>(features[i]->data);
        }
    }

    if (!self->map) {
        delete self;
        return nullptr;
    }

    self->urids.atom_Object = self->map->map(self->map->handle, LV2_ATOM__Object);
    self->urids.atom_Sequence = self->map->map(self->map->handle, LV2_ATOM__Sequence);
    self->urids.atom_Float = self->map->map(self->map->handle, LV2_ATOM__Float);
    self->urids.atom_Int = self->map->map(self->map->handle, LV2_ATOM__Int);
    self->urids.time_Position = self->map->map(self->map->handle, LV2_TIME__Position);
    self->urids.time_speed = self->map->map(self->map->handle, LV2_TIME__speed);
    self->urids.time_bar = self->map->map(self->map->handle, LV2_TIME__bar);
    self->urids.time_barBeat = self->map->map(self->map->handle, LV2_TIME__barBeat);
    self->urids.time_beatsPerBar = self->map->map(self->map->handle, LV2_TIME__beatsPerBar);
    self->urids.time_beatsPerMinute = self->map->map(self->map->handle, LV2_TIME__beatsPerMinute);
    self->urids.midi_Event = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);

    for (int i = 0; i < kMaxAnts; ++i) {
        self->ants[i].x = i % kGridSize;
        self->ants[i].y = i % kGridSize;
    }

    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    Ants* self = static_cast<Ants*>(instance);
    if (!self) return;

    switch (port) {
        case PORT_CONTROL: self->control = static_cast<const LV2_Atom_Sequence*>(data); break;
        case PORT_MIDI_OUT: self->midi_out = static_cast<LV2_Atom_Sequence*>(data); break;
        case PORT_ANTS: self->ants_port = static_cast<const float*>(data); break;
        case PORT_VOICES: self->voices_port = static_cast<const float*>(data); break;
        case PORT_SCALE: self->scale_port = static_cast<const float*>(data); break;
        case PORT_ROOT: self->root_port = static_cast<const float*>(data); break;
        case PORT_STEPS: self->steps_port = static_cast<const float*>(data); break;
        case PORT_SPEED: self->speed_port = static_cast<const float*>(data); break;
        case PORT_RANDOM: self->random_port = static_cast<const float*>(data); break;
        case PORT_TRAIL: self->trail_port = static_cast<const float*>(data); break;
        case PORT_DECAY: self->decay_port = static_cast<const float*>(data); break;
        case PORT_NOTE_LEN: self->note_len_port = static_cast<const float*>(data); break;
        case PORT_GAP: self->gap_port = static_cast<const float*>(data); break;
        case PORT_VELOCITY: self->velocity_port = static_cast<const float*>(data); break;
        case PORT_DENSITY: self->density_port = static_cast<const float*>(data); break;
        default: break;
    }
}

static void activate(LV2_Handle instance) {
    Ants* self = static_cast<Ants*>(instance);
    if (!self) return;
    std::memset(self->grid, 0, sizeof(self->grid));
    for (int i = 0; i < kMaxAnts; ++i) {
        self->ants[i].x = i % kGridSize;
        self->ants[i].y = i % kGridSize;
        self->next_emit_time[i] = -1000000.0;
    }
    for (int i = 0; i < kMaxOffs; ++i) {
        self->offs[i].active = false;
    }
    self->rr_index = 0;
    self->next_tick_beat = 0.0;
    self->tick_interval = 1.0;
    self->tick_ready = false;
    self->last_beat_start = 0.0;
    self->playing_state = false;
    self->last_bpm = 120.0f;
    self->last_beats_per_bar = 4.0;
    self->last_beat_pos = 0.0;
    self->have_last_pos = false;
    self->last_tick_time = -1.0;
}

static void run(LV2_Handle instance, uint32_t nframes) {
    Ants* self = static_cast<Ants*>(instance);
    if (!self || !self->midi_out) return;

    self->midi_out->atom.type = self->urids.atom_Sequence;
    self->midi_out->atom.size = sizeof(LV2_Atom_Sequence_Body);
    self->midi_out->body.unit = 0;
    self->midi_out->body.pad = 0;

    float bpm = self->last_bpm;
    bool playing = self->playing_state;
    double bar = 0.0;
    double barBeat = 0.0;
    double beatsPerBar = self->last_beats_per_bar;
    bool saw_position = false;
    bool saw_speed = false;

    if (self->control && self->control->atom.type == self->urids.atom_Sequence) {
        LV2_ATOM_SEQUENCE_FOREACH(self->control, ev) {
            const LV2_Atom_Object* obj = nullptr;
            if (ev->body.type == self->urids.time_Position) {
                obj = reinterpret_cast<const LV2_Atom_Object*>(&ev->body);
            } else if (ev->body.type == self->urids.atom_Object) {
                const LV2_Atom_Object* cand = reinterpret_cast<const LV2_Atom_Object*>(&ev->body);
                if (cand->body.otype == self->urids.time_Position) {
                    obj = cand;
                }
            }
            if (!obj) continue;
            saw_position = true;

            const LV2_Atom* barAtom = nullptr;
            const LV2_Atom* barBeatAtom = nullptr;
            const LV2_Atom* beatsPerBarAtom = nullptr;
            const LV2_Atom* bpmAtom = nullptr;
            const LV2_Atom* speedAtom = nullptr;

            lv2_atom_object_get(obj,
                                self->urids.time_bar, &barAtom,
                                self->urids.time_barBeat, &barBeatAtom,
                                self->urids.time_beatsPerBar, &beatsPerBarAtom,
                                self->urids.time_beatsPerMinute, &bpmAtom,
                                self->urids.time_speed, &speedAtom,
                                0);

            if (barAtom && (barAtom->type == self->urids.atom_Float || barAtom->type == self->urids.atom_Int)) {
                bar = (barAtom->type == self->urids.atom_Float)
                    ? reinterpret_cast<const LV2_Atom_Float*>(barAtom)->body
                    : reinterpret_cast<const LV2_Atom_Int*>(barAtom)->body;
            }
            if (barBeatAtom && (barBeatAtom->type == self->urids.atom_Float || barBeatAtom->type == self->urids.atom_Int)) {
                barBeat = (barBeatAtom->type == self->urids.atom_Float)
                    ? reinterpret_cast<const LV2_Atom_Float*>(barBeatAtom)->body
                    : reinterpret_cast<const LV2_Atom_Int*>(barBeatAtom)->body;
            }
            if (beatsPerBarAtom && beatsPerBarAtom->type == self->urids.atom_Float) {
                beatsPerBar = reinterpret_cast<const LV2_Atom_Float*>(beatsPerBarAtom)->body;
            }
            if (bpmAtom && bpmAtom->type == self->urids.atom_Float) {
                bpm = reinterpret_cast<const LV2_Atom_Float*>(bpmAtom)->body;
            }
            if (speedAtom && speedAtom->type == self->urids.atom_Float) {
                playing = reinterpret_cast<const LV2_Atom_Float*>(speedAtom)->body > 0.0f;
                saw_speed = true;
            }
        }
    }

    if (saw_position && !saw_speed) {
        // If speed isn't provided, assume playing while the transport is reporting position.
        playing = self->playing_state ? self->playing_state : true;
    }

    self->playing_state = playing;
    self->last_bpm = bpm;
    self->last_beats_per_bar = beatsPerBar;

    if (!playing || bpm <= 0.0) {
        return;
    }

    const int ants_count = clampi(self->ants_port ? (int)lroundf(*self->ants_port) : 4, 2, 8);
    const int voices = clampi(self->voices_port ? (int)lroundf(*self->voices_port) : 2, 1, 4);
    const int scale_index = self->scale_port ? (int)lroundf(*self->scale_port) : 1;
    const int root = self->root_port ? (int)lroundf(*self->root_port) : 0;
    const int steps = clampi(self->steps_port ? (int)lroundf(*self->steps_port) : 1, 1, 4);
    const int speed_index = clampi(self->speed_port ? (int)lroundf(*self->speed_port) : 2, 0, 4);
    const float random_w = self->random_port ? *self->random_port : 0.35f;
    const float trail_w = self->trail_port ? *self->trail_port : 0.7f;
    const float decay = self->decay_port ? *self->decay_port : 0.9f;
    const float note_len = self->note_len_port ? *self->note_len_port : 0.5f;
    const float gap_beats = self->gap_port ? *self->gap_port : 0.0f;
    const float velocity_scale = self->velocity_port ? *self->velocity_port : 1.0f;
    const float density = self->density_port ? *self->density_port : 0.75f;

    const double frames_per_beat = self->sample_rate * 60.0 / bpm;
    double beat_start = bar * beatsPerBar + barBeat;
    if (!saw_position) {
        if (!self->have_last_pos) {
            return;
        }
        beat_start = self->last_beat_pos + (double)nframes / frames_per_beat;
    }
    const double beat_end = beat_start + (double)nframes / frames_per_beat;
    self->last_beat_pos = beat_start;
    self->have_last_pos = true;
    uint32_t capacity = 8192;

    for (int i = 0; i < kMaxOffs; ++i) {
        if (!self->offs[i].active) continue;
        if (self->offs[i].frame < (uint64_t)nframes) {
            emit_note_off(self, (uint32_t)self->offs[i].frame, self->offs[i].note);
            self->offs[i].active = false;
        } else {
            self->offs[i].frame -= nframes;
        }
    }

    double interval = 1.0;
    switch (speed_index) {
        case 0: interval = 4.0; break;
        case 1: interval = 2.0; break;
        case 2: interval = 1.0; break;
        case 3: interval = 0.5; break;
        case 4: interval = 0.25; break;
        default: interval = 1.0; break;
    }

    const bool interval_changed = std::fabs(interval - self->tick_interval) > 1e-9;
    const bool jumped_back = beat_start < self->last_beat_start - 0.001;
    const bool lagging = beat_start - self->next_tick_beat > interval * 4.0;
    const bool ahead = self->next_tick_beat - beat_start > interval * 4.0;

    if (!self->tick_ready || interval_changed || jumped_back || lagging || ahead) {
        self->tick_interval = interval;
        self->tick_ready = true;
        self->next_tick_beat = std::floor(beat_start / interval) * interval;
        if (self->next_tick_beat < beat_start - 1e-9) {
            self->next_tick_beat += interval;
        }
    }
    self->last_beat_start = beat_start;

    double tick = self->next_tick_beat;
    const double tick_window = frames_per_beat * interval;

    while (tick < beat_end + 1e-6) {
        if (tick >= beat_start - 1e-9) {
            if (self->last_tick_time >= 0.0) {
                if (tick < self->last_tick_time - 1e-6 || tick - self->last_tick_time > interval * 8.0) {
                    for (int a = 0; a < ants_count; ++a) {
                        self->next_emit_time[a] = tick;
                    }
                }
            }
            double tick_offset = (tick - beat_start) * frames_per_beat;
            if (tick_offset < (double)nframes) {
                decay_grid(self, decay);

                if (ants_count > 0) {
                    for (int a = 0; a < ants_count; ++a) {
                        for (int s = 0; s < steps; ++s) {
                            move_ant(self, self->ants[a], random_w, trail_w);
                        }
                    }
                }

                int emitted = 0;
                int attempts = 0;
                const double emit_spacing = tick_window / (double)voices;
                while (emitted < voices && attempts < ants_count) {
                    int a = self->rr_index % ants_count;
                    self->rr_index = (self->rr_index + 1) % ants_count;
                    attempts++;

                    Ant& ant = self->ants[a];
                    if (tick < self->next_emit_time[a]) {
                        continue;
                    }
                    if (rand_float(&self->rng_state) > density) {
                        continue;
                    }

                    int note = note_for_row(ant.y, root, scale_index);
                    float pher = self->grid[ant.y][ant.x];
                    uint8_t vel = (uint8_t)clampi((int)lroundf(40.0f + pher * 100.0f * velocity_scale), 1, 127);

                    const double emit_offset = tick_offset + (emitted * emit_spacing);
                    if (emit_offset >= nframes) {
                        emitted++;
                        continue;
                    }

                    emit_note_on(self, (uint32_t)emit_offset, (uint8_t)note, vel);
                    self->next_emit_time[a] = tick + gap_beats;

                    uint64_t off_frame = (uint64_t)lround(emit_offset + note_len * frames_per_beat);
                    if (off_frame < (uint64_t)nframes) {
                        emit_note_off(self, (uint32_t)off_frame, (uint8_t)note);
                    } else {
                        for (int o = 0; o < kMaxOffs; ++o) {
                            if (!self->offs[o].active) {
                                self->offs[o].frame = off_frame - nframes;
                                self->offs[o].note = (uint8_t)note;
                                self->offs[o].channel = 0;
                                self->offs[o].active = true;
                                break;
                            }
                        }
                    }

                    emitted++;

                    for (int b = 0; b < ants_count; ++b) {
                        if (b == a) continue;
                        if (self->ants[b].x == ant.x && self->ants[b].y == ant.y) {
                            emit_note_on(self, (uint32_t)emit_offset, (uint8_t)note, vel);
                            uint64_t off2 = (uint64_t)lround(emit_offset + note_len * frames_per_beat);
                            if (off2 < (uint64_t)nframes) {
                                emit_note_off(self, (uint32_t)off2, (uint8_t)note);
                            } else {
                                for (int o = 0; o < kMaxOffs; ++o) {
                                    if (!self->offs[o].active) {
                                        self->offs[o].frame = off2 - nframes;
                                        self->offs[o].note = (uint8_t)note;
                                        self->offs[o].channel = 0;
                                        self->offs[o].active = true;
                                        break;
                                    }
                                }
                            }
                            break;
                        }
                    }
                }

                for (int a = 0; a < ants_count; ++a) {
                    float updated = self->grid[self->ants[a].y][self->ants[a].x] + trail_w * 0.2f;
                    if (updated > 1.0f) updated = 1.0f;
                    self->grid[self->ants[a].y][self->ants[a].x] = updated;
                }
            }
        }

        self->last_tick_time = tick;
        tick += interval;
    }

    self->next_tick_beat = tick;
}

static void deactivate(LV2_Handle /*instance*/) {}

static void cleanup(LV2_Handle instance) {
    Ants* self = static_cast<Ants*>(instance);
    delete self;
}

static const void* extension_data(const char* uri) {
    (void)uri;
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    ANTS_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return (index == 0) ? &descriptor : nullptr;
}
