#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>
#include <lv2/time/time.h>
#include <lv2/state/state.h>

#include "outsider/command_packet.hpp"
#include "outsider/transport_snapshot.hpp"
#include "outsider_client_net.hpp"
#include "outsider_client_state.hpp"
#include "spsc_queue.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#define OUTSIDER_CLIENT_URI "https://danja.github.io/flues/plugins/outsider-client"

enum PortIndex {
    PORT_CONTROL = 0,
    PORT_AUDIO_IN_L = 1,
    PORT_AUDIO_IN_R = 2,
    PORT_AUDIO_OUT_L = 3,
    PORT_AUDIO_OUT_R = 4,
    PORT_ENABLE = 5,
    PORT_SESSION_SLOT = 6,
    PORT_ENDPOINT_SLOT = 7,
    PORT_AUTHORITY = 8,
    PORT_RECONNECT = 9,
    PORT_FALLBACK_GAIN = 10,
    PORT_CONNECTED = 11,
    PORT_SERVER_SEEN = 12,
    PORT_CURRENT_GAIN = 13,
    PORT_CURRENT_STATE = 14,
    PORT_CURRENT_MODE = 15,
    PORT_AUTHORITY_ACTIVE = 16
};

struct OutsiderClientURIDs {
    LV2_URID atom_Object = 0;
    LV2_URID atom_Float = 0;
    LV2_URID atom_Double = 0;
    LV2_URID atom_Int = 0;
    LV2_URID atom_Long = 0;
    LV2_URID atom_Sequence = 0;
    LV2_URID atom_Chunk = 0;
    LV2_URID time_Position = 0;
    LV2_URID time_speed = 0;
    LV2_URID time_bar = 0;
    LV2_URID time_barBeat = 0;
    LV2_URID time_beatsPerBar = 0;
    LV2_URID time_beatsPerMinute = 0;
    LV2_URID state_config = 0;
};

struct TimeInfo {
    bool valid = false;
    bool playing = false;
    double bar = 0.0;
    double barBeat = 0.0;
    double beatsPerBar = 4.0;
    double bpm = 120.0;
};

struct OutsiderClient {
    const LV2_Atom_Sequence* control = nullptr;
    const float* audio_in_l = nullptr;
    const float* audio_in_r = nullptr;
    float* audio_out_l = nullptr;
    float* audio_out_r = nullptr;

    const float* enable_port = nullptr;
    const float* session_slot_port = nullptr;
    const float* endpoint_slot_port = nullptr;
    const float* authority_port = nullptr;
    const float* reconnect_port = nullptr;
    const float* fallback_gain_port = nullptr;

    float* connected_out = nullptr;
    float* server_seen_out = nullptr;
    float* current_gain_out = nullptr;
    float* current_state_out = nullptr;
    float* current_mode_out = nullptr;
    float* authority_active_out = nullptr;

    LV2_URID_Map* map = nullptr;
    OutsiderClientURIDs urids{};
    double sample_rate = 48000.0;
    std::uint64_t block_counter = 0;

    outsider_client::PersistedConfig config{};
    outsider::OutsiderMode current_mode = outsider::OutsiderMode::Bypass;
    outsider_client::RuntimeState current_state = outsider_client::RuntimeState::Bypass;
    float current_gain = 1.0f;
    float target_gain = 1.0f;
    float fade_step = 0.0f;
    std::uint32_t fade_remaining = 0;
    std::uint64_t last_command_id = 0;

    outsider_client::SpscQueue<outsider::TransportSnapshot, 128> outbound_transport;
    outsider_client::SpscQueue<outsider::CommandPacket, 32> inbound_commands;
    outsider_client::OutsiderClientNet net;
};

