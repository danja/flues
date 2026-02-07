#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>
#include <lv2/midi/midi.h>
#include <lv2/state/state.h>

#include <cstdint>
#include <cstring>
#include <cmath>

#define QUANTICO_URI "https://danja.github.io/flues/plugins/quantico"

enum PortIndex {
    PORT_MIDI_IN = 0,
    PORT_MIDI_OUT = 1,
    PORT_KEY = 2,
    PORT_SCALE = 3
};

struct QuanticoURIDs {
    LV2_URID atom_Sequence = 0;
    LV2_URID midi_Event = 0;
    LV2_URID atom_Float = 0;
    LV2_URID state_key = 0;
    LV2_URID state_scale = 0;
};

struct Quantico {
    const LV2_Atom_Sequence* midi_in = nullptr;
    LV2_Atom_Sequence* midi_out = nullptr;
    const float* key_port = nullptr;
    const float* scale_port = nullptr;

    LV2_URID_Map* map = nullptr;
    QuanticoURIDs urids{};

    float cached_key = 0.0f;
    float cached_scale = 1.0f;

    uint32_t buffer_capacity = 8192;
    int16_t note_map[16][128];
};

static const int kScaleCount = 10;
static const int kScaleSizes[kScaleCount] = {
    12, // Chromatic
    7,  // Major
    7,  // Natural Minor
    7,  // Harmonic Minor
    7,  // Melodic Minor
    5,  // Pentatonic Major
    5,  // Pentatonic Minor
    6,  // Blues
    7,  // Dorian
    7   // Mixolydian
};

static const int kScales[kScaleCount][12] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},        // Chromatic
    {0, 2, 4, 5, 7, 9, 11},                        // Major
    {0, 2, 3, 5, 7, 8, 10},                        // Natural Minor
    {0, 2, 3, 5, 7, 8, 11},                        // Harmonic Minor
    {0, 2, 3, 5, 7, 9, 11},                        // Melodic Minor
    {0, 2, 4, 7, 9},                               // Pentatonic Major
    {0, 3, 5, 7, 10},                              // Pentatonic Minor
    {0, 3, 5, 6, 7, 10},                           // Blues
    {0, 2, 3, 5, 7, 9, 10},                        // Dorian
    {0, 2, 4, 5, 7, 9, 10}                         // Mixolydian
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

static void update_cached_params(Quantico* self) {
    if (!self) return;
    if (self->key_port) {
        self->cached_key = *self->key_port;
    }
    if (self->scale_port) {
        self->cached_scale = *self->scale_port;
    }
}

static int quantize_note(int note, int key, int scale_index) {
    if (scale_index < 0) scale_index = 0;
    if (scale_index >= kScaleCount) scale_index = kScaleCount - 1;
    key = key % 12;
    if (key < 0) key += 12;

    const int* scale = kScales[scale_index];
    const int scale_size = kScaleSizes[scale_index];

    const int octave = note / 12;
    int best_note = note;
    int best_dist = 128;

    for (int oct = octave - 1; oct <= octave + 1; ++oct) {
        if (oct < -1 || oct > 10) {
            continue;
        }
        for (int i = 0; i < scale_size; ++i) {
            int candidate = (oct * 12) + key + scale[i];
            if (candidate < 0 || candidate > 127) {
                continue;
            }
            int dist = candidate - note;
            int abs_dist = dist < 0 ? -dist : dist;
            if (abs_dist < best_dist || (abs_dist == best_dist && candidate < best_note)) {
                best_dist = abs_dist;
                best_note = candidate;
            }
        }
    }

    return best_note;
}

static LV2_State_Status quantico_state_save(
    LV2_Handle instance,
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t /*flags*/,
    const LV2_Feature* const* /*features*/) {
    Quantico* self = static_cast<Quantico*>(instance);
    if (!self || !store) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    update_cached_params(self);
    const uint32_t flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;

    store(handle, self->urids.state_key,
          &self->cached_key, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_scale,
          &self->cached_scale, sizeof(float), self->urids.atom_Float, flags);

    return LV2_STATE_SUCCESS;
}

