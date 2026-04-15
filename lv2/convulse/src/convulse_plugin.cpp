#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/state/state.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#include "ConvulseEngine.hpp"

#define CONVULSE_URI "https://danja.github.io/flues/plugins/convulse"

using flues::convulse::Engine;
using flues::convulse::Params;

enum PortIndex {
    PORT_CONTROL = 0,
    PORT_AUDIO_IN_L,
    PORT_AUDIO_IN_R,
    PORT_AUDIO_OUT_L,
    PORT_AUDIO_OUT_R,
    PORT_DRY_WET,
    PORT_KERNEL_SIZE,
    PORT_MODE,
    PORT_PITCH,
    PORT_SHAPE,
    PORT_DECAY,
    PORT_REFRESH,
    PORT_STEREO_WIDTH,
    PORT_FEEDBACK,
    PORT_DRIVE,
    PORT_TOTAL_COUNT
};

struct ConvulseURIDs {
    LV2_URID atom_Object = 0;
    LV2_URID atom_Float = 0;
    LV2_URID atom_Double = 0;
    LV2_URID atom_Int = 0;
    LV2_URID atom_Long = 0;
    LV2_URID atom_Sequence = 0;

    LV2_URID time_Position = 0;
    LV2_URID time_speed = 0;
    LV2_URID time_beatsPerMinute = 0;

    LV2_URID state_dry_wet = 0;
    LV2_URID state_kernel_size = 0;
    LV2_URID state_mode = 0;
    LV2_URID state_pitch = 0;
    LV2_URID state_shape = 0;
    LV2_URID state_decay = 0;
    LV2_URID state_refresh = 0;
    LV2_URID state_stereo_width = 0;
    LV2_URID state_feedback = 0;
    LV2_URID state_drive = 0;
};

struct TimeInfo {
    bool valid = false;
    bool playing = false;
    double bpm = 120.0;
};

struct Convulse {
    const LV2_Atom_Sequence* control = nullptr;
    const float* audio_in[2] = {nullptr, nullptr};
    float* audio_out[2] = {nullptr, nullptr};

    const float* dry_wet_port = nullptr;
    const float* kernel_size_port = nullptr;
    const float* mode_port = nullptr;
    const float* pitch_port = nullptr;
    const float* shape_port = nullptr;
    const float* decay_port = nullptr;
    const float* refresh_port = nullptr;
    const float* stereo_width_port = nullptr;
    const float* feedback_port = nullptr;
    const float* drive_port = nullptr;

    LV2_URID_Map* map = nullptr;
    ConvulseURIDs urids{};
    double sample_rate = 48000.0;
    Engine engine{};

    float cached_dry_wet = 0.5f;
    float cached_kernel_size = 256.0f;
    float cached_mode = 0.0f;
    float cached_pitch = 220.0f;
    float cached_shape = 0.5f;
    float cached_decay = 0.6f;
    float cached_refresh = 0.5f;
    float cached_stereo_width = 0.3f;
    float cached_feedback = 10.0f;
    float cached_drive = 0.35f;
};

