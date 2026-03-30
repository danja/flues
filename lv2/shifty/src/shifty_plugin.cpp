#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#define SHIFTY_URI "https://danja.github.io/flues/plugins/shifty"

static constexpr int kMaxDivisions = 16;
static constexpr int kBufferSize = 65536;
static constexpr int kGrainCount = 4;
static constexpr int kGrainSize = 2048;
static constexpr int kGrainHop = kGrainSize / 2;
static constexpr int kBaseLatency = 8192;

enum PortIndex {
    PORT_CONTROL = 0,
    PORT_IN_L,
    PORT_IN_R,
    PORT_OUT_L,
    PORT_OUT_R,
    PORT_BLOCK_BARS,
    PORT_DIVISION_COUNT,
    PORT_MIX,
    PORT_SMOOTH_MS,
    PORT_SHIFT_1,
    PORT_SHIFT_16 = PORT_SHIFT_1 + 15,
    PORT_ACTIVE_DIVISION,
    PORT_ACTIVE_SHIFT
};

struct TimeInfo {
    bool valid = false;
    bool playing = false;
    double bar = 0.0;
    double barBeat = 0.0;
    double beatsPerBar = 4.0;
    double bpm = 120.0;
};

struct ShiftyURIDs {
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
};

struct Shifty {
    const LV2_Atom_Sequence* control = nullptr;
    const float* in_l = nullptr;
    const float* in_r = nullptr;
    float* out_l = nullptr;
    float* out_r = nullptr;

    const float* block_bars = nullptr;
    const float* division_count = nullptr;
    const float* mix = nullptr;
    const float* smooth_ms = nullptr;
    const float* shifts[kMaxDivisions] = {nullptr};

    float* active_division = nullptr;
    float* active_shift = nullptr;

    LV2_URID_Map* map = nullptr;
    ShiftyURIDs urids{};
    double sample_rate = 48000.0;

    int last_division = -1;
    float current_shift = 0.0f;
    float target_shift = 0.0f;
    float current_ratio = 1.0f;
    int write_pos = 0;
    int spawn_countdown = 0;

    float buffer_l[kBufferSize] = {0.0f};
    float buffer_r[kBufferSize] = {0.0f};

    struct Grain {
        bool active = false;
        float start_pos = 0.0f;
        float progress = 0.0f;
        float ratio = 1.0f;
    } grains[kGrainCount];
};