static inline float clampf(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static inline std::uint16_t clamp_slot(float value) {
    int rounded = static_cast<int>(std::lrint(value));
    if (rounded < 1) rounded = 1;
    if (rounded > 65535) rounded = 65535;
    return static_cast<std::uint16_t>(rounded);
}

static bool atom_to_double(const LV2_Atom* atom, const OutsiderClientURIDs& urids, double* out) {
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

static bool read_time_info(const LV2_Atom_Sequence* control,
                           const OutsiderClientURIDs& urids,
                           TimeInfo* info) {
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
            const LV2_Atom_Object* candidate = reinterpret_cast<const LV2_Atom_Object*>(&ev->body);
            if (candidate->body.otype == urids.time_Position) {
                obj = candidate;
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

static void update_cached_config(OutsiderClient* self) {
    if (!self) {
        return;
    }
    if (self->enable_port) self->config.enable = *self->enable_port;
    if (self->session_slot_port) self->config.session_slot = *self->session_slot_port;
    if (self->endpoint_slot_port) self->config.endpoint_slot = *self->endpoint_slot_port;
    if (self->authority_port) self->config.authority = *self->authority_port;
    if (self->reconnect_port) self->config.reconnect = *self->reconnect_port;
    if (self->fallback_gain_port) self->config.fallback_gain = *self->fallback_gain_port;
}

static void reset_runtime(OutsiderClient* self) {
    self->block_counter = 0;
    self->current_mode = outsider::OutsiderMode::Bypass;
    self->current_state = outsider_client::RuntimeState::Bypass;
    self->current_gain = 1.0f;
    self->target_gain = 1.0f;
    self->fade_step = 0.0f;
    self->fade_remaining = 0;
    self->last_command_id = 0;
    self->outbound_transport.clear();
    self->inbound_commands.clear();
    self->net.stop();
}

static void apply_command(OutsiderClient* self,
                          const outsider::CommandPacket& command,
                          double bpm) {
    if (command.command_id < self->last_command_id) {
        return;
    }
    self->last_command_id = command.command_id;
    self->current_mode = command.mode;
    self->current_state = outsider_client::command_state_to_runtime(command.target_state);
    self->target_gain = clampf(command.target_gain, 0.0f, 1.0f);

    const double safe_bpm = bpm > 1.0 ? bpm : 120.0;
    const std::uint32_t total_samples = static_cast<std::uint32_t>(
        std::max(0.0, static_cast<double>(command.duration_beats)) * (60.0 / safe_bpm) * self->sample_rate);
    if (total_samples == 0) {
        self->current_gain = self->target_gain;
        self->fade_step = 0.0f;
        self->fade_remaining = 0;
        return;
    }

    self->fade_remaining = total_samples;
    self->fade_step = (self->target_gain - self->current_gain) / static_cast<float>(total_samples);
}

static LV2_State_Status outsider_client_state_save(LV2_Handle instance,
                                                   LV2_State_Store_Function store,
                                                   LV2_State_Handle handle,
                                                   std::uint32_t,
                                                   const LV2_Feature* const*) {
    OutsiderClient* self = reinterpret_cast<OutsiderClient*>(instance);
    if (!self || !store) {
        return LV2_STATE_ERR_UNKNOWN;
    }
    const std::uint32_t flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;
    store(handle, self->urids.state_config,
          &self->config, sizeof(self->config),
          self->urids.atom_Chunk, flags);
    return LV2_STATE_SUCCESS;
}

static LV2_State_Status outsider_client_state_restore(LV2_Handle instance,
                                                      LV2_State_Retrieve_Function retrieve,
                                                      LV2_State_Handle handle,
                                                      std::uint32_t,
                                                      const LV2_Feature* const*) {
    OutsiderClient* self = reinterpret_cast<OutsiderClient*>(instance);
    if (!self || !retrieve) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    size_t size = 0;
    std::uint32_t type = 0;
    std::uint32_t flags = 0;
    const void* data = retrieve(handle, self->urids.state_config, &size, &type, &flags);
    if (data && size == sizeof(self->config) && type == self->urids.atom_Chunk) {
        std::memcpy(&self->config, data, sizeof(self->config));
    }
    return LV2_STATE_SUCCESS;
}

static const LV2_State_Interface outsider_client_state_interface = {
    outsider_client_state_save,
    outsider_client_state_restore
};

static LV2_Handle instantiate(const LV2_Descriptor*,
                              double rate,
                              const char*,
                              const LV2_Feature* const* features) {
    OutsiderClient* self = new OutsiderClient();
    self->sample_rate = rate;

    for (int i = 0; features && features[i]; ++i) {
        if (!std::strcmp(features[i]->URI, LV2_URID__map)) {
            self->map = reinterpret_cast<LV2_URID_Map*>(features[i]->data);
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
    self->urids.atom_Chunk = self->map->map(self->map->handle, LV2_ATOM__Chunk);
    self->urids.time_Position = self->map->map(self->map->handle, LV2_TIME__Position);
    self->urids.time_speed = self->map->map(self->map->handle, LV2_TIME__speed);
    self->urids.time_bar = self->map->map(self->map->handle, LV2_TIME__bar);
    self->urids.time_barBeat = self->map->map(self->map->handle, LV2_TIME__barBeat);
    self->urids.time_beatsPerBar = self->map->map(self->map->handle, LV2_TIME__beatsPerBar);
    self->urids.time_beatsPerMinute = self->map->map(self->map->handle, LV2_TIME__beatsPerMinute);
    self->urids.state_config = self->map->map(self->map->handle, OUTSIDER_CLIENT_URI "#config");

    reset_runtime(self);
    return self;
}

static void connect_port(LV2_Handle instance, std::uint32_t port, void* data) {
    OutsiderClient* self = reinterpret_cast<OutsiderClient*>(instance);
    switch (port) {
        case PORT_CONTROL: self->control = reinterpret_cast<const LV2_Atom_Sequence*>(data); break;
        case PORT_AUDIO_IN_L: self->audio_in_l = reinterpret_cast<const float*>(data); break;
        case PORT_AUDIO_IN_R: self->audio_in_r = reinterpret_cast<const float*>(data); break;
        case PORT_AUDIO_OUT_L: self->audio_out_l = reinterpret_cast<float*>(data); break;
        case PORT_AUDIO_OUT_R: self->audio_out_r = reinterpret_cast<float*>(data); break;
        case PORT_ENABLE: self->enable_port = reinterpret_cast<const float*>(data); break;
        case PORT_SESSION_SLOT: self->session_slot_port = reinterpret_cast<const float*>(data); break;
        case PORT_ENDPOINT_SLOT: self->endpoint_slot_port = reinterpret_cast<const float*>(data); break;
        case PORT_AUTHORITY: self->authority_port = reinterpret_cast<const float*>(data); break;
        case PORT_RECONNECT: self->reconnect_port = reinterpret_cast<const float*>(data); break;
        case PORT_FALLBACK_GAIN: self->fallback_gain_port = reinterpret_cast<const float*>(data); break;
        case PORT_CONNECTED: self->connected_out = reinterpret_cast<float*>(data); break;
        case PORT_SERVER_SEEN: self->server_seen_out = reinterpret_cast<float*>(data); break;
        case PORT_CURRENT_GAIN: self->current_gain_out = reinterpret_cast<float*>(data); break;
        case PORT_CURRENT_STATE: self->current_state_out = reinterpret_cast<float*>(data); break;
        case PORT_CURRENT_MODE: self->current_mode_out = reinterpret_cast<float*>(data); break;
        case PORT_AUTHORITY_ACTIVE: self->authority_active_out = reinterpret_cast<float*>(data); break;
        default: break;
    }
}

static void activate(LV2_Handle instance) {
    OutsiderClient* self = reinterpret_cast<OutsiderClient*>(instance);
    reset_runtime(self);
}

static void run(LV2_Handle instance, std::uint32_t n_samples) {
    OutsiderClient* self = reinterpret_cast<OutsiderClient*>(instance);
    update_cached_config(self);

    const bool enabled = self->config.enable >= 0.5f;
    const std::uint16_t session_slot = clamp_slot(self->config.session_slot);
    const std::uint16_t endpoint_slot = clamp_slot(self->config.endpoint_slot);
    const bool authority_requested = self->config.authority >= 0.5f;
    const bool reconnect_enabled = self->config.reconnect >= 0.5f;

    self->net.configure(session_slot, endpoint_slot, authority_requested, reconnect_enabled);
    if (enabled) {
        self->net.start();
    } else {
        self->net.stop();
    }

    TimeInfo time_info{};
    read_time_info(self->control, self->urids, &time_info);

    outsider::TransportSnapshot snapshot{};
    snapshot.session_slot = session_slot;
    snapshot.endpoint_slot = endpoint_slot;
    snapshot.authority = authority_requested;
    snapshot.playing = time_info.playing;
    snapshot.block_counter = ++self->block_counter;
    snapshot.bar = time_info.bar;
    snapshot.beat = time_info.barBeat;
    snapshot.beats_per_bar = time_info.beatsPerBar;
    snapshot.bpm = time_info.bpm;
    snapshot.sample_rate = self->sample_rate;
    snapshot.block_size = n_samples;
    self->outbound_transport.push(snapshot);

    outsider::CommandPacket command{};
    while (self->inbound_commands.pop(&command)) {
        apply_command(self, command, time_info.bpm);
    }

    const float input_l_fallback = 0.0f;
    const float input_r_fallback = 0.0f;
    for (std::uint32_t i = 0; i < n_samples; ++i) {
        if (self->fade_remaining > 0) {
            self->current_gain += self->fade_step;
            self->fade_remaining--;
            if (self->fade_remaining == 0) {
                self->current_gain = self->target_gain;
            }
        } else {
            self->current_gain = self->target_gain;
        }

        const float gain = self->current_state == outsider_client::RuntimeState::Mute ? 0.0f : self->current_gain;
        const float in_l = self->audio_in_l ? self->audio_in_l[i] : input_l_fallback;
        const float in_r = self->audio_in_r ? self->audio_in_r[i] : input_r_fallback;
        if (self->audio_out_l) self->audio_out_l[i] = in_l * gain;
        if (self->audio_out_r) self->audio_out_r[i] = in_r * gain;
    }

    if (self->connected_out) *self->connected_out = self->net.connected() ? 1.0f : 0.0f;
    if (self->server_seen_out) *self->server_seen_out = self->net.server_seen() ? 1.0f : 0.0f;
    if (self->current_gain_out) *self->current_gain_out = self->current_gain;
    if (self->current_state_out) *self->current_state_out = outsider_client::runtime_state_to_port(self->current_state);
    if (self->current_mode_out) *self->current_mode_out = outsider_client::mode_to_port(self->current_mode);
    if (self->authority_active_out) *self->authority_active_out = self->net.authority_active() ? 1.0f : 0.0f;
}

static void deactivate(LV2_Handle instance) {
    OutsiderClient* self = reinterpret_cast<OutsiderClient*>(instance);
    self->net.stop();
}

static void cleanup(LV2_Handle instance) {
    OutsiderClient* self = reinterpret_cast<OutsiderClient*>(instance);
    delete self;
}

static const void* extension_data(const char* uri) {
    if (!std::strcmp(uri, LV2_STATE__interface)) {
        return &outsider_client_state_interface;
    }
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    OUTSIDER_CLIENT_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(std::uint32_t index) {
    return index == 0 ? &descriptor : nullptr;
}