static inline float clampf(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static bool atom_to_double(const LV2_Atom* atom, const ConvulseURIDs& urids, double* out) {
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

static void read_time_info(const LV2_Atom_Sequence* control, const ConvulseURIDs& urids, TimeInfo* info) {
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

        const LV2_Atom* bpmAtom = nullptr;
        const LV2_Atom* speedAtom = nullptr;

        lv2_atom_object_get(obj,
                            urids.time_beatsPerMinute, &bpmAtom,
                            urids.time_speed, &speedAtom,
                            0);

        double value = 0.0;
        if (atom_to_double(bpmAtom, urids, &value) && value > 0.0) {
            local.bpm = value;
            local.valid = true;
        }
        if (atom_to_double(speedAtom, urids, &value)) {
            local.playing = value > 0.0;
        }
    }

    *info = local;
}

static void update_cached_params(Convulse* self) {
    if (!self) {
        return;
    }
    if (self->dry_wet_port) self->cached_dry_wet = *self->dry_wet_port;
    if (self->kernel_size_port) self->cached_kernel_size = *self->kernel_size_port;
    if (self->mode_port) self->cached_mode = *self->mode_port;
    if (self->pitch_port) self->cached_pitch = *self->pitch_port;
    if (self->shape_port) self->cached_shape = *self->shape_port;
    if (self->decay_port) self->cached_decay = *self->decay_port;
    if (self->refresh_port) self->cached_refresh = *self->refresh_port;
    if (self->stereo_width_port) self->cached_stereo_width = *self->stereo_width_port;
    if (self->feedback_port) self->cached_feedback = *self->feedback_port;
    if (self->drive_port) self->cached_drive = *self->drive_port;
}

static LV2_State_Status convulse_state_save(
    LV2_Handle instance,
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t,
    const LV2_Feature* const*) {
    Convulse* self = static_cast<Convulse*>(instance);
    if (!self || !store) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    update_cached_params(self);

    const uint32_t flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;
    store(handle, self->urids.state_dry_wet,
          &self->cached_dry_wet, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_kernel_size,
          &self->cached_kernel_size, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_mode,
          &self->cached_mode, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_pitch,
          &self->cached_pitch, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_shape,
          &self->cached_shape, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_decay,
          &self->cached_decay, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_refresh,
          &self->cached_refresh, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_stereo_width,
          &self->cached_stereo_width, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_feedback,
          &self->cached_feedback, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_drive,
          &self->cached_drive, sizeof(float), self->urids.atom_Float, flags);

    return LV2_STATE_SUCCESS;
}

static LV2_State_Status convulse_state_restore(
    LV2_Handle instance,
    LV2_State_Retrieve_Function retrieve,
    LV2_State_Handle handle,
    uint32_t,
    const LV2_Feature* const*) {
    Convulse* self = static_cast<Convulse*>(instance);
    if (!self || !retrieve) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    auto restore_value = [&](LV2_URID key, float* target) {
        size_t size = 0;
        uint32_t type = 0;
        uint32_t valFlags = 0;
        const void* data = retrieve(handle, key, &size, &type, &valFlags);
        if (!data || size < sizeof(float) || type != self->urids.atom_Float) {
            return;
        }
        *target = *static_cast<const float*>(data);
    };

    restore_value(self->urids.state_dry_wet, &self->cached_dry_wet);
    restore_value(self->urids.state_kernel_size, &self->cached_kernel_size);
    restore_value(self->urids.state_mode, &self->cached_mode);
    restore_value(self->urids.state_pitch, &self->cached_pitch);
    restore_value(self->urids.state_shape, &self->cached_shape);
    restore_value(self->urids.state_decay, &self->cached_decay);
    restore_value(self->urids.state_refresh, &self->cached_refresh);
    restore_value(self->urids.state_stereo_width, &self->cached_stereo_width);
    restore_value(self->urids.state_feedback, &self->cached_feedback);
    restore_value(self->urids.state_drive, &self->cached_drive);

    return LV2_STATE_SUCCESS;
}

static const LV2_State_Interface convulse_state_interface = {
    convulse_state_save,
    convulse_state_restore
};

static LV2_Handle instantiate(const LV2_Descriptor*,
                              double rate,
                              const char*,
                              const LV2_Feature* const* features) {
    Convulse* self = new Convulse();
    if (!self) {
        return nullptr;
    }

    self->sample_rate = rate;
    self->engine.setSampleRate(rate);

    for (int i = 0; features && features[i]; ++i) {
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
    self->urids.time_beatsPerMinute = self->map->map(self->map->handle, LV2_TIME__beatsPerMinute);

    self->urids.state_dry_wet = self->map->map(self->map->handle, CONVULSE_URI "#dry_wet");
    self->urids.state_kernel_size = self->map->map(self->map->handle, CONVULSE_URI "#kernel_size");
    self->urids.state_mode = self->map->map(self->map->handle, CONVULSE_URI "#mode");
    self->urids.state_pitch = self->map->map(self->map->handle, CONVULSE_URI "#pitch");
    self->urids.state_shape = self->map->map(self->map->handle, CONVULSE_URI "#shape");
    self->urids.state_decay = self->map->map(self->map->handle, CONVULSE_URI "#decay");
    self->urids.state_refresh = self->map->map(self->map->handle, CONVULSE_URI "#refresh");
    self->urids.state_stereo_width = self->map->map(self->map->handle, CONVULSE_URI "#stereo_width");
    self->urids.state_feedback = self->map->map(self->map->handle, CONVULSE_URI "#feedback");
    self->urids.state_drive = self->map->map(self->map->handle, CONVULSE_URI "#drive");

    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    Convulse* self = static_cast<Convulse*>(instance);
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
        case PORT_DRY_WET:
            self->dry_wet_port = static_cast<const float*>(data);
            break;
        case PORT_KERNEL_SIZE:
            self->kernel_size_port = static_cast<const float*>(data);
            break;
        case PORT_MODE:
            self->mode_port = static_cast<const float*>(data);
            break;
        case PORT_PITCH:
            self->pitch_port = static_cast<const float*>(data);
            break;
        case PORT_SHAPE:
            self->shape_port = static_cast<const float*>(data);
            break;
        case PORT_DECAY:
            self->decay_port = static_cast<const float*>(data);
            break;
        case PORT_REFRESH:
            self->refresh_port = static_cast<const float*>(data);
            break;
        case PORT_STEREO_WIDTH:
            self->stereo_width_port = static_cast<const float*>(data);
            break;
        case PORT_FEEDBACK:
            self->feedback_port = static_cast<const float*>(data);
            break;
        case PORT_DRIVE:
            self->drive_port = static_cast<const float*>(data);
            break;
        default:
            break;
    }
}

static void activate(LV2_Handle instance) {
    Convulse* self = static_cast<Convulse*>(instance);
    if (!self) {
        return;
    }
    self->engine.reset();
}

static void run(LV2_Handle instance, uint32_t nframes) {
    Convulse* self = static_cast<Convulse*>(instance);
    if (!self || nframes == 0) {
        return;
    }

    update_cached_params(self);

    Params params;
    params.dryWet = clampf(self->cached_dry_wet, 0.0f, 1.0f);
    params.kernelSize = clampf(self->cached_kernel_size, 32.0f, 1024.0f);
    params.mode = clampf(self->cached_mode, 0.0f, 5.0f);
    params.pitch = clampf(self->cached_pitch, 20.0f, 2000.0f);
    params.shape = clampf(self->cached_shape, 0.0f, 1.0f);
    params.decay = clampf(self->cached_decay, 0.0f, 1.0f);
    params.refresh = clampf(self->cached_refresh, 0.0f, 8.0f);
    params.stereoWidth = clampf(self->cached_stereo_width, 0.0f, 1.0f);
    params.feedback = clampf(self->cached_feedback, 0.0f, 95.0f);
    params.drive = clampf(self->cached_drive, 0.0f, 2.0f);

    TimeInfo timeInfo;
    read_time_info(self->control, self->urids, &timeInfo);

    self->engine.setParams(params);
    self->engine.processBlock(self->audio_in[0],
                              self->audio_in[1],
                              self->audio_out[0],
                              self->audio_out[1],
                              nframes,
                              timeInfo.playing,
                              timeInfo.valid,
                              timeInfo.bpm);
}

static void deactivate(LV2_Handle) {
}

static void cleanup(LV2_Handle instance) {
    Convulse* self = static_cast<Convulse*>(instance);
    delete self;
}

static const void* extension_data(const char* uri) {
    if (!std::strcmp(uri, LV2_STATE__interface)) {
        return &convulse_state_interface;
    }
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    CONVULSE_URI,
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
