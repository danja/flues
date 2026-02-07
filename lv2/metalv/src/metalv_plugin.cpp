#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>
#include <lv2/midi/midi.h>
#include <lv2/state/state.h>

#include <cstring>
#include <vector>
#include <cstdlib>

#include "host/HostEngine.hpp"
#include "mcp/MCPServer.hpp"

#define METALV_URI "https://danja.github.io/flues/plugins/metalv"

static constexpr int kSlotCount = 4;
static constexpr int kBankSize = 8;

enum PortIndex {
    PORT_IN_L = 0,
    PORT_IN_R = 1,
    PORT_OUT_L = 2,
    PORT_OUT_R = 3,
    PORT_MIDI_IN = 4,
    PORT_MIDI_OUT = 5,
    PORT_SLOT_0_BYPASS = 6
};

struct MetaLVURIDs {
    LV2_URID atom_Sequence = 0;
    LV2_URID atom_String = 0;
    LV2_URID atom_Int = 0;
    LV2_URID midi_Event = 0;

    LV2_URID state_slot_uri[kSlotCount] = {0};
    LV2_URID state_slot_map[kSlotCount] = {0};
};

struct MetaLV {
    const float* in_l = nullptr;
    const float* in_r = nullptr;
    float* out_l = nullptr;
    float* out_r = nullptr;

    const LV2_Atom_Sequence* midi_in = nullptr;
    LV2_Atom_Sequence* midi_out = nullptr;

    const float* slot_bypass[kSlotCount] = {nullptr};
    const float* slot_gain[kSlotCount] = {nullptr};
    const float* slot_params[kSlotCount][kBankSize] = {{nullptr}};

    LV2_URID_Map* map = nullptr;
    MetaLVURIDs urids{};

    HostEngine* engine = nullptr;
    MCPServer* mcp = nullptr;

    double sample_rate = 48000.0;
    uint64_t frame_time = 0;
};

static int slot_port_base(int slot) {
    return PORT_SLOT_0_BYPASS + (slot * (2 + kBankSize));
}

static LV2_State_Status metalv_state_save(
    LV2_Handle instance,
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t /*flags*/,
    const LV2_Feature* const* /*features*/) {
    MetaLV* self = static_cast<MetaLV*>(instance);
    if (!self || !store) return LV2_STATE_ERR_UNKNOWN;

    const uint32_t flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;

    for (int s = 0; s < kSlotCount; ++s) {
        std::string uri = self->engine ? self->engine->slot_uri(s) : std::string();
        if (!uri.empty()) {
            store(handle, self->urids.state_slot_uri[s],
                  uri.c_str(), uri.size() + 1, self->urids.atom_String, flags);
        }

        int32_t mapping[kBankSize];
        for (int b = 0; b < kBankSize; ++b) {
            mapping[b] = self->engine ? self->engine->mapping(s, b) : -1;
        }
        store(handle, self->urids.state_slot_map[s],
              mapping, sizeof(mapping), self->urids.atom_Int, flags);
    }

    return LV2_STATE_SUCCESS;
}

static LV2_State_Status metalv_state_restore(
    LV2_Handle instance,
    LV2_State_Retrieve_Function retrieve,
    LV2_State_Handle handle,
    uint32_t /*flags*/,
    const LV2_Feature* const* /*features*/) {
    MetaLV* self = static_cast<MetaLV*>(instance);
    if (!self || !retrieve || !self->engine) return LV2_STATE_ERR_UNKNOWN;

    for (int s = 0; s < kSlotCount; ++s) {
        size_t size = 0;
        uint32_t type = 0;
        uint32_t val_flags = 0;

        const void* uri_data = retrieve(handle, self->urids.state_slot_uri[s], &size, &type, &val_flags);
        if (uri_data && type == self->urids.atom_String) {
            const char* uri = static_cast<const char*>(uri_data);
            if (uri && *uri) {
                self->engine->load_slot(s, uri);
            }
        }

        const void* map_data = retrieve(handle, self->urids.state_slot_map[s], &size, &type, &val_flags);
        if (map_data && type == self->urids.atom_Int && size >= sizeof(int32_t) * kBankSize) {
            const int32_t* mapping = static_cast<const int32_t*>(map_data);
            for (int b = 0; b < kBankSize; ++b) {
                if (mapping[b] >= 0) {
                    self->engine->map_param(s, b, mapping[b]);
                }
            }
        }
    }

    return LV2_STATE_SUCCESS;
}

static const LV2_State_Interface metalv_state_interface = {
    metalv_state_save,
    metalv_state_restore
};

