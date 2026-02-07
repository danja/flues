#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>
#include <lv2/midi/midi.h>
#include <lv2/state/state.h>

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cmath>

#define MIDI_FLIP_URI "https://danja.github.io/flues/plugins/midi-flip"

enum PortIndex {
    PORT_MIDI_IN = 0,
    PORT_MIDI_OUT = 1,
    PORT_PIVOT = 2
};

struct MidiFlipURIDs {
    LV2_URID atom_Sequence = 0;
    LV2_URID midi_Event = 0;
    LV2_URID atom_Float = 0;
    LV2_URID state_pivot = 0;
};

struct MidiFlip {
    const LV2_Atom_Sequence* midi_in = nullptr;
    LV2_Atom_Sequence* midi_out = nullptr;
    const float* pivot_port = nullptr;

    LV2_URID_Map* map = nullptr;
    MidiFlipURIDs urids{};

    float cached_pivot = 60.0f;
    uint32_t buffer_capacity = 8192;
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

static void update_cached_params(MidiFlip* self) {
    if (!self) return;
    if (self->pivot_port) {
        self->cached_pivot = *self->pivot_port;
    }
}

static LV2_State_Status midi_flip_state_save(
    LV2_Handle instance,
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t /*flags*/,
    const LV2_Feature* const* /*features*/) {
    MidiFlip* self = static_cast<MidiFlip*>(instance);
    if (!self || !store) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    update_cached_params(self);
    const uint32_t flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;

    store(handle,
          self->urids.state_pivot,
          &self->cached_pivot,
          sizeof(float),
          self->urids.atom_Float,
          flags);

    return LV2_STATE_SUCCESS;
}

static LV2_State_Status midi_flip_state_restore(
    LV2_Handle instance,
    LV2_State_Retrieve_Function retrieve,
    LV2_State_Handle handle,
    uint32_t /*flags*/,
    const LV2_Feature* const* /*features*/) {
    MidiFlip* self = static_cast<MidiFlip*>(instance);
    if (!self || !retrieve) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    size_t size = 0;
    uint32_t type = 0;
    uint32_t val_flags = 0;
    const void* data = retrieve(handle, self->urids.state_pivot, &size, &type, &val_flags);
    if (data && size >= sizeof(float) && type == self->urids.atom_Float) {
        self->cached_pivot = *static_cast<const float*>(data);
    }

    return LV2_STATE_SUCCESS;
}

static const LV2_State_Interface midi_flip_state_interface = {
    midi_flip_state_save,
    midi_flip_state_restore
};

static LV2_Handle instantiate(const LV2_Descriptor* /*descriptor*/,
                              double /*rate*/,
                              const char* /*bundle_path*/,
                              const LV2_Feature* const* features) {
    MidiFlip* self = new MidiFlip();
    if (!self) {
        return nullptr;
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
    self->urids.state_pivot = self->map->map(self->map->handle, MIDI_FLIP_URI "#pivot");

    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    MidiFlip* self = static_cast<MidiFlip*>(instance);
    if (!self) return;

    switch (port) {
        case PORT_MIDI_IN:
            self->midi_in = static_cast<const LV2_Atom_Sequence*>(data);
            break;
        case PORT_MIDI_OUT:
            self->midi_out = static_cast<LV2_Atom_Sequence*>(data);
            break;
        case PORT_PIVOT:
            self->pivot_port = static_cast<const float*>(data);
            break;
        default:
            break;
    }
}

static void activate(LV2_Handle instance) {
    MidiFlip* self = static_cast<MidiFlip*>(instance);
    if (!self) return;
    self->cached_pivot = 60.0f;
}

static void run(LV2_Handle instance, uint32_t nframes) {
    MidiFlip* self = static_cast<MidiFlip*>(instance);
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

    const int pivot = static_cast<int>(std::lround(self->cached_pivot));

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

        if ((type == 0x90 || type == 0x80) && size >= 3) {
            uint8_t note = msg[1] & 0x7F;
            int flipped = (2 * pivot) - note;
            uint8_t out_note = clamp_midi(flipped);

            uint8_t out_msg[3] = {status, out_note, msg[2]};
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
    MidiFlip* self = static_cast<MidiFlip*>(instance);
    delete self;
}

static const void* extension_data(const char* uri) {
    if (!std::strcmp(uri, LV2_STATE__interface)) {
        return &midi_flip_state_interface;
    }
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    MIDI_FLIP_URI,
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
