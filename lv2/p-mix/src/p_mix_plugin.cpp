#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>
#include <lv2/time/time.h>
#include <lv2/state/state.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#define PMIX_URI "https://danja.github.io/flues/plugins/p-mix"

static constexpr uint32_t kMaxChannels = 8;
static constexpr float kFadeMinFraction = 0.125f;

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
    PORT_GRANULARITY,
    PORT_MAINTAIN,
    PORT_FADE,
    PORT_CUT,
    PORT_FADE_DUR_MAX,
    PORT_BIAS,
    PORT_TOTAL_COUNT
};

struct PMixURIDs {
    LV2_URID atom_Object = 0;
    LV2_URID atom_Float = 0;
    LV2_URID atom_Double = 0;
    LV2_URID atom_Int = 0;
    LV2_URID atom_Long = 0;
    LV2_URID atom_Sequence = 0;
    LV2_URID time_Position = 0;
    LV2_URID time_speed = 0;
    LV2_URID time_bar = 0;
    LV2_URID time_barBeat = 0;
    LV2_URID time_beatsPerBar = 0;
    LV2_URID time_beatsPerMinute = 0;
    LV2_URID state_granularity = 0;
    LV2_URID state_maintain = 0;
    LV2_URID state_fade = 0;
    LV2_URID state_cut = 0;
    LV2_URID state_fade_dur_max = 0;
    LV2_URID state_bias = 0;
};

struct TimeInfo {
    bool valid = false;
    bool playing = false;
    double bar = 0.0;
    double barBeat = 0.0;
    double beatsPerBar = 4.0;
    double bpm = 120.0;
};

struct PMix {
    const LV2_Atom_Sequence* control = nullptr;
    const float* audio_in[kMaxChannels] = {nullptr};
    float* audio_out[kMaxChannels] = {nullptr};

    const float* granularity_port = nullptr;
    const float* maintain_port = nullptr;
    const float* fade_port = nullptr;
    const float* cut_port = nullptr;
    const float* fade_dur_max_port = nullptr;
    const float* bias_port = nullptr;

    LV2_URID_Map* map = nullptr;
    PMixURIDs urids{};

    double sample_rate = 48000.0;

    float current_gain = 1.0f;
    float target_gain = 1.0f;
    float fade_step = 0.0f;
    uint32_t fade_remaining = 0;

    uint32_t rng_state = 0x12345678u;

