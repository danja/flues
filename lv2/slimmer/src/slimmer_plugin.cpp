#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>
#include <lv2/midi/midi.h>
#include <lv2/state/state.h>

#include <cstdint>
#include <cstring>
#include <cmath>

#define SLIMMER_URI "https://danja.github.io/flues/plugins/slimmer"

enum PortIndex {
    PORT_MIDI_IN = 0,
    PORT_MIDI_OUT = 1,
    PORT_MODE = 2,
    PORT_GAP_MS = 3,
    PORT_HOLD_MS = 4,
    PORT_RETRIGGER = 5
};

enum Mode {
    MODE_HIGHEST = 0,
    MODE_LOWEST = 1,
    MODE_NEAREST = 2,
    MODE_FARTHEST = 3,
    MODE_VELOCITY = 4,
    MODE_MOST_RECENT = 5,
    MODE_OLDEST = 6,
    MODE_CENTER = 7,
    MODE_ROUND_ROBIN = 8,
    MODE_RANDOM = 9
};

struct SlimmerURIDs {
    LV2_URID atom_Sequence = 0;
    LV2_URID midi_Event = 0;
    LV2_URID atom_Float = 0;
    LV2_URID state_mode = 0;
    LV2_URID state_gap = 0;
    LV2_URID state_hold = 0;
    LV2_URID state_retrigger = 0;
};

struct Slimmer {
    const LV2_Atom_Sequence* midi_in = nullptr;
    LV2_Atom_Sequence* midi_out = nullptr;
    const float* mode_port = nullptr;
    const float* gap_port = nullptr;
    const float* hold_port = nullptr;
    const float* retrigger_port = nullptr;

    LV2_URID_Map* map = nullptr;
    SlimmerURIDs urids{};

    double sample_rate = 48000.0;

    float cached_mode = 0.0f;
    float cached_gap = 30.0f;
    float cached_hold = 120.0f;
    float cached_retrigger = 0.0f;

    uint32_t buffer_capacity = 8192;
    uint64_t frame_time = 0;

    bool held[128] = {false};
    uint8_t velocity[128] = {0};
    uint64_t last_on_time[128] = {0};

    int current_note = -1;
    uint8_t current_velocity = 0;
    uint8_t current_channel = 0;
    uint64_t note_on_frame = 0;
    uint64_t pending_note_off = 0;

    int pending_note = -1;
    uint8_t pending_velocity = 0;
    uint8_t pending_channel = 0;
    uint64_t pending_note_on = 0;

    uint64_t next_allowed_frame = 0;
    int last_output_note = -1;
    int last_rr_note = -1;

    uint32_t rng_state = 0x12345678u;
};

