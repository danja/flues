#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/state/state.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>

#define CHORDANT_URI "https://danja.github.io/flues/plugins/chordant"

enum PortIndex {
    PORT_CONTROL = 0,
    PORT_AUDIO_IN_L,
    PORT_AUDIO_IN_R,
    PORT_AUDIO_OUT_L,
    PORT_AUDIO_OUT_R,
    PORT_TOTAL_BARS,
    PORT_DIVISION,
    PORT_STEPS,
    PORT_OFFSET,
    PORT_FADE,
    PORT_MAX_SEGMENTS,
    PORT_NOCAP_PASSTHROUGH,
    PORT_CLEAR_TRIGGER,
    PORT_CAPTURE_MODE,
    PORT_DRY_WET,
    PORT_ATTACK_MS,
    PORT_DECAY_MS,
    PORT_TOTAL_COUNT
};

enum CaptureMode {
    CAPTURE_1_STEP = 0,
    CAPTURE_2_STEPS = 1,
    CAPTURE_4_STEPS = 2,
    CAPTURE_1_BAR = 3
};

struct ChordantURIDs {
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
    LV2_URID state_max_segments = 0;
    LV2_URID state_nocap_passthrough = 0;
    LV2_URID state_clear_trigger = 0;
    LV2_URID state_capture_mode = 0;
    LV2_URID state_dry_wet = 0;
    LV2_URID state_attack_ms = 0;
    LV2_URID state_decay_ms = 0;
};

struct TimeInfo {
    bool valid = false;
    bool playing = false;
    double bar = 0.0;
    double barBeat = 0.0;
    double beatsPerBar = 4.0;
    double bpm = 120.0;
};

struct Segment {
    std::vector<float> l;
    std::vector<float> r;
};

struct MixSegment {
    Segment segment;
    size_t pos = 0;
};

struct Chordant {
    const LV2_Atom_Sequence* control = nullptr;
    const float* in_l = nullptr;
    const float* in_r = nullptr;
    float* out_l = nullptr;
    float* out_r = nullptr;

    const float* total_bars_port = nullptr;
    const float* division_port = nullptr;
    const float* steps_port = nullptr;
    const float* offset_port = nullptr;
    const float* fade_port = nullptr;
    const float* max_segments_port = nullptr;
    const float* nocap_passthrough_port = nullptr;
    const float* clear_trigger_port = nullptr;
    const float* capture_mode_port = nullptr;
    const float* dry_wet_port = nullptr;
    const float* attack_ms_port = nullptr;
    const float* decay_ms_port = nullptr;

    LV2_URID_Map* map = nullptr;
    ChordantURIDs urids{};

    double sample_rate = 48000.0;

    float cached_total_bars = 2.0f;
    float cached_division = 8.0f;
    float cached_steps = 2.0f;
    float cached_offset = 0.0f;
    float cached_fade = 0.0f;
    float cached_max_segments = 8.0f;
    float cached_nocap_passthrough = 0.0f;
    float cached_clear_trigger = 1.0f;
    float cached_capture_mode = 0.0f;
    float cached_dry_wet = 100.0f;
    float cached_attack_ms = 8.0f;
    float cached_decay_ms = 180.0f;

    bool transport_was_playing = false;
    double cycle_origin_bar = 0.0;
    double fallback_bar_pos = 0.0;
    double fallback_bpm = 120.0;
    double fallback_beats_per_bar = 4.0;

    int last_step_index = -1;
    bool last_step_active = false;

    std::deque<Segment> captures;
    std::vector<float> current_l;
    std::vector<float> current_r;
    uint32_t current_remaining = 0;

    std::vector<MixSegment> active_mix;
    uint32_t active_mix_count = 0;

