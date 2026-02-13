#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>
#include <lv2/time/time.h>
#include <lv2/state/state.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#define EUDELAY_URI "https://danja.github.io/flues/plugins/eudelay"

static constexpr uint32_t kMaxChannels = 2;
static constexpr int kMinSteps = 2;
static constexpr int kMaxSteps = 24;
static constexpr float kMinScale = 0.125f;
static constexpr float kMaxScale = 8.0f;
static constexpr float kMaxFeedback = 0.98f;
static constexpr double kFallbackBpm = 120.0;
static constexpr double kMinBpm = 20.0;
static constexpr double kMaxDelaySeconds = 120.0;

enum PortIndex {
    PORT_CONTROL = 0,
    PORT_AUDIO_IN_L,
    PORT_AUDIO_IN_R,
    PORT_AUDIO_OUT_L,
    PORT_AUDIO_OUT_R,
    PORT_SCALE,
    PORT_STEPS,
    PORT_TAPS,
    PORT_OFFSET,
    PORT_FEEDBACK,
    PORT_MIX,
    PORT_TOTAL_COUNT
};

struct EuDelayURIDs {
    LV2_URID atom_Object = 0;
    LV2_URID atom_Float = 0;
    LV2_URID atom_Double = 0;
    LV2_URID atom_Int = 0;
    LV2_URID atom_Long = 0;
    LV2_URID time_Position = 0;
    LV2_URID time_speed = 0;
    LV2_URID time_beatsPerMinute = 0;

    LV2_URID state_scale = 0;
    LV2_URID state_steps = 0;
    LV2_URID state_taps = 0;
    LV2_URID state_offset = 0;
    LV2_URID state_feedback = 0;
    LV2_URID state_mix = 0;
};

struct TimeInfo {
    bool valid = false;
    bool playing = true;
    double bpm = kFallbackBpm;
};

struct EuDelay {
    const LV2_Atom_Sequence* control = nullptr;
    const float* audio_in[kMaxChannels] = {nullptr};
    float* audio_out[kMaxChannels] = {nullptr};

    const float* scale_port = nullptr;
    const float* steps_port = nullptr;
    const float* taps_port = nullptr;
    const float* offset_port = nullptr;
    const float* feedback_port = nullptr;
    const float* mix_port = nullptr;

    LV2_URID_Map* map = nullptr;
    EuDelayURIDs urids{};

    double sample_rate = 48000.0;

    std::vector<float> delay_buffer[kMaxChannels];
    uint32_t write_index = 0;

    float cached_scale = 1.0f;
    float cached_steps = 16.0f;
    float cached_taps = 5.0f;
    float cached_offset = 0.0f;
    float cached_feedback = 35.0f;
    float cached_mix = 35.0f;

    std::array<int, kMaxSteps> tap_step_index{};
    int tap_count = 0;
    int cached_pattern_steps = -1;
    int cached_pattern_taps = -1;
    int cached_pattern_offset = -1;
};