static LV2_State_Status quantico_state_restore(
    LV2_Handle instance,
    LV2_State_Retrieve_Function retrieve,
    LV2_State_Handle handle,
    uint32_t /*flags*/,
    const LV2_Feature* const* /*features*/) {
    Quantico* self = static_cast<Quantico*>(instance);
    if (!self || !retrieve) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    size_t size = 0;
    uint32_t type = 0;
    uint32_t val_flags = 0;

    const void* data = retrieve(handle, self->urids.state_key, &size, &type, &val_flags);
    if (data && size >= sizeof(float) && type == self->urids.atom_Float) {
        self->cached_key = *static_cast<const float*>(data);
    }

    data = retrieve(handle, self->urids.state_scale, &size, &type, &val_flags);
    if (data && size >= sizeof(float) && type == self->urids.atom_Float) {
        self->cached_scale = *static_cast<const float*>(data);
    }

    return LV2_STATE_SUCCESS;
}

static const LV2_State_Interface quantico_state_interface = {
    quantico_state_save,
    quantico_state_restore
};

static LV2_Handle instantiate(const LV2_Descriptor* /*descriptor*/,
                              double /*rate*/,
                              const char* /*bundle_path*/,
                              const LV2_Feature* const* features) {
    Quantico* self = new Quantico();
    if (!self) {
        return nullptr;
    }

    for (int ch = 0; ch < 16; ++ch) {
        for (int n = 0; n < 128; ++n) {
            self->note_map[ch][n] = -1;
        }
    }

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
    self->urids.state_key = self->map->map(self->map->handle, QUANTICO_URI "#key");
    self->urids.state_scale = self->map->map(self->map->handle, QUANTICO_URI "#scale");

    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    Quantico* self = static_cast<Quantico*>(instance);
    if (!self) return;

    switch (port) {
        case PORT_MIDI_IN:
            self->midi_in = static_cast<const LV2_Atom_Sequence*>(data);
            break;
        case PORT_MIDI_OUT:
            self->midi_out = static_cast<LV2_Atom_Sequence*>(data);
            break;
        case PORT_KEY:
            self->key_port = static_cast<const float*>(data);
            break;
        case PORT_SCALE:
            self->scale_port = static_cast<const float*>(data);
            break;
        default:
            break;
    }
}

static void activate(LV2_Handle instance) {
    Quantico* self = static_cast<Quantico*>(instance);
    if (!self) return;

    self->cached_key = 0.0f;
    self->cached_scale = 1.0f;
    for (int ch = 0; ch < 16; ++ch) {
        for (int n = 0; n < 128; ++n) {
            self->note_map[ch][n] = -1;
        }
    }
}

static void run(LV2_Handle instance, uint32_t nframes) {
    Quantico* self = static_cast<Quantico*>(instance);
    if (!self || !self->midi_out) {
        return;
    }

    update_cached_params(self);

    self->midi_out->atom.type = self->urids.atom_Sequence;
    self->midi_out->atom.size = sizeof(LV2_Atom_Sequence_Body);
    self->midi_out->body.unit = 0;
    self->midi_out->body.pad = 0;

    if (!self->midi_in) {
        return;
    }

    const int key = static_cast<int>(lroundf(self->cached_key));
    const int scale = static_cast<int>(lroundf(self->cached_scale));

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

        if ((type == 0x90 || type == 0x80) && size >= 3) {
            uint8_t note = msg[1] & 0x7F;
            uint8_t vel = msg[2] & 0x7F;
            bool is_note_on = (type == 0x90) && vel > 0;

            uint8_t out_note = note;
            if (is_note_on) {
                int quantized = quantize_note(note, key, scale);
                out_note = clamp_midi(quantized);
                self->note_map[channel][note] = out_note;
            } else {
                int mapped = self->note_map[channel][note];
                if (mapped >= 0) {
                    out_note = static_cast<uint8_t>(mapped);
                } else {
                    int quantized = quantize_note(note, key, scale);
                    out_note = clamp_midi(quantized);
                }
                self->note_map[channel][note] = -1;
            }

            uint8_t out_msg[3] = {status, out_note, vel};
            append_midi(self->midi_out, self->buffer_capacity, self->urids.midi_Event,
                        ev->time.frames, out_msg, 3);
        } else {
            append_midi(self->midi_out, self->buffer_capacity, self->urids.midi_Event,
                        ev->time.frames, msg, size);
        }
    }
}

static void deactivate(LV2_Handle /*instance*/) {
}

static void cleanup(LV2_Handle instance) {
    Quantico* self = static_cast<Quantico*>(instance);
    delete self;
}

static const void* extension_data(const char* uri) {
    if (!std::strcmp(uri, LV2_STATE__interface)) {
        return &quantico_state_interface;
    }
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    QUANTICO_URI,
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