    uint32_t env_attack_samples = 1;
    uint32_t env_decay_samples = 1;
    uint32_t env_attack_remaining = 0;
    uint32_t env_decay_remaining = 0;
    float env_value = 0.0f;
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

static bool atom_to_double(const LV2_Atom* atom, const ChordantURIDs& urids, double* out) {
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

static bool read_time_info(const LV2_Atom_Sequence* control, const ChordantURIDs& urids, TimeInfo* info) {
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
        bool saw_speed = false;

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
            saw_speed = true;
            local.playing = value > 0.0;
        }
        if (!saw_speed) {
            // Some hosts omit time:speed in Position events while transport is running.
            local.playing = true;
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

static void update_cached_params(Chordant* self) {
    if (!self) return;
    self->cached_total_bars = self->total_bars_port ? *self->total_bars_port : self->cached_total_bars;
    self->cached_division = self->division_port ? *self->division_port : self->cached_division;
    self->cached_steps = self->steps_port ? *self->steps_port : self->cached_steps;
    self->cached_offset = self->offset_port ? *self->offset_port : self->cached_offset;
    self->cached_fade = self->fade_port ? *self->fade_port : self->cached_fade;
    self->cached_max_segments = self->max_segments_port ? *self->max_segments_port : self->cached_max_segments;
    self->cached_nocap_passthrough = self->nocap_passthrough_port ? *self->nocap_passthrough_port : self->cached_nocap_passthrough;
    self->cached_clear_trigger = self->clear_trigger_port ? *self->clear_trigger_port : self->cached_clear_trigger;
    self->cached_capture_mode = self->capture_mode_port ? *self->capture_mode_port : self->cached_capture_mode;
    self->cached_dry_wet = self->dry_wet_port ? *self->dry_wet_port : self->cached_dry_wet;
    self->cached_attack_ms = self->attack_ms_port ? *self->attack_ms_port : self->cached_attack_ms;
    self->cached_decay_ms = self->decay_ms_port ? *self->decay_ms_port : self->cached_decay_ms;
}

static uint32_t capture_target_frames(CaptureMode mode, double step_frames, double bar_frames) {
    double frames = step_frames;
    switch (mode) {
        case CAPTURE_2_STEPS:
            frames = step_frames * 2.0;
            break;
        case CAPTURE_4_STEPS:
            frames = step_frames * 4.0;
            break;
        case CAPTURE_1_BAR:
            frames = bar_frames;
            break;
        case CAPTURE_1_STEP:
        default:
            frames = step_frames;
            break;
    }
    if (frames < 1.0) {
        frames = 1.0;
    }
    return static_cast<uint32_t>(frames);
}

static void finalize_capture_segment(Chordant* self, int max_segments) {
    if (!self || self->current_l.empty()) {
        self->current_l.clear();
        self->current_r.clear();
        return;
    }

    Segment seg;
    seg.l.swap(self->current_l);
    seg.r.swap(self->current_r);
    self->captures.push_back(std::move(seg));

    while ((int)self->captures.size() > max_segments) {
        self->captures.pop_front();
    }
}

static void start_mix_trigger(Chordant* self, bool clear_trigger) {
    self->active_mix.clear();
    self->active_mix_count = static_cast<uint32_t>(self->captures.size());
    self->active_mix.reserve(self->captures.size());

    for (size_t i = 0; i < self->captures.size(); ++i) {
        MixSegment ms;
        ms.segment = self->captures[i];
        ms.pos = 0;
        self->active_mix.push_back(ms);
    }

    if (clear_trigger) {
        self->captures.clear();
    }
}

static float compute_mix_weight(bool active, double local_bar, double block_bars, float fade_bars) {
    if (!active) {
        return 0.0f;
    }
    if (fade_bars <= 0.0f) {
        return 1.0f;
    }
    const double fade = static_cast<double>(fade_bars);
    const double in_gain = clampf(static_cast<float>(local_bar / fade), 0.0f, 1.0f);
    const double out_gain = clampf(static_cast<float>((block_bars - local_bar) / fade), 0.0f, 1.0f);
    return static_cast<float>(in_gain < out_gain ? in_gain : out_gain);
}

static inline uint32_t ms_to_samples(double sample_rate, float ms) {
    const double samples = (sample_rate * static_cast<double>(ms)) * 0.001;
    return static_cast<uint32_t>(samples < 1.0 ? 1.0 : std::lround(samples));
}

static inline void trigger_envelope(Chordant* self) {
    if (!self) {
        return;
    }
    self->env_value = 0.0f;
    self->env_attack_remaining = self->env_attack_samples;
    self->env_decay_remaining = self->env_decay_samples;
}

static inline float advance_envelope(Chordant* self) {
    if (!self) {
        return 0.0f;
    }

    if (self->env_attack_remaining > 0) {
        const float step = 1.0f / static_cast<float>(self->env_attack_samples);
        self->env_value += step;
        if (self->env_value > 1.0f) {
            self->env_value = 1.0f;
        }
        self->env_attack_remaining--;
        return self->env_value;
    }

    if (self->env_decay_remaining > 0) {
        const float step = 1.0f / static_cast<float>(self->env_decay_samples);
        self->env_value -= step;
        if (self->env_value < 0.0f) {
            self->env_value = 0.0f;
        }
        self->env_decay_remaining--;
        return self->env_value;
    }

    self->env_value = 0.0f;
    return 0.0f;
}

static LV2_State_Status chordant_state_save(
    LV2_Handle instance,
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t /*flags*/,
    const LV2_Feature* const* /*features*/) {
    Chordant* self = static_cast<Chordant*>(instance);
    if (!self || !store) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    update_cached_params(self);

    const uint32_t flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;
    store(handle, self->urids.state_total_bars, &self->cached_total_bars, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_division, &self->cached_division, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_steps, &self->cached_steps, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_offset, &self->cached_offset, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_fade, &self->cached_fade, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_max_segments, &self->cached_max_segments, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_nocap_passthrough, &self->cached_nocap_passthrough, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_clear_trigger, &self->cached_clear_trigger, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_capture_mode, &self->cached_capture_mode, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_dry_wet, &self->cached_dry_wet, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_attack_ms, &self->cached_attack_ms, sizeof(float), self->urids.atom_Float, flags);
    store(handle, self->urids.state_decay_ms, &self->cached_decay_ms, sizeof(float), self->urids.atom_Float, flags);

    return LV2_STATE_SUCCESS;
}

static LV2_State_Status chordant_state_restore(
    LV2_Handle instance,
    LV2_State_Retrieve_Function retrieve,
    LV2_State_Handle handle,
    uint32_t /*flags*/,
    const LV2_Feature* const* /*features*/) {
    Chordant* self = static_cast<Chordant*>(instance);
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
    restore_value(self->urids.state_max_segments, &self->cached_max_segments);
    restore_value(self->urids.state_nocap_passthrough, &self->cached_nocap_passthrough);
    restore_value(self->urids.state_clear_trigger, &self->cached_clear_trigger);
    restore_value(self->urids.state_capture_mode, &self->cached_capture_mode);
    restore_value(self->urids.state_dry_wet, &self->cached_dry_wet);
    restore_value(self->urids.state_attack_ms, &self->cached_attack_ms);
    restore_value(self->urids.state_decay_ms, &self->cached_decay_ms);

    return LV2_STATE_SUCCESS;
}

static const LV2_State_Interface chordant_state_interface = {
    chordant_state_save,
    chordant_state_restore
};

static LV2_Handle instantiate(const LV2_Descriptor* /*descriptor*/,
                              double rate,
                              const char* /*bundle_path*/,
                              const LV2_Feature* const* features) {
    Chordant* self = new Chordant();
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

    self->urids.state_total_bars = self->map->map(self->map->handle, CHORDANT_URI "#total_bars");
    self->urids.state_division = self->map->map(self->map->handle, CHORDANT_URI "#division");
    self->urids.state_steps = self->map->map(self->map->handle, CHORDANT_URI "#steps");
    self->urids.state_offset = self->map->map(self->map->handle, CHORDANT_URI "#offset");
    self->urids.state_fade = self->map->map(self->map->handle, CHORDANT_URI "#fade");
    self->urids.state_max_segments = self->map->map(self->map->handle, CHORDANT_URI "#max_segments");
    self->urids.state_nocap_passthrough = self->map->map(self->map->handle, CHORDANT_URI "#nocap_passthrough");
    self->urids.state_clear_trigger = self->map->map(self->map->handle, CHORDANT_URI "#clear_trigger");
    self->urids.state_capture_mode = self->map->map(self->map->handle, CHORDANT_URI "#capture_mode");
    self->urids.state_dry_wet = self->map->map(self->map->handle, CHORDANT_URI "#dry_wet");
    self->urids.state_attack_ms = self->map->map(self->map->handle, CHORDANT_URI "#attack_ms");
    self->urids.state_decay_ms = self->map->map(self->map->handle, CHORDANT_URI "#decay_ms");

    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    Chordant* self = static_cast<Chordant*>(instance);
    if (!self) {
        return;
    }

    switch (port) {
        case PORT_CONTROL:
            self->control = static_cast<const LV2_Atom_Sequence*>(data);
            break;
        case PORT_AUDIO_IN_L:
            self->in_l = static_cast<const float*>(data);
            break;
        case PORT_AUDIO_IN_R:
            self->in_r = static_cast<const float*>(data);
            break;
        case PORT_AUDIO_OUT_L:
            self->out_l = static_cast<float*>(data);
            break;
        case PORT_AUDIO_OUT_R:
            self->out_r = static_cast<float*>(data);
            break;
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
        case PORT_MAX_SEGMENTS:
            self->max_segments_port = static_cast<const float*>(data);
            break;
        case PORT_NOCAP_PASSTHROUGH:
            self->nocap_passthrough_port = static_cast<const float*>(data);
            break;
        case PORT_CLEAR_TRIGGER:
            self->clear_trigger_port = static_cast<const float*>(data);
            break;
        case PORT_CAPTURE_MODE:
            self->capture_mode_port = static_cast<const float*>(data);
            break;
        case PORT_DRY_WET:
            self->dry_wet_port = static_cast<const float*>(data);
            break;
        case PORT_ATTACK_MS:
            self->attack_ms_port = static_cast<const float*>(data);
            break;
        case PORT_DECAY_MS:
            self->decay_ms_port = static_cast<const float*>(data);
            break;
        default:
            break;
    }
}

static void activate(LV2_Handle instance) {
    Chordant* self = static_cast<Chordant*>(instance);
    if (!self) {
        return;
    }
    self->transport_was_playing = false;
    self->cycle_origin_bar = 0.0;
    self->fallback_bar_pos = 0.0;
    self->fallback_bpm = 120.0;
    self->fallback_beats_per_bar = 4.0;
    self->last_step_index = -1;
    self->last_step_active = false;
    self->captures.clear();
    self->current_l.clear();
    self->current_r.clear();
    self->current_remaining = 0;
    self->active_mix.clear();
    self->active_mix_count = 0;
    self->env_attack_remaining = 0;
    self->env_decay_remaining = 0;
    self->env_value = 0.0f;
}

static void run(LV2_Handle instance, uint32_t nframes) {
    Chordant* self = static_cast<Chordant*>(instance);
    if (!self || nframes == 0) {
        return;
    }

    update_cached_params(self);

    const float total_bars = clampf(self->cached_total_bars, 0.125f, 8.0f);
    const int division = clampi(static_cast<int>(std::lround(self->cached_division)), 1, 16);
    const int steps = clampi(static_cast<int>(std::lround(self->cached_steps)), 0, division);
    const int offset = clampi(static_cast<int>(std::lround(self->cached_offset)), 0, division - 1);
    const float fade_bars = clampf(self->cached_fade, 0.0f, 1.0f);
    const int max_segments = clampi(static_cast<int>(std::lround(self->cached_max_segments)), 1, 12);
    const bool nocap_passthrough = self->cached_nocap_passthrough >= 0.5f;
    const bool clear_trigger = self->cached_clear_trigger >= 0.5f;
    const CaptureMode capture_mode = static_cast<CaptureMode>(clampi(static_cast<int>(std::lround(self->cached_capture_mode)), 0, 3));
    const float dry_wet = clampf(self->cached_dry_wet, 0.0f, 100.0f) * 0.01f;
    const float attack_ms = clampf(self->cached_attack_ms, 1.0f, 200.0f);
    const float decay_ms = clampf(self->cached_decay_ms, 10.0f, 800.0f);
    self->env_attack_samples = ms_to_samples(self->sample_rate, attack_ms);
    self->env_decay_samples = ms_to_samples(self->sample_rate, decay_ms);

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
            self->last_step_index = -1;
            self->last_step_active = false;
            self->current_remaining = 0;
            self->current_l.clear();
            self->current_r.clear();
        }
        self->transport_was_playing = true;
    } else {
        self->transport_was_playing = false;
        bar_pos = self->fallback_bar_pos;
        bar_step = self->fallback_bpm / (60.0 * self->sample_rate * self->fallback_beats_per_bar);
    }

    const double cycle_bars = static_cast<double>(total_bars);
    const double block_bars = cycle_bars / static_cast<double>(division);
    const double frames_per_bar = (60.0 * self->sample_rate * time_info.beatsPerBar) / (time_info.bpm > 0.0 ? time_info.bpm : 120.0);
    const double frames_per_step = frames_per_bar * block_bars;
    const uint32_t target_frames = capture_target_frames(capture_mode, frames_per_step, frames_per_bar);

    for (uint32_t i = 0; i < nframes; ++i) {
        const float in_l = self->in_l ? self->in_l[i] : 0.0f;
        const float in_r = self->in_r ? self->in_r[i] : 0.0f;

        float out_l = in_l;
        float out_r = in_r;

        if (block_bars <= 0.0) {
            if (self->out_l) self->out_l[i] = out_l;
            if (self->out_r) self->out_r[i] = out_r;
            bar_pos += bar_step;
            continue;
        }

        const double rel_bar = bar_pos - self->cycle_origin_bar;
        double cycle_pos = std::fmod(rel_bar, cycle_bars);
        if (cycle_pos < 0.0) {
            cycle_pos += cycle_bars;
        }

        int step_index = static_cast<int>(std::floor(cycle_pos / block_bars));
        step_index = clampi(step_index, 0, division - 1);
        const bool step_active = euclid_hit(step_index, steps, offset, division);

        const bool step_changed = (step_index != self->last_step_index);
        const bool active_changed = (step_active != self->last_step_active);
        if (step_changed || active_changed) {
            if (step_active) {
                if (!self->current_l.empty()) {
                    finalize_capture_segment(self, max_segments);
                    self->current_remaining = 0;
                }
                start_mix_trigger(self, clear_trigger);
                trigger_envelope(self);
            } else {
                self->current_remaining = target_frames;
                self->current_l.clear();
                self->current_r.clear();
            }
            self->last_step_index = step_index;
            self->last_step_active = step_active;
        }

        if (!step_active) {
            if (self->current_remaining == 0) {
                self->current_remaining = target_frames;
                self->current_l.clear();
                self->current_r.clear();
            }
            self->current_l.push_back(in_l);
            self->current_r.push_back(in_r);
            if (self->current_remaining > 0) {
                self->current_remaining--;
            }
            if (self->current_remaining == 0) {
                finalize_capture_segment(self, max_segments);
                self->current_remaining = target_frames;
                self->current_l.clear();
                self->current_r.clear();
            }

            // Capture while output stays silent; active steps play back the accumulated content.
            out_l = 0.0f;
            out_r = 0.0f;
        }

        float wet_l = 0.0f;
        float wet_r = 0.0f;
        if (!self->active_mix.empty() && self->active_mix_count > 0) {
            uint32_t contributing = 0;
            for (size_t m = 0; m < self->active_mix.size(); ++m) {
                MixSegment& ms = self->active_mix[m];
                if (ms.pos < ms.segment.l.size() && ms.pos < ms.segment.r.size()) {
                    wet_l += ms.segment.l[ms.pos];
                    wet_r += ms.segment.r[ms.pos];
                    contributing++;
                    ms.pos++;
                }
            }

            if (contributing > 0) {
                // Intentionally summed stabs: each captured segment contributes directly.
            } else {
                self->active_mix.clear();
                self->active_mix_count = 0;
            }
        }

        if (step_active) {
            if (self->active_mix_count == 0) {
                if (nocap_passthrough) {
                    wet_l = in_l;
                    wet_r = in_r;
                } else {
                    wet_l = 0.0f;
                    wet_r = 0.0f;
                }
            }

            const double step_start = static_cast<double>(step_index) * block_bars;
            const double local_bar = cycle_pos - step_start;
            const float mix_w = compute_mix_weight(step_active, local_bar, block_bars, fade_bars);
            const float dry_w = 1.0f - mix_w;
            out_l = in_l * dry_w + wet_l * mix_w;
            out_r = in_r * dry_w + wet_r * mix_w;
        }

        const float env = advance_envelope(self);
        const float processed_l = out_l * env;
        const float processed_r = out_r * env;
        out_l = in_l * (1.0f - dry_wet) + processed_l * dry_wet;
        out_r = in_r * (1.0f - dry_wet) + processed_r * dry_wet;

        if (self->out_l) self->out_l[i] = out_l;
        if (self->out_r) self->out_r[i] = out_r;

        bar_pos += bar_step;
    }

    self->fallback_bar_pos = bar_pos;
}

static void deactivate(LV2_Handle /*instance*/) {
}

static void cleanup(LV2_Handle instance) {
    Chordant* self = static_cast<Chordant*>(instance);
    delete self;
}

static const void* extension_data(const char* uri) {
    if (!std::strcmp(uri, LV2_STATE__interface)) {
        return &chordant_state_interface;
    }
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    CHORDANT_URI,
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