static inline int clampi(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static inline float clampf(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static bool atom_to_double(const LV2_Atom* atom, const ShiftyURIDs& urids, double* out) {
    if (!atom || !out) {
        return false;
    }
    if (atom->type == urids.atom_Float) {
        *out = ((const LV2_Atom_Float*)atom)->body;
        return true;
    }
    if (atom->type == urids.atom_Double) {
        *out = ((const LV2_Atom_Double*)atom)->body;
        return true;
    }
    if (atom->type == urids.atom_Int) {
        *out = ((const LV2_Atom_Int*)atom)->body;
        return true;
    }
    if (atom->type == urids.atom_Long) {
        *out = (double)((const LV2_Atom_Long*)atom)->body;
        return true;
    }
    return false;
}

static bool read_time_info(const LV2_Atom_Sequence* control, const ShiftyURIDs& urids, TimeInfo* info) {
    if (!control || !info) {
        return false;
    }

    bool found = false;
    TimeInfo local = *info;
    LV2_ATOM_SEQUENCE_FOREACH(control, ev) {
        const LV2_Atom_Object* obj = nullptr;
        if (ev->body.type == urids.time_Position) {
            obj = (const LV2_Atom_Object*)&ev->body;
        } else if (ev->body.type == urids.atom_Object || ev->body.type == urids.atom_Blank) {
            const LV2_Atom_Object* cand = (const LV2_Atom_Object*)&ev->body;
            if (cand->body.otype == urids.time_Position) {
                obj = cand;
            }
        }
        if (!obj) {
            continue;
        }

        found = true;
        const LV2_Atom* bar_atom = nullptr;
        const LV2_Atom* bar_beat_atom = nullptr;
        const LV2_Atom* beats_per_bar_atom = nullptr;
        const LV2_Atom* bpm_atom = nullptr;
        const LV2_Atom* speed_atom = nullptr;

        lv2_atom_object_get(obj,
                            urids.time_bar, &bar_atom,
                            urids.time_barBeat, &bar_beat_atom,
                            urids.time_beatsPerBar, &beats_per_bar_atom,
                            urids.time_beatsPerMinute, &bpm_atom,
                            urids.time_speed, &speed_atom,
                            0);

        double value = 0.0;
        if (atom_to_double(bar_atom, urids, &value)) {
            local.bar = value;
        }
        if (atom_to_double(bar_beat_atom, urids, &value)) {
            local.barBeat = value;
        }
        if (atom_to_double(beats_per_bar_atom, urids, &value) && value > 0.0) {
            local.beatsPerBar = value;
        }
        if (atom_to_double(bpm_atom, urids, &value) && value > 0.0) {
            local.bpm = value;
        }
        if (atom_to_double(speed_atom, urids, &value)) {
            local.playing = value > 0.0;
        }
    }

    if (found) {
        local.valid = true;
        *info = local;
    }
    return found;
}

static float semitones_to_ratio(float semitones) {
    return powf(2.0f, semitones / 12.0f);
}

static int wrap_index(int value) {
    int wrapped = value % kBufferSize;
    return wrapped < 0 ? wrapped + kBufferSize : wrapped;
}

static float read_buffer_linear(const float* buffer, float pos) {
    const int i0 = wrap_index((int)floorf(pos));
    const int i1 = wrap_index(i0 + 1);
    const float frac = pos - floorf(pos);
    return buffer[i0] + (buffer[i1] - buffer[i0]) * frac;
}

static float grain_window(float progress) {
    const float phase = progress / (float)kGrainSize;
    return 0.5f - 0.5f * cosf(phase * 2.0f * (float)M_PI);
}

static void reset_engine(Shifty* self) {
    memset(self->buffer_l, 0, sizeof(self->buffer_l));
    memset(self->buffer_r, 0, sizeof(self->buffer_r));
    for (int i = 0; i < kGrainCount; ++i) {
        self->grains[i].active = false;
        self->grains[i].start_pos = 0.0f;
        self->grains[i].progress = 0.0f;
        self->grains[i].ratio = 1.0f;
    }
    self->write_pos = 0;
    self->spawn_countdown = 0;
    self->current_ratio = 1.0f;
}

static void spawn_grain(Shifty* self) {
    int slot = -1;
    for (int i = 0; i < kGrainCount; ++i) {
        if (!self->grains[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        slot = 0;
        float max_progress = self->grains[0].progress;
        for (int i = 1; i < kGrainCount; ++i) {
            if (self->grains[i].progress > max_progress) {
                max_progress = self->grains[i].progress;
                slot = i;
            }
        }
    }

    self->grains[slot].active = true;
    self->grains[slot].progress = 0.0f;
    self->grains[slot].ratio = self->current_ratio;
    self->grains[slot].start_pos = (float)wrap_index(self->write_pos - kBaseLatency);
}

static void process_pitch_shift(Shifty* self, uint32_t n_samples, float mix, float smooth_ms) {
    const float input_mix = 1.0f - mix;
    const float smooth_samples = fmaxf(1.0f, (smooth_ms * 0.001f) * (float)self->sample_rate);
    const float smooth_coeff = 1.0f / smooth_samples;

    for (uint32_t i = 0; i < n_samples; ++i) {
        const float in_l = self->in_l ? self->in_l[i] : 0.0f;
        const float in_r = self->in_r ? self->in_r[i] : 0.0f;

        self->buffer_l[self->write_pos] = in_l;
        self->buffer_r[self->write_pos] = in_r;

        const float target_ratio = semitones_to_ratio(self->target_shift);
        self->current_ratio += (target_ratio - self->current_ratio) * smooth_coeff;

        if (self->spawn_countdown <= 0) {
            spawn_grain(self);
            self->spawn_countdown = kGrainHop;
        }
        self->spawn_countdown -= 1;

        float wet_l = 0.0f;
        float wet_r = 0.0f;
        float weight_sum = 0.0f;

        for (int g = 0; g < kGrainCount; ++g) {
            if (!self->grains[g].active) {
                continue;
            }

            const float window = grain_window(self->grains[g].progress);
            const float read_pos = self->grains[g].start_pos + self->grains[g].progress * self->grains[g].ratio;
            wet_l += read_buffer_linear(self->buffer_l, read_pos) * window;
            wet_r += read_buffer_linear(self->buffer_r, read_pos) * window;
            weight_sum += window;

            self->grains[g].progress += 1.0f;
            if (self->grains[g].progress >= (float)kGrainSize) {
                self->grains[g].active = false;
            }
        }

        if (weight_sum > 0.0001f) {
            wet_l /= weight_sum;
            wet_r /= weight_sum;
        } else {
            wet_l = in_l;
            wet_r = in_r;
        }

        self->out_l[i] = in_l * input_mix + wet_l * mix;
        self->out_r[i] = in_r * input_mix + wet_r * mix;
        self->write_pos = wrap_index(self->write_pos + 1);
    }
}

static int active_division_for_time(Shifty* self, const TimeInfo& info, int block_bars, int division_count) {
    if (!info.valid || !info.playing || block_bars <= 0 || division_count <= 0) {
        return -1;
    }

    const double absolute_bars = info.bar + (info.barBeat / info.beatsPerBar);
    const double block_phase = fmod(absolute_bars, (double)block_bars);
    const double positive_phase = block_phase < 0.0 ? block_phase + (double)block_bars : block_phase;
    const double division_span = (double)block_bars / (double)division_count;
    return clampi((int)floor(positive_phase / division_span), 0, division_count - 1);
}

static float shift_value_for_division(Shifty* self, int division) {
    if (division < 0 || division >= kMaxDivisions || !self->shifts[division]) {
        return 0.0f;
    }
    return clampf(*self->shifts[division], -24.0f, 24.0f);
}

static LV2_Handle instantiate(const LV2_Descriptor*, double rate, const char*, const LV2_Feature* const* features) {
    Shifty* self = new Shifty();
    if (!self) {
        return nullptr;
    }
    self->sample_rate = rate;
    for (int i = 0; features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_URID__map)) {
            self->map = (LV2_URID_Map*)features[i]->data;
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
    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    Shifty* self = (Shifty*)instance;
    switch (port) {
        case PORT_CONTROL: self->control = (const LV2_Atom_Sequence*)data; break;
        case PORT_IN_L: self->in_l = (const float*)data; break;
        case PORT_IN_R: self->in_r = (const float*)data; break;
        case PORT_OUT_L: self->out_l = (float*)data; break;
        case PORT_OUT_R: self->out_r = (float*)data; break;
        case PORT_BLOCK_BARS: self->block_bars = (const float*)data; break;
        case PORT_DIVISION_COUNT: self->division_count = (const float*)data; break;
        case PORT_MIX: self->mix = (const float*)data; break;
        case PORT_SMOOTH_MS: self->smooth_ms = (const float*)data; break;
        case PORT_ACTIVE_DIVISION: self->active_division = (float*)data; break;
        case PORT_ACTIVE_SHIFT: self->active_shift = (float*)data; break;
        default:
            if (port >= PORT_SHIFT_1 && port <= PORT_SHIFT_16) {
                self->shifts[port - PORT_SHIFT_1] = (const float*)data;
            }
            break;
    }
}

static void activate(LV2_Handle instance) {
    Shifty* self = (Shifty*)instance;
    self->last_division = -1;
    self->current_shift = 0.0f;
    self->target_shift = 0.0f;
    reset_engine(self);
    if (self->active_division) {
        *self->active_division = -1.0f;
    }
    if (self->active_shift) {
        *self->active_shift = 0.0f;
    }
}

static void run(LV2_Handle instance, uint32_t n_samples) {
    Shifty* self = (Shifty*)instance;
    if (!self || !self->out_l || !self->out_r) {
        return;
    }

    TimeInfo info;
    read_time_info(self->control, self->urids, &info);

    const int block_bars = clampi((int)lroundf(self->block_bars ? *self->block_bars : 2.0f), 1, 8);
    const int division_count = clampi((int)lroundf(self->division_count ? *self->division_count : 8.0f), 1, kMaxDivisions);
    const int active_division = active_division_for_time(self, info, block_bars, division_count);
    const float shift = shift_value_for_division(self, active_division);
    const float mix = clampf(self->mix ? *self->mix : 1.0f, 0.0f, 1.0f);
    const float smooth_ms = clampf(self->smooth_ms ? *self->smooth_ms : 30.0f, 0.0f, 250.0f);

    self->last_division = active_division;
    self->target_shift = shift;
    self->current_shift += (self->target_shift - self->current_shift) * 0.25f;

    if (!info.valid || !info.playing) {
        reset_engine(self);
        for (uint32_t i = 0; i < n_samples; ++i) {
            self->out_l[i] = self->in_l ? self->in_l[i] : 0.0f;
            self->out_r[i] = self->in_r ? self->in_r[i] : 0.0f;
        }
    } else {
        process_pitch_shift(self, n_samples, mix, smooth_ms);
    }

    if (self->active_division) {
        *self->active_division = (float)active_division;
    }
    if (self->active_shift) {
        *self->active_shift = shift;
    }
}

static void deactivate(LV2_Handle instance) {
    Shifty* self = (Shifty*)instance;
    self->last_division = -1;
    self->current_shift = 0.0f;
    self->target_shift = 0.0f;
    reset_engine(self);
}

static void cleanup(LV2_Handle instance) {
    delete (Shifty*)instance;
}

static const void* extension_data(const char*) {
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    SHIFTY_URI,
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
    return index == 0 ? &descriptor : nullptr;
}