static inline float clampf(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static inline int clampi(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static bool atom_to_double(const LV2_Atom* atom, const EuDelayURIDs& urids, double* out) {
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

static void read_time_info(const LV2_Atom_Sequence* control, const EuDelayURIDs& urids, TimeInfo* info) {
    if (!control || !info) {
        return;
    }

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

        const LV2_Atom* bpm_atom = nullptr;
        const LV2_Atom* speed_atom = nullptr;

        lv2_atom_object_get(obj,
                            urids.time_beatsPerMinute, &bpm_atom,
                            urids.time_speed, &speed_atom,
                            0);

        double value = 0.0;
        if (atom_to_double(bpm_atom, urids, &value) && value > 0.0) {
            local.bpm = value;
            local.valid = true;
        }
        if (atom_to_double(speed_atom, urids, &value)) {
            local.playing = value > 0.0;
        }
    }

    *info = local;
}

static void update_cached_params(EuDelay* self) {
    if (!self) {
        return;
    }
    if (self->scale_port) self->cached_scale = *self->scale_port;
    if (self->steps_port) self->cached_steps = *self->steps_port;
    if (self->taps_port) self->cached_taps = *self->taps_port;
    if (self->offset_port) self->cached_offset = *self->offset_port;
    if (self->feedback_port) self->cached_feedback = *self->feedback_port;
    if (self->mix_port) self->cached_mix = *self->mix_port;
}

static void ensure_delay_buffers(EuDelay* self) {
    if (!self) {
        return;
    }
    const double capped_seconds = std::max(1.0, kMaxDelaySeconds);
    const size_t target_size = static_cast<size_t>(std::ceil(self->sample_rate * capped_seconds)) + 1;

    for (uint32_t ch = 0; ch < kMaxChannels; ++ch) {
        if (self->delay_buffer[ch].size() != target_size) {
            self->delay_buffer[ch].assign(target_size, 0.0f);
        }
    }
    if (self->write_index >= target_size) {
        self->write_index = 0;
    }
}

static void rebuild_pattern(EuDelay* self, int steps, int taps, int offset) {
    if (!self) {
        return;
    }
    if (self->cached_pattern_steps == steps &&
        self->cached_pattern_taps == taps &&
        self->cached_pattern_offset == offset) {
        return;
    }

    self->tap_step_index.fill(0);
    self->tap_count = 0;

    for (int k = 0; k < taps; ++k) {
        const int base_step = static_cast<int>(std::floor((static_cast<double>(k) * steps) / taps));
        int step = (base_step + offset) % steps;
        if (step < 0) {
            step += steps;
        }
        self->tap_step_index[self->tap_count++] = step;
    }

    std::sort(self->tap_step_index.begin(), self->tap_step_index.begin() + self->tap_count);
    const auto unique_end = std::unique(self->tap_step_index.begin(), self->tap_step_index.begin() + self->tap_count);
    self->tap_count = static_cast<int>(unique_end - self->tap_step_index.begin());

    self->cached_pattern_steps = steps;
    self->cached_pattern_taps = taps;
    self->cached_pattern_offset = offset;
}

static LV2_State_Status eudelay_state_save(
    LV2_Handle instance,
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t /*flags*/,
    const LV2_Feature* const* /*features*/) {
    EuDelay* self = static_cast<EuDelay*>(instance);
    if (!self || !store) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    update_cached_params(self);

    const uint32_t flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;

    store(handle, self->urids.state_scale,
          &self->cached_scale, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_steps,
          &self->cached_steps, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_taps,
          &self->cached_taps, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_offset,
          &self->cached_offset, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_feedback,
          &self->cached_feedback, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_mix,
          &self->cached_mix, sizeof(float), self->urids.atom_Float, flags);

    return LV2_STATE_SUCCESS;
}

static LV2_State_Status eudelay_state_restore(
    LV2_Handle instance,
    LV2_State_Retrieve_Function retrieve,
    LV2_State_Handle handle,
    uint32_t /*flags*/,
    const LV2_Feature* const* /*features*/) {
    EuDelay* self = static_cast<EuDelay*>(instance);
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

    restore_value(self->urids.state_scale, &self->cached_scale);
    restore_value(self->urids.state_steps, &self->cached_steps);
    restore_value(self->urids.state_taps, &self->cached_taps);
    restore_value(self->urids.state_offset, &self->cached_offset);
    restore_value(self->urids.state_feedback, &self->cached_feedback);
    restore_value(self->urids.state_mix, &self->cached_mix);

    return LV2_STATE_SUCCESS;
}

static const LV2_State_Interface eudelay_state_interface = {
    eudelay_state_save,
    eudelay_state_restore
};

static LV2_Handle instantiate(const LV2_Descriptor* /*descriptor*/,
                              double rate,
                              const char* /*bundle_path*/,
                              const LV2_Feature* const* features) {
    EuDelay* self = new EuDelay();
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
    self->urids.atom_Float = self->map->map(self->map->handle, LV2_ATOM__Float);
    self->urids.atom_Double = self->map->map(self->map->handle, LV2_ATOM__Double);
    self->urids.atom_Int = self->map->map(self->map->handle, LV2_ATOM__Int);
    self->urids.atom_Long = self->map->map(self->map->handle, LV2_ATOM__Long);

    self->urids.time_Position = self->map->map(self->map->handle, LV2_TIME__Position);
    self->urids.time_speed = self->map->map(self->map->handle, LV2_TIME__speed);
    self->urids.time_beatsPerMinute = self->map->map(self->map->handle, LV2_TIME__beatsPerMinute);

    self->urids.state_scale = self->map->map(self->map->handle, EUDELAY_URI "#scale");
    self->urids.state_steps = self->map->map(self->map->handle, EUDELAY_URI "#steps");
    self->urids.state_taps = self->map->map(self->map->handle, EUDELAY_URI "#taps");
    self->urids.state_offset = self->map->map(self->map->handle, EUDELAY_URI "#offset");
    self->urids.state_feedback = self->map->map(self->map->handle, EUDELAY_URI "#feedback");
    self->urids.state_mix = self->map->map(self->map->handle, EUDELAY_URI "#mix");

    ensure_delay_buffers(self);

    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    EuDelay* self = static_cast<EuDelay*>(instance);
    if (!self) {
        return;
    }

    switch (port) {
        case PORT_CONTROL:
            self->control = static_cast<const LV2_Atom_Sequence*>(data);
            break;
        case PORT_AUDIO_IN_L:
            self->audio_in[0] = static_cast<const float*>(data);
            break;
        case PORT_AUDIO_IN_R:
            self->audio_in[1] = static_cast<const float*>(data);
            break;
        case PORT_AUDIO_OUT_L:
            self->audio_out[0] = static_cast<float*>(data);
            break;
        case PORT_AUDIO_OUT_R:
            self->audio_out[1] = static_cast<float*>(data);
            break;
        case PORT_SCALE:
            self->scale_port = static_cast<const float*>(data);
            break;
        case PORT_STEPS:
            self->steps_port = static_cast<const float*>(data);
            break;
        case PORT_TAPS:
            self->taps_port = static_cast<const float*>(data);
            break;
        case PORT_OFFSET:
            self->offset_port = static_cast<const float*>(data);
            break;
        case PORT_FEEDBACK:
            self->feedback_port = static_cast<const float*>(data);
            break;
        case PORT_MIX:
            self->mix_port = static_cast<const float*>(data);
            break;
        default:
            break;
    }
}

static void activate(LV2_Handle instance) {
    EuDelay* self = static_cast<EuDelay*>(instance);
    if (!self) {
        return;
    }

    ensure_delay_buffers(self);
    for (uint32_t ch = 0; ch < kMaxChannels; ++ch) {
        std::fill(self->delay_buffer[ch].begin(), self->delay_buffer[ch].end(), 0.0f);
    }

    self->write_index = 0;
    self->cached_pattern_steps = -1;
    self->cached_pattern_taps = -1;
    self->cached_pattern_offset = -1;
}

static void run(LV2_Handle instance, uint32_t nframes) {
    EuDelay* self = static_cast<EuDelay*>(instance);
    if (!self || nframes == 0) {
        return;
    }

    update_cached_params(self);

    const float scale = clampf(self->cached_scale, kMinScale, kMaxScale);
    const int steps = clampi(static_cast<int>(std::lround(self->cached_steps)), kMinSteps, kMaxSteps);
    const int taps = clampi(static_cast<int>(std::lround(self->cached_taps)), 1, steps);
    const int offset = clampi(static_cast<int>(std::lround(self->cached_offset)), 0, steps);
    const float feedback = clampf(self->cached_feedback, 0.0f, 100.0f);
    const float mix = clampf(self->cached_mix, 0.0f, 100.0f) * 0.01f;

    TimeInfo time_info;
    read_time_info(self->control, self->urids, &time_info);
    const double bpm = std::max(kMinBpm, time_info.bpm);

    const double step_seconds = static_cast<double>(scale) * (60.0 / bpm);
    const uint32_t step_samples = std::max<uint32_t>(1u, static_cast<uint32_t>(std::lround(step_seconds * self->sample_rate)));

    rebuild_pattern(self, steps, taps, offset);

    const size_t buffer_size = self->delay_buffer[0].size();
    if (buffer_size < 2) {
        return;
    }

    std::array<uint32_t, kMaxSteps> tap_delays{};
    int active_taps = 0;
    for (int i = 0; i < self->tap_count; ++i) {
        const uint64_t raw_delay = static_cast<uint64_t>(self->tap_step_index[i]) * step_samples;
        const uint32_t max_delay = static_cast<uint32_t>(buffer_size - 1);
        tap_delays[active_taps++] = static_cast<uint32_t>(std::min<uint64_t>(raw_delay, max_delay));
    }

    if (active_taps <= 0) {
        tap_delays[0] = step_samples;
        active_taps = 1;
    }

    const float wet_norm = 1.0f / static_cast<float>(active_taps);
    const float feedback_gain = clampf((feedback * 0.01f), 0.0f, kMaxFeedback);

    for (uint32_t i = 0; i < nframes; ++i) {
        for (uint32_t ch = 0; ch < kMaxChannels; ++ch) {
            float* out = self->audio_out[ch];
            if (!out) {
                continue;
            }

            const float* in = self->audio_in[ch];
            const float dry = in ? in[i] : 0.0f;

            const auto& buffer = self->delay_buffer[ch];
            float tap_sum = 0.0f;
            for (int t = 0; t < active_taps; ++t) {
                const uint32_t delay = tap_delays[t];
                const uint32_t read_index = (self->write_index + buffer_size - delay) % buffer_size;
                tap_sum += buffer[read_index];
            }

            const float wet = tap_sum * wet_norm;
            const float write_sample = dry + (wet * feedback_gain);

            self->delay_buffer[ch][self->write_index] = write_sample;
            out[i] = (dry * (1.0f - mix)) + (wet * mix);
        }

        self->write_index = (self->write_index + 1) % buffer_size;
    }
}

static void deactivate(LV2_Handle /*instance*/) {
}

static void cleanup(LV2_Handle instance) {
    EuDelay* self = static_cast<EuDelay*>(instance);
    delete self;
}

static const void* extension_data(const char* uri) {
    if (!std::strcmp(uri, LV2_STATE__interface)) {
        return &eudelay_state_interface;
    }
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    EUDELAY_URI,
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