static LV2_Handle instantiate(const LV2_Descriptor* /*descriptor*/,
                              double rate,
                              const char* /*bundle_path*/,
                              const LV2_Feature* const* features) {
    MetaLV* self = new MetaLV();
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
    self->urids.atom_String = self->map->map(self->map->handle, LV2_ATOM__String);
    self->urids.atom_Int = self->map->map(self->map->handle, LV2_ATOM__Int);
    self->urids.midi_Event = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);

    for (int s = 0; s < kSlotCount; ++s) {
        std::string uri_key = std::string(METALV_URI) + "#slot_" + std::to_string(s) + "_uri";
        std::string map_key = std::string(METALV_URI) + "#slot_" + std::to_string(s) + "_map";
        self->urids.state_slot_uri[s] = self->map->map(self->map->handle, uri_key.c_str());
        self->urids.state_slot_map[s] = self->map->map(self->map->handle, map_key.c_str());
    }

    self->engine = new HostEngine(rate, kSlotCount, kBankSize, self->urids.midi_Event, self->urids.atom_Sequence);

    MCPServer::Backend backend = MCPServer::Backend::Tcp;
    int tcp_port = 5566;
    const char* env_port = std::getenv("METALV_MCP_TCP");
    if (env_port && *env_port) {
        tcp_port = std::atoi(env_port);
        if (tcp_port <= 0) {
            backend = MCPServer::Backend::Stdio;
            tcp_port = 0;
        }
    }

    self->mcp = new MCPServer(self->engine, backend, tcp_port);
    self->mcp->start();

    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    MetaLV* self = static_cast<MetaLV*>(instance);
    if (!self) return;

    if (port == PORT_IN_L) {
        self->in_l = static_cast<const float*>(data);
        return;
    }
    if (port == PORT_IN_R) {
        self->in_r = static_cast<const float*>(data);
        return;
    }
    if (port == PORT_OUT_L) {
        self->out_l = static_cast<float*>(data);
        return;
    }
    if (port == PORT_OUT_R) {
        self->out_r = static_cast<float*>(data);
        return;
    }
    if (port == PORT_MIDI_IN) {
        self->midi_in = static_cast<const LV2_Atom_Sequence*>(data);
        return;
    }
    if (port == PORT_MIDI_OUT) {
        self->midi_out = static_cast<LV2_Atom_Sequence*>(data);
        return;
    }

    for (int s = 0; s < kSlotCount; ++s) {
        int base = slot_port_base(s);
        if (port == base) {
            self->slot_bypass[s] = static_cast<const float*>(data);
            return;
        }
        if (port == base + 1) {
            self->slot_gain[s] = static_cast<const float*>(data);
            return;
        }
        int param_base = base + 2;
        if (port >= param_base && port < param_base + kBankSize) {
            int idx = port - param_base;
            self->slot_params[s][idx] = static_cast<const float*>(data);
            return;
        }
    }
}

static void activate(LV2_Handle instance) {
    MetaLV* self = static_cast<MetaLV*>(instance);
    if (!self) return;
    self->frame_time = 0;
}

static void run(LV2_Handle instance, uint32_t nframes) {
    MetaLV* self = static_cast<MetaLV*>(instance);
    if (!self || !self->engine || !self->out_l || !self->out_r) return;

    std::memset(self->out_l, 0, sizeof(float) * nframes);
    std::memset(self->out_r, 0, sizeof(float) * nframes);

    for (int s = 0; s < kSlotCount; ++s) {
        float bypass = self->slot_bypass[s] ? *self->slot_bypass[s] : 0.0f;
        float gain = self->slot_gain[s] ? *self->slot_gain[s] : 1.0f;
        self->engine->set_slot_bypass_gain(s, bypass, gain);

        float bank[kBankSize];
        for (int b = 0; b < kBankSize; ++b) {
            bank[b] = self->slot_params[s][b] ? *self->slot_params[s][b] : 0.0f;
        }
        self->engine->set_slot_params(s, bank, kBankSize);
    }

    self->engine->process(self->in_l, self->in_r, self->out_l, self->out_r,
                          self->midi_in, self->midi_out, nframes, self->frame_time);

    self->frame_time += nframes;
}

static void deactivate(LV2_Handle /*instance*/) {}

static void cleanup(LV2_Handle instance) {
    MetaLV* self = static_cast<MetaLV*>(instance);
    if (!self) return;

    if (self->mcp) {
        self->mcp->stop();
        delete self->mcp;
    }
    delete self->engine;
    delete self;
}

static const void* extension_data(const char* uri) {
    if (!std::strcmp(uri, LV2_STATE__interface)) {
        return &metalv_state_interface;
    }
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    METALV_URI,
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