    float cached_granularity = 4.0f;
    float cached_maintain = 50.0f;
    float cached_fade = 25.0f;
    float cached_cut = 25.0f;
    float cached_fade_dur_max = 1.0f;
    float cached_bias = 50.0f;
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

static inline float rand_float(uint32_t* state) {
    *state = (*state * 1664525u) + 1013904223u;
    return static_cast<float>(*state & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

static bool atom_to_double(const LV2_Atom* atom, const PMixURIDs& urids, double* out) {
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

static bool read_time_info(const LV2_Atom_Sequence* control, const PMixURIDs& urids, TimeInfo* info) {
    if (!control || !info) {
        return false;
    }

    bool found = false;
    TimeInfo local = *info;

    LV2_ATOM_SEQUENCE_FOREACH(control, ev) {
        const LV2_Atom_Object* obj = nullptr;
        if (ev->body.type == urids.time_Position) {
            obj = reinterpret_cast<const LV2_Atom_Object*>(&ev->body);
        } else if (ev->body.type == urids.atom_Object) {
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
        if (atom_to_double(beatsPerBarAtom, urids, &value)) {
            local.beatsPerBar = value;
        }
        if (atom_to_double(bpmAtom, urids, &value)) {
            if (value > 0.0) {
                local.bpm = value;
            }
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

static void update_cached_params(PMix* self) {
    if (!self) {
        return;
    }
    self->cached_granularity = self->granularity_port ? *self->granularity_port : self->cached_granularity;
    self->cached_maintain = self->maintain_port ? *self->maintain_port : self->cached_maintain;
    self->cached_fade = self->fade_port ? *self->fade_port : self->cached_fade;
    self->cached_cut = self->cut_port ? *self->cut_port : self->cached_cut;
    self->cached_fade_dur_max = self->fade_dur_max_port ? *self->fade_dur_max_port : self->cached_fade_dur_max;
    self->cached_bias = self->bias_port ? *self->bias_port : self->cached_bias;
}

static void start_fade(PMix* self, float target_gain, int granularity, double frames_per_bar, float fade_dur_max) {
    if (!self || frames_per_bar <= 0.0) {
        return;
    }
    float max_fraction = clampf(fade_dur_max, kFadeMinFraction, 1.0f);
    float fraction = kFadeMinFraction;
    if (max_fraction > kFadeMinFraction) {
        fraction = kFadeMinFraction + rand_float(&self->rng_state) * (max_fraction - kFadeMinFraction);
    }
    const double frames = frames_per_bar * static_cast<double>(granularity) * static_cast<double>(fraction);
    uint32_t total_frames = frames > 1.0 ? static_cast<uint32_t>(std::llround(frames)) : 1u;
    self->fade_remaining = total_frames;
    self->target_gain = target_gain;
    self->fade_step = (target_gain - self->current_gain) / static_cast<float>(total_frames);
}

static void apply_transition(PMix* self,
                             int64_t bar,
                             int granularity,
                             double frames_per_bar,
                             float maintain,
                             float fade,
                             float cut,
                             float fade_dur_max,
                             float bias) {
    if (!self || granularity <= 0) {
        return;
    }

    if ((bar % granularity) != 0) {
        return;
    }

    if (self->fade_remaining > 0) {
        return;
    }

    float gain = self->current_gain;
    if (gain > 0.001f && gain < 0.999f) {
        gain = (gain < 0.5f) ? 0.0f : 1.0f;
        self->current_gain = gain;
    }

    float p_maintain = maintain;
    float p_fade = fade;
    float p_cut = cut;
    float sum = p_maintain + p_fade + p_cut;
    if (sum <= 0.0001f) {
        p_maintain = 1.0f;
        p_fade = 0.0f;
        p_cut = 0.0f;
        sum = 1.0f;
    }
    p_maintain /= sum;
    p_fade /= sum;
    p_cut /= sum;

    const float r = rand_float(&self->rng_state);
    if (r < p_maintain) {
        return;
    }

    const float bias_norm = clampf(bias * 0.01f, 0.0f, 1.0f);
    const float target = (rand_float(&self->rng_state) < bias_norm) ? 1.0f : 0.0f;

    if (r < (p_maintain + p_fade)) {
        start_fade(self, target, granularity, frames_per_bar, fade_dur_max);
    } else {
        self->current_gain = target;
        self->target_gain = target;
        self->fade_remaining = 0;
        self->fade_step = 0.0f;
    }
}

static LV2_State_Status pmix_state_save(
    LV2_Handle instance,
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t /*flags*/,
    const LV2_Feature* const* /*features*/) {
    PMix* self = static_cast<PMix*>(instance);
    if (!self || !store) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    update_cached_params(self);

    const uint32_t flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;

    store(handle, self->urids.state_granularity,
          &self->cached_granularity, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_maintain,
          &self->cached_maintain, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_fade,
          &self->cached_fade, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_cut,
          &self->cached_cut, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_fade_dur_max,
          &self->cached_fade_dur_max, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_bias,
          &self->cached_bias, sizeof(float), self->urids.atom_Float, flags);

    return LV2_STATE_SUCCESS;
}

static LV2_State_Status pmix_state_restore(
    LV2_Handle instance,
    LV2_State_Retrieve_Function retrieve,
    LV2_State_Handle handle,
    uint32_t /*flags*/,
    const LV2_Feature* const* /*features*/) {
    PMix* self = static_cast<PMix*>(instance);
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

    restore_value(self->urids.state_granularity, &self->cached_granularity);
    restore_value(self->urids.state_maintain, &self->cached_maintain);
    restore_value(self->urids.state_fade, &self->cached_fade);
    restore_value(self->urids.state_cut, &self->cached_cut);
    restore_value(self->urids.state_fade_dur_max, &self->cached_fade_dur_max);
    restore_value(self->urids.state_bias, &self->cached_bias);

    return LV2_STATE_SUCCESS;
}

static const LV2_State_Interface pmix_state_interface = {
    pmix_state_save,
    pmix_state_restore
};

static LV2_Handle instantiate(const LV2_Descriptor* /*descriptor*/,
                              double rate,
                              const char* /*bundle_path*/,
                              const LV2_Feature* const* features) {
    PMix* self = new PMix();
    if (!self) {
        return nullptr;
    }

    self->sample_rate = rate;
    self->rng_state ^= static_cast<uint32_t>(reinterpret_cast<uintptr_t>(self) >> 4);

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
    self->urids.atom_Float = self->map->map(self->map->handle, LV2_ATOM__Float);
    self->urids.atom_Double = self->map->map(self->map->handle, LV2_ATOM__Double);
    self->urids.atom_Int = self->map->map(self->map->handle, LV2_ATOM__Int);
    self->urids.atom_Long = self->map->map(self->map->handle, LV2_ATOM__Long);
    self->urids.atom_Sequence = self->map->map(self->map->handle, LV2_ATOM__Sequence);

    self->urids.time_Position = self->map->map(self->map->handle, LV2_TIME__Position);
    self->urids.time_speed = self->map->map(self->map->handle, LV2_TIME__speed);
    self->urids.time_bar = self->map->map(self->map->handle, LV2_TIME__bar);
    self->urids.time_barBeat = self->map->map(self->map->handle, LV2_TIME__barBeat);
    self->urids.time_beatsPerBar = self->map->map(self->map->handle, LV2_TIME__beatsPerBar);
    self->urids.time_beatsPerMinute = self->map->map(self->map->handle, LV2_TIME__beatsPerMinute);

    self->urids.state_granularity = self->map->map(self->map->handle, PMIX_URI "#granularity");
    self->urids.state_maintain = self->map->map(self->map->handle, PMIX_URI "#maintain");
    self->urids.state_fade = self->map->map(self->map->handle, PMIX_URI "#fade");
    self->urids.state_cut = self->map->map(self->map->handle, PMIX_URI "#cut");
    self->urids.state_fade_dur_max = self->map->map(self->map->handle, PMIX_URI "#fade_dur_max");
    self->urids.state_bias = self->map->map(self->map->handle, PMIX_URI "#bias");

    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    PMix* self = static_cast<PMix*>(instance);
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
        case PORT_GRANULARITY:
            self->granularity_port = static_cast<const float*>(data);
            break;
        case PORT_MAINTAIN:
            self->maintain_port = static_cast<const float*>(data);
            break;
        case PORT_FADE:
            self->fade_port = static_cast<const float*>(data);
            break;
        case PORT_CUT:
            self->cut_port = static_cast<const float*>(data);
            break;
        case PORT_FADE_DUR_MAX:
            self->fade_dur_max_port = static_cast<const float*>(data);
            break;
        case PORT_BIAS:
            self->bias_port = static_cast<const float*>(data);
            break;
        default:
            break;
    }
}

static void activate(LV2_Handle instance) {
    PMix* self = static_cast<PMix*>(instance);
    if (!self) {
        return;
    }
    self->current_gain = 1.0f;
    self->target_gain = 1.0f;
    self->fade_remaining = 0;
    self->fade_step = 0.0f;
}

static void run(LV2_Handle instance, uint32_t nframes) {
    PMix* self = static_cast<PMix*>(instance);
    if (!self || nframes == 0) {
        return;
    }

    update_cached_params(self);

    const int granularity = clampi(static_cast<int>(std::lround(self->cached_granularity)), 1, 32);
    const float maintain = clampf(self->cached_maintain, 0.0f, 100.0f);
    const float fade = clampf(self->cached_fade, 0.0f, 100.0f);
    const float cut = clampf(self->cached_cut, 0.0f, 100.0f);
    const float fade_dur_max = clampf(self->cached_fade_dur_max, kFadeMinFraction, 1.0f);
    const float bias = clampf(self->cached_bias, 0.0f, 100.0f);

    TimeInfo time_info;
    time_info.bpm = 120.0;
    time_info.beatsPerBar = 4.0;
    time_info.playing = true;
    time_info.bar = 0.0;
    time_info.barBeat = 0.0;
    time_info.valid = false;

    read_time_info(self->control, self->urids, &time_info);

    double frames_per_bar = 0.0;
    if (time_info.valid && time_info.playing && time_info.bpm > 0.0) {
        const double frames_per_beat = self->sample_rate * 60.0 / time_info.bpm;
        frames_per_bar = frames_per_beat * time_info.beatsPerBar;
    }

    struct Boundary {
        uint32_t frame;
        int64_t bar;
    };

    Boundary boundaries[16];
    size_t boundary_count = 0;
    size_t boundary_index = 0;

    if (time_info.valid && time_info.playing && frames_per_bar > 0.0) {
        const double eps = 1e-6;
        int64_t bar = static_cast<int64_t>(std::llround(time_info.bar));
        double bar_beat = time_info.barBeat;
        if (bar_beat < 0.0) {
            bar_beat = 0.0;
        }

        if (bar_beat <= eps && boundary_count < sizeof(boundaries) / sizeof(boundaries[0])) {
            boundaries[boundary_count++] = {0u, bar};
        }

        double frames_to_next = (time_info.beatsPerBar - bar_beat) * (frames_per_bar / time_info.beatsPerBar);
        if (frames_to_next <= eps) {
            frames_to_next = frames_per_bar;
        }

        double frame_pos = frames_to_next;
        while (frame_pos < static_cast<double>(nframes) && boundary_count < sizeof(boundaries) / sizeof(boundaries[0])) {
            bar += 1;
            boundaries[boundary_count++] = {static_cast<uint32_t>(std::lround(frame_pos)), bar};
            frame_pos += frames_per_bar;
        }
    }

    for (uint32_t i = 0; i < nframes; ++i) {
        if (boundary_index < boundary_count && boundaries[boundary_index].frame == i) {
            apply_transition(self, boundaries[boundary_index].bar, granularity, frames_per_bar,
                             maintain, fade, cut, fade_dur_max, bias);
            boundary_index++;
        }

        if (self->fade_remaining > 0) {
            self->current_gain += self->fade_step;
            self->fade_remaining--;
            if (self->fade_remaining == 0) {
                self->current_gain = self->target_gain;
                self->fade_step = 0.0f;
            }
        }

        const float gain = self->current_gain;
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
}

static void deactivate(LV2_Handle /*instance*/) {
}

static void cleanup(LV2_Handle instance) {
    PMix* self = static_cast<PMix*>(instance);
    delete self;
}

static const void* extension_data(const char* uri) {
    if (!std::strcmp(uri, LV2_STATE__interface)) {
        return &pmix_state_interface;
    }
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    PMIX_URI,
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
