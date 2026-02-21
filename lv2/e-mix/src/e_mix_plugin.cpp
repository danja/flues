#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/state/state.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#define EMIX_URI "https://danja.github.io/flues/plugins/e-mix"

static constexpr uint32_t kMaxChannels = 8;

enum PortIndex {
    PORT_CONTROL = 0,
    PORT_AUDIO_IN_1,
    PORT_AUDIO_IN_2,
    PORT_AUDIO_IN_3,
    PORT_AUDIO_IN_4,
    PORT_AUDIO_IN_5,
    PORT_AUDIO_IN_6,
    PORT_AUDIO_IN_7,
    PORT_AUDIO_IN_8,
    PORT_AUDIO_OUT_1,
    PORT_AUDIO_OUT_2,
    PORT_AUDIO_OUT_3,
    PORT_AUDIO_OUT_4,
    PORT_AUDIO_OUT_5,
    PORT_AUDIO_OUT_6,
    PORT_AUDIO_OUT_7,
    PORT_AUDIO_OUT_8,
    PORT_TOTAL_BARS,
    PORT_DIVISION,
    PORT_STEPS,
    PORT_OFFSET,
    PORT_FADE,
    PORT_TOTAL_COUNT
};

struct EMixURIDs {
    LV2_URID atom_Object = 0;
    LV2_URID atom_Blank = 0;
    LV2_URID atom_Float = 0;
    LV2_URID atom_Double = 0;
    LV2_URID atom_Int = 0;
    LV2_URID atom_Long = 0;
    LV2_URID time_Position = 0;
    LV2_URID time_speed = 0;
    LV2_URID time_bar = 0;
    LV2_URID time_barBeat = 0;
    LV2_URID time_beatsPerBar = 0;
    LV2_URID time_beatsPerMinute = 0;
    LV2_URID state_total_bars = 0;
    LV2_URID state_division = 0;
    LV2_URID state_steps = 0;
    LV2_URID state_offset = 0;
    LV2_URID state_fade = 0;
};

struct TimeInfo {
    bool valid = false;
    bool playing = false;
    double bar = 0.0;
    double barBeat = 0.0;
    double beatsPerBar = 4.0;
    double bpm = 120.0;
};

struct EMix {
    const LV2_Atom_Sequence* control = nullptr;
    const float* audio_in[kMaxChannels] = {nullptr};
    float* audio_out[kMaxChannels] = {nullptr};

    const float* total_bars_port = nullptr;
    const float* division_port = nullptr;
    const float* steps_port = nullptr;
    const float* offset_port = nullptr;
    const float* fade_port = nullptr;

    LV2_URID_Map* map = nullptr;
    EMixURIDs urids{};

    double sample_rate = 48000.0;

    float cached_total_bars = 128.0f;
    float cached_division = 16.0f;
    float cached_steps = 8.0f;
    float cached_offset = 0.0f;
    float cached_fade = 0.0f;

    bool transport_was_playing = false;
    double cycle_origin_bar = 0.0;

    double fallback_bar_pos = 0.0;
    double fallback_bpm = 120.0;
    double fallback_beats_per_bar = 4.0;
};