static inline uint8_t clamp_midi(int value) {
    if (value < 0) return 0;
    if (value > 127) return 127;
    return static_cast<uint8_t>(value);
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

static inline float rand_float(uint32_t* state) {
    *state = (*state * 1664525u) + 1013904223u;
    return static_cast<float>(*state & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

static void update_cached_params(Slimmer* self) {
    if (!self) return;
    if (self->mode_port) self->cached_mode = *self->mode_port;
    if (self->gap_port) self->cached_gap = *self->gap_port;
    if (self->hold_port) self->cached_hold = *self->hold_port;
    if (self->retrigger_port) self->cached_retrigger = *self->retrigger_port;
}

static int select_note(Slimmer* self, int mode) {
    if (!self) return -1;

    int count = 0;
    int min_note = 128;
    int max_note = -1;
    int best = -1;

    for (int n = 0; n < 128; ++n) {
        if (self->held[n]) {
            count++;
            if (n < min_note) min_note = n;
            if (n > max_note) max_note = n;
        }
    }

    if (count == 0) return -1;

    switch (mode) {
        case MODE_HIGHEST:
            return max_note;
        case MODE_LOWEST:
            return min_note;
        case MODE_NEAREST: {
            int ref = (self->last_output_note >= 0) ? self->last_output_note : min_note;
            int best_dist = 999;
            for (int n = 0; n < 128; ++n) {
                if (!self->held[n]) continue;
                int dist = n - ref;
                int abs_dist = dist < 0 ? -dist : dist;
                if (abs_dist < best_dist || (abs_dist == best_dist && n < best)) {
                    best_dist = abs_dist;
                    best = n;
                }
            }
            return best;
        }
        case MODE_FARTHEST: {
            int ref = (self->last_output_note >= 0) ? self->last_output_note : min_note;
            int best_dist = -1;
            for (int n = 0; n < 128; ++n) {
                if (!self->held[n]) continue;
                int dist = n - ref;
                int abs_dist = dist < 0 ? -dist : dist;
                if (abs_dist > best_dist || (abs_dist == best_dist && n < best)) {
                    best_dist = abs_dist;
                    best = n;
                }
            }
            return best;
        }
        case MODE_VELOCITY: {
            int best_vel = -1;
            for (int n = 0; n < 128; ++n) {
                if (!self->held[n]) continue;
                if (self->velocity[n] > best_vel) {
                    best_vel = self->velocity[n];
                    best = n;
                }
            }
            return best;
        }
        case MODE_MOST_RECENT: {
            uint64_t best_time = 0;
            for (int n = 0; n < 128; ++n) {
                if (!self->held[n]) continue;
                if (self->last_on_time[n] >= best_time) {
                    best_time = self->last_on_time[n];
                    best = n;
                }
            }
            return best;
        }
        case MODE_OLDEST: {
            uint64_t best_time = UINT64_MAX;
            for (int n = 0; n < 128; ++n) {
                if (!self->held[n]) continue;
                if (self->last_on_time[n] < best_time) {
                    best_time = self->last_on_time[n];
                    best = n;
                }
            }
            return best;
        }
        case MODE_CENTER: {
            int sum = 0;
            for (int n = 0; n < 128; ++n) {
                if (self->held[n]) sum += n;
            }
            float avg = (float)sum / (float)count;
            int best_dist = 999;
            for (int n = 0; n < 128; ++n) {
                if (!self->held[n]) continue;
                int dist = (int)lroundf(fabsf(avg - (float)n));
                if (dist < best_dist || (dist == best_dist && n < best)) {
                    best_dist = dist;
                    best = n;
                }
            }
            return best;
        }
        case MODE_ROUND_ROBIN: {
            int start = self->last_rr_note;
            if (start < 0) start = min_note - 1;
            for (int n = start + 1; n < 128; ++n) {
                if (self->held[n]) {
                    self->last_rr_note = n;
                    return n;
                }
            }
            for (int n = 0; n < start + 1; ++n) {
                if (self->held[n]) {
                    self->last_rr_note = n;
                    return n;
                }
            }
            return min_note;
        }
        case MODE_RANDOM: {
            int target = (int)(rand_float(&self->rng_state) * count);
            int idx = 0;
            for (int n = 0; n < 128; ++n) {
                if (!self->held[n]) continue;
                if (idx == target) return n;
                idx++;
            }
            return min_note;
        }
        default:
            return max_note;
    }
}

static void schedule_note_off(Slimmer* self, uint64_t frame) {
    if (self->current_note < 0) return;

    const uint64_t min_hold_frames = (uint64_t)lround(self->sample_rate * (self->cached_hold * 0.001));
    uint64_t off_time = frame;
    if (self->note_on_frame + min_hold_frames > off_time) {
        off_time = self->note_on_frame + min_hold_frames;
    }

    if (self->pending_note_off == 0 || off_time < self->pending_note_off) {
        self->pending_note_off = off_time;
    }

    const uint64_t gap_frames = (uint64_t)lround(self->sample_rate * (self->cached_gap * 0.001));
    self->next_allowed_frame = off_time + gap_frames;
}

static void emit_note_on(Slimmer* self, uint64_t abs_frame, uint64_t block_start) {
    if (self->pending_note < 0) return;
    uint64_t offset = abs_frame - block_start;

    uint8_t msg[3] = {static_cast<uint8_t>(0x90 | (self->pending_channel & 0x0F)),
                      static_cast<uint8_t>(self->pending_note & 0x7F),
                      self->pending_velocity};

    append_midi(self->midi_out, self->buffer_capacity, self->urids.midi_Event,
                (uint32_t)offset, msg, 3);

    self->current_note = self->pending_note;
    self->current_velocity = self->pending_velocity;
    self->current_channel = self->pending_channel;
    self->note_on_frame = abs_frame;
    self->last_output_note = self->current_note;

    self->pending_note = -1;
    self->pending_velocity = 0;
    self->pending_channel = 0;
    self->pending_note_on = 0;
}

static void emit_note_off(Slimmer* self, uint64_t abs_frame, uint64_t block_start) {
    if (self->current_note < 0) return;

    uint64_t offset = abs_frame - block_start;

    uint8_t msg[3] = {static_cast<uint8_t>(0x80 | (self->current_channel & 0x0F)),
                      static_cast<uint8_t>(self->current_note & 0x7F),
                      0};

    append_midi(self->midi_out, self->buffer_capacity, self->urids.midi_Event,
                (uint32_t)offset, msg, 3);

    self->current_note = -1;
    self->current_velocity = 0;
    self->note_on_frame = 0;
    self->pending_note_off = 0;
}

static void try_trigger_pending(Slimmer* self, uint64_t block_start, uint64_t block_end) {
    if (self->pending_note < 0) return;

    uint64_t start_time = self->pending_note_on;
    if (start_time == 0) start_time = self->next_allowed_frame;

    if (start_time < block_start) start_time = block_start;

    if (start_time >= block_start && start_time < block_end) {
        emit_note_on(self, start_time, block_start);
    }
}

static LV2_State_Status slimmer_state_save(
    LV2_Handle instance,
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t /*flags*/,
    const LV2_Feature* const* /*features*/) {
    Slimmer* self = static_cast<Slimmer*>(instance);
    if (!self || !store) return LV2_STATE_ERR_UNKNOWN;

    update_cached_params(self);
    const uint32_t flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;

    store(handle, self->urids.state_mode, &self->cached_mode, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_gap, &self->cached_gap, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_hold, &self->cached_hold, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_retrigger, &self->cached_retrigger, sizeof(float), self->urids.atom_Float, flags);

    return LV2_STATE_SUCCESS;
}

static LV2_State_Status slimmer_state_restore(
    LV2_Handle instance,
    LV2_State_Retrieve_Function retrieve,
    LV2_State_Handle handle,
    uint32_t /*flags*/,
    const LV2_Feature* const* /*features*/) {
    Slimmer* self = static_cast<Slimmer*>(instance);
    if (!self || !retrieve) return LV2_STATE_ERR_UNKNOWN;

    size_t size = 0;
    uint32_t type = 0;
    uint32_t val_flags = 0;

    const void* data = retrieve(handle, self->urids.state_mode, &size, &type, &val_flags);
    if (data && size >= sizeof(float) && type == self->urids.atom_Float) {
        self->cached_mode = *static_cast<const float*>(data);
    }

    data = retrieve(handle, self->urids.state_gap, &size, &type, &val_flags);
    if (data && size >= sizeof(float) && type == self->urids.atom_Float) {
        self->cached_gap = *static_cast<const float*>(data);
    }

    data = retrieve(handle, self->urids.state_hold, &size, &type, &val_flags);
    if (data && size >= sizeof(float) && type == self->urids.atom_Float) {
        self->cached_hold = *static_cast<const float*>(data);
    }

    data = retrieve(handle, self->urids.state_retrigger, &size, &type, &val_flags);
    if (data && size >= sizeof(float) && type == self->urids.atom_Float) {
        self->cached_retrigger = *static_cast<const float*>(data);
    }

    return LV2_STATE_SUCCESS;
}

static const LV2_State_Interface slimmer_state_interface = {
    slimmer_state_save,
    slimmer_state_restore
};

static LV2_Handle instantiate(const LV2_Descriptor* /*descriptor*/,
                              double rate,
                              const char* /*bundle_path*/,
                              const LV2_Feature* const* features) {
    Slimmer* self = new Slimmer();
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

    self->urids.atom_Sequence = self->map->map(self->map->handle, LV2_ATOM__Sequence);
    self->urids.midi_Event = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);
    self->urids.atom_Float = self->map->map(self->map->handle, LV2_ATOM__Float);

    self->urids.state_mode = self->map->map(self->map->handle, SLIMMER_URI "#mode");
    self->urids.state_gap = self->map->map(self->map->handle, SLIMMER_URI "#gap_ms");
    self->urids.state_hold = self->map->map(self->map->handle, SLIMMER_URI "#hold_ms");
    self->urids.state_retrigger = self->map->map(self->map->handle, SLIMMER_URI "#retrigger");

    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    Slimmer* self = static_cast<Slimmer*>(instance);
    if (!self) return;

    switch (port) {
        case PORT_MIDI_IN:
            self->midi_in = static_cast<const LV2_Atom_Sequence*>(data);
            break;
        case PORT_MIDI_OUT:
            self->midi_out = static_cast<LV2_Atom_Sequence*>(data);
            break;
        case PORT_MODE:
            self->mode_port = static_cast<const float*>(data);
            break;
        case PORT_GAP_MS:
            self->gap_port = static_cast<const float*>(data);
            break;
        case PORT_HOLD_MS:
            self->hold_port = static_cast<const float*>(data);
            break;
        case PORT_RETRIGGER:
            self->retrigger_port = static_cast<const float*>(data);
            break;
        default:
            break;
    }
}

static void activate(LV2_Handle instance) {
    Slimmer* self = static_cast<Slimmer*>(instance);
    if (!self) return;

    self->cached_mode = 0.0f;
    self->cached_gap = 30.0f;
    self->cached_hold = 120.0f;
    self->cached_retrigger = 0.0f;

    self->frame_time = 0;
    self->current_note = -1;
    self->pending_note = -1;
    self->pending_note_off = 0;
    self->next_allowed_frame = 0;
    self->last_output_note = -1;
    self->last_rr_note = -1;

    std::memset(self->held, 0, sizeof(self->held));
    std::memset(self->velocity, 0, sizeof(self->velocity));
    std::memset(self->last_on_time, 0, sizeof(self->last_on_time));
}

static void run(LV2_Handle instance, uint32_t nframes) {
    Slimmer* self = static_cast<Slimmer*>(instance);
    if (!self || !self->midi_out) return;

    update_cached_params(self);

    self->midi_out->atom.type = self->urids.atom_Sequence;
    self->midi_out->atom.size = sizeof(LV2_Atom_Sequence_Body);
    self->midi_out->body.unit = 0;
    self->midi_out->body.pad = 0;

    const uint64_t block_start = self->frame_time;
    const uint64_t block_end = self->frame_time + nframes;

    if (self->pending_note_off) {
        if (self->pending_note_off < block_start) {
            emit_note_off(self, block_start, block_start);
        } else if (self->pending_note_off < block_end) {
            emit_note_off(self, self->pending_note_off, block_start);
        }
    }

    try_trigger_pending(self, block_start, block_end);

    if (self->midi_in) {
        LV2_ATOM_SEQUENCE_FOREACH(self->midi_in, ev) {
            if (ev->body.type != self->urids.midi_Event) {
                continue;
            }

            const uint8_t* msg = (const uint8_t*)(ev + 1);
            const uint32_t size = ev->body.size;
            if (size < 2) {
                continue;
            }

            const uint8_t status = msg[0];
            const uint8_t type = status & 0xF0;
            const uint8_t channel = status & 0x0F;
            const uint64_t abs_frame = block_start + ev->time.frames;

            if ((type == 0x90 || type == 0x80) && size >= 3) {
                uint8_t note = msg[1] & 0x7F;
                uint8_t vel = msg[2] & 0x7F;
                bool is_note_on = (type == 0x90) && vel > 0;

                if (is_note_on) {
                    self->held[note] = true;
                    self->velocity[note] = vel;
                    self->last_on_time[note] = abs_frame;
                } else {
                    self->held[note] = false;
                    self->velocity[note] = 0;
                }

                int mode = (int)lroundf(self->cached_mode);
                if (mode < 0) mode = 0;
                if (mode > MODE_RANDOM) mode = MODE_RANDOM;

                if (is_note_on) {
                    int chosen = select_note(self, mode);
                    if (chosen >= 0) {
                        bool same_note = (self->current_note == chosen);
                        bool retrigger = (lroundf(self->cached_retrigger) > 0.5f);

                        if (self->current_note < 0 && abs_frame >= self->next_allowed_frame) {
                            self->pending_note = chosen;
                            self->pending_velocity = self->velocity[chosen];
                            self->pending_channel = channel;
                            self->pending_note_on = abs_frame;
                            try_trigger_pending(self, block_start, block_end);
                        } else if (self->current_note < 0 && abs_frame < self->next_allowed_frame) {
                            self->pending_note = chosen;
                            self->pending_velocity = self->velocity[chosen];
                            self->pending_channel = channel;
                            self->pending_note_on = self->next_allowed_frame;
                        } else if (!same_note || retrigger) {
                            self->pending_note = chosen;
                            self->pending_velocity = self->velocity[chosen];
                            self->pending_channel = channel;
                            self->pending_note_on = 0;
                            schedule_note_off(self, abs_frame);
                        }
                    }
                } else {
                    if (self->current_note == note) {
                        schedule_note_off(self, abs_frame);
                    }
                }
            } else {
                append_midi(self->midi_out, self->buffer_capacity, self->urids.midi_Event,
                            ev->time.frames, msg, size);
            }
        }
    }

    if (self->pending_note_off) {
        if (self->pending_note_off < block_start) {
            emit_note_off(self, block_start, block_start);
    } else if (self->pending_note_off < block_end) {
            emit_note_off(self, self->pending_note_off, block_start);
    }
    }

    try_trigger_pending(self, block_start, block_end);

    self->frame_time += nframes;
}

static void deactivate(LV2_Handle /*instance*/) {}

static void cleanup(LV2_Handle instance) {
    Slimmer* self = static_cast<Slimmer*>(instance);
    delete self;
}

static const void* extension_data(const char* uri) {
    if (!std::strcmp(uri, LV2_STATE__interface)) {
        return &slimmer_state_interface;
    }
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    SLIMMER_URI,
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