static inline float clampf(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static inline int clampi(int value, int minValue, int maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static bool atom_to_double(const LV2_Atom* atom, const EMixURIDs& urids, double* out) {
    if (!atom || !out) {
        return false;
    }
    if (atom->type == urids.atom_Float) {
        *out = reinterpret_cast<const LV2_Atom_Float*>(atom)->body;
        return true;
    }
    if (atom->type == urids.atom_Double) {
        *out = reinterpret_cast<const LV2_Atom_Double*>(atom)->body;
        return true;
    }
    if (atom->type == urids.atom_Int) {
        *out = reinterpret_cast<const LV2_Atom_Int*>(atom)->body;
        return true;
    }
    if (atom->type == urids.atom_Long) {
        *out = reinterpret_cast<const LV2_Atom_Long*>(atom)->body;
        return true;
    }
    return false;
}

static bool read_time_info(const LV2_Atom_Sequence* control, const EMixURIDs& urids, TimeInfo* info) {
    if (!control || !info) {
        return false;
    }

    bool found = false;
    TimeInfo local = *info;

    LV2_ATOM_SEQUENCE_FOREACH(control, ev) {
        const LV2_Atom_Object* obj = nullptr;
        if (ev->body.type == urids.time_Position) {
            obj = reinterpret_cast<const LV2_Atom_Object*>(&ev->body);
        } else if (ev->body.type == urids.atom_Object || ev->body.type == urids.atom_Blank) {
            const LV2_Atom_Object* cand = reinterpret_cast<const LV2_Atom_Object*>(&ev->body);
            if (cand->body.otype == urids.time_Position) {
                obj = cand;
            }
        }

        if (!obj) {
            continue;
        }

        found = true;

        const LV2_Atom* barAtom = nullptr;
        const LV2_Atom* barBeatAtom = nullptr;
        const LV2_Atom* beatsPerBarAtom = nullptr;
        const LV2_Atom* bpmAtom = nullptr;
        const LV2_Atom* speedAtom = nullptr;

        lv2_atom_object_get(obj,
                            urids.time_bar, &barAtom,
                            urids.time_barBeat, &barBeatAtom,
                            urids.time_beatsPerBar, &beatsPerBarAtom,
                            urids.time_beatsPerMinute, &bpmAtom,
                            urids.time_speed, &speedAtom,
                            0);

        double value = 0.0;
        if (atom_to_double(barAtom, urids, &value)) {
            local.bar = value;
        }
        if (atom_to_double(barBeatAtom, urids, &value)) {
            local.barBeat = value;
        }
        if (atom_to_double(beatsPerBarAtom, urids, &value) && value > 0.0) {
            local.beatsPerBar = value;
        }
        if (atom_to_double(bpmAtom, urids, &value) && value > 0.0) {
            local.bpm = value;
        }
        if (atom_to_double(speedAtom, urids, &value)) {
            local.playing = value > 0.0;
        }
    }

    if (found) {
        local.valid = true;
        *info = local;
    }

    return found;
}

static bool euclid_hit(int step, int pulses, int offset, int length) {
    if (length <= 0 || pulses <= 0) return false;
    if (pulses >= length) return true;
    const int base = (step + (offset % length)) % length;
    return ((base * pulses) % length) < pulses;
}

static void update_cached_params(EMix* self) {
    if (!self) {
        return;
    }
    self->cached_total_bars = self->total_bars_port ? *self->total_bars_port : self->cached_total_bars;
    self->cached_division = self->division_port ? *self->division_port : self->cached_division;
    self->cached_steps = self->steps_port ? *self->steps_port : self->cached_steps;
    self->cached_offset = self->offset_port ? *self->offset_port : self->cached_offset;
    self->cached_fade = self->fade_port ? *self->fade_port : self->cached_fade;
}

static LV2_State_Status emix_state_save(
    LV2_Handle instance,
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t /*flags*/,
    const LV2_Feature* const* /*features*/) {
    EMix* self = static_cast<EMix*>(instance);
    if (!self || !store) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    update_cached_params(self);

    const uint32_t flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;
    store(handle, self->urids.state_total_bars,
          &self->cached_total_bars, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_division,
          &self->cached_division, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_steps,
          &self->cached_steps, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_offset,
          &self->cached_offset, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_fade,
          &self->cached_fade, sizeof(float), self->urids.atom_Float, flags);

    return LV2_STATE_SUCCESS;
}

static LV2_State_Status emix_state_restore(
    LV2_Handle instance,
    LV2_State_Retrieve_Function retrieve,
    LV2_State_Handle handle,
    uint32_t /*flags*/,
    const LV2_Feature* const* /*features*/) {
    EMix* self = static_cast<EMix*>(instance);
    if (!self || !retrieve) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    auto restore_value = [&](LV2_URID key, float* target) {
        size_t size = 0;
        uint32_t type = 0;
        uint32_t val_flags = 0;
        const void* data = retrieve(handle, key, &size, &type, &val_flags);
        if (!data || size < sizeof(float)) {
            return;
        }
        if (type != self->urids.atom_Float) {
            return;
        }
        *target = *static_cast<const float*>(data);
    };

    restore_value(self->urids.state_total_bars, &self->cached_total_bars);
    restore_value(self->urids.state_division, &self->cached_division);
    restore_value(self->urids.state_steps, &self->cached_steps);
    restore_value(self->urids.state_offset, &self->cached_offset);
    restore_value(self->urids.state_fade, &self->cached_fade);

    return LV2_STATE_SUCCESS;
}

static const LV2_State_Interface emix_state_interface = {
    emix_state_save,
    emix_state_restore
};

static LV2_Handle instantiate(const LV2_Descriptor* /*descriptor*/,
                              double rate,
                              const char* /*bundle_path*/,
                              const LV2_Feature* const* features) {
    EMix* self = new EMix();
    if (!self) {
        return nullptr;
    }

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
    self->urids.atom_Blank = self->map->map(self->map->handle, LV2_ATOM__Blank);
    self->urids.atom_Float = self->map->map(self->map->handle, LV2_ATOM__Float);
    self->urids.atom_Double = self->map->map(self->map->handle, LV2_ATOM__Double);
    self->urids.atom_Int = self->map->map(self->map->handle, LV2_ATOM__Int);
    self->urids.atom_Long = self->map->map(self->map->handle, LV2_ATOM__Long);

    self->urids.time_Position = self->map->map(self->map->handle, LV2_TIME__Position);
    self->urids.time_speed = self->map->map(self->map->handle, LV2_TIME__speed);
    self->urids.time_bar = self->map->map(self->map->handle, LV2_TIME__bar);
    self->urids.time_barBeat = self->map->map(self->map->handle, LV2_TIME__barBeat);
    self->urids.time_beatsPerBar = self->map->map(self->map->handle, LV2_TIME__beatsPerBar);
    self->urids.time_beatsPerMinute = self->map->map(self->map->handle, LV2_TIME__beatsPerMinute);

    self->urids.state_total_bars = self->map->map(self->map->handle, EMIX_URI "#total_bars");
    self->urids.state_division = self->map->map(self->map->handle, EMIX_URI "#division");
    self->urids.state_steps = self->map->map(self->map->handle, EMIX_URI "#steps");
    self->urids.state_offset = self->map->map(self->map->handle, EMIX_URI "#offset");
    self->urids.state_fade = self->map->map(self->map->handle, EMIX_URI "#fade");

    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    EMix* self = static_cast<EMix*>(instance);
    if (!self) {
        return;
    }

    if (port == PORT_CONTROL) {
        self->control = static_cast<const LV2_Atom_Sequence*>(data);
        return;
    }

    if (port >= PORT_AUDIO_IN_1 && port <= PORT_AUDIO_IN_8) {
        self->audio_in[port - PORT_AUDIO_IN_1] = static_cast<const float*>(data);
        return;
    }

    if (port >= PORT_AUDIO_OUT_1 && port <= PORT_AUDIO_OUT_8) {
        self->audio_out[port - PORT_AUDIO_OUT_1] = static_cast<float*>(data);
        return;
    }

    switch (port) {
        case PORT_TOTAL_BARS:
            self->total_bars_port = static_cast<const float*>(data);
            break;
        case PORT_DIVISION:
            self->division_port = static_cast<const float*>(data);
            break;
        case PORT_STEPS:
            self->steps_port = static_cast<const float*>(data);
            break;
        case PORT_OFFSET:
            self->offset_port = static_cast<const float*>(data);
            break;
        case PORT_FADE:
            self->fade_port = static_cast<const float*>(data);
            break;
        default:
            break;
    }
}

static void activate(LV2_Handle instance) {
    EMix* self = static_cast<EMix*>(instance);
    if (!self) {
        return;
    }
    self->transport_was_playing = false;
    self->cycle_origin_bar = 0.0;
    self->fallback_bar_pos = 0.0;
    self->fallback_bpm = 120.0;
    self->fallback_beats_per_bar = 4.0;
}

static void run(LV2_Handle instance, uint32_t nframes) {
    EMix* self = static_cast<EMix*>(instance);
    if (!self || nframes == 0) {
        return;
    }

    update_cached_params(self);

    const int total_bars = clampi(static_cast<int>(std::lround(self->cached_total_bars)), 1, 4096);
    const int division = clampi(static_cast<int>(std::lround(self->cached_division)), 1, 512);
    const int steps = clampi(static_cast<int>(std::lround(self->cached_steps)), 0, division);
    const int offset = clampi(static_cast<int>(std::lround(self->cached_offset)), 0, division - 1);
    const float fade_bars = clampf(self->cached_fade, 0.0f, static_cast<float>(total_bars));

    TimeInfo time_info;
    time_info.bpm = 120.0;
    time_info.beatsPerBar = 4.0;
    time_info.playing = false;
    time_info.bar = 0.0;
    time_info.barBeat = 0.0;
    time_info.valid = false;

    read_time_info(self->control, self->urids, &time_info);

    double bar_pos = 0.0;
    double bar_step = 0.0;
    const bool host_playing = time_info.valid && time_info.playing && time_info.bpm > 0.0 && time_info.beatsPerBar > 0.0;
    if (host_playing) {
        bar_pos = time_info.bar + (time_info.barBeat / time_info.beatsPerBar);
        bar_step = time_info.bpm / (60.0 * self->sample_rate * time_info.beatsPerBar);
        self->fallback_bpm = time_info.bpm;
        self->fallback_beats_per_bar = time_info.beatsPerBar;
        self->fallback_bar_pos = bar_pos;

        if (!self->transport_was_playing) {
            self->cycle_origin_bar = bar_pos;
        }
        self->transport_was_playing = true;
    } else {
        self->transport_was_playing = false;
        bar_pos = self->fallback_bar_pos;
        bar_step = self->fallback_bpm / (60.0 * self->sample_rate * self->fallback_beats_per_bar);
    }

    const double cycle_bars = static_cast<double>(total_bars);
    const double block_bars = cycle_bars / static_cast<double>(division);

    for (uint32_t i = 0; i < nframes; ++i) {
        float gain = 1.0f;

        const double rel_bar = bar_pos - self->cycle_origin_bar;
        double cycle_pos = std::fmod(rel_bar, cycle_bars);
        if (cycle_pos < 0.0) {
            cycle_pos += cycle_bars;
        }

        int block_index = static_cast<int>(std::floor(cycle_pos / block_bars));
        block_index = clampi(block_index, 0, division - 1);

        const bool active = euclid_hit(block_index, steps, offset, division);
        if (!active) {
            gain = 0.0f;
        } else {
            const double block_start = static_cast<double>(block_index) * block_bars;
            const double local_bar = cycle_pos - block_start;
            if (fade_bars > 0.0f) {
                const double fade = static_cast<double>(fade_bars);
                const double in_gain = clampf(static_cast<float>(local_bar / fade), 0.0f, 1.0f);
                const double out_gain = clampf(static_cast<float>((block_bars - local_bar) / fade), 0.0f, 1.0f);
                gain = static_cast<float>(in_gain < out_gain ? in_gain : out_gain);
                }
        }
        bar_pos += bar_step;

        for (uint32_t ch = 0; ch < kMaxChannels; ++ch) {
            float* out = self->audio_out[ch];
            if (!out) {
                continue;
            }
            const float* in = self->audio_in[ch];
            const float sample = in ? in[i] : 0.0f;
            out[i] = sample * gain;
        }
    }

    self->fallback_bar_pos = bar_pos;
}

static void deactivate(LV2_Handle /*instance*/) {
}

static void cleanup(LV2_Handle instance) {
    EMix* self = static_cast<EMix*>(instance);
    delete self;
}

static const void* extension_data(const char* uri) {
    if (!std::strcmp(uri, LV2_STATE__interface)) {
        return &emix_state_interface;
    }
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    EMIX_URI,
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
