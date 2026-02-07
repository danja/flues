#include "HostEngine.hpp"

#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/midi/midi.h>

#include <algorithm>
#include <cstring>

namespace {

static float clampf(float v, float min_v, float max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

}

void HostEngine::MidiBuffer::resize(size_t bytes) {
    storage.resize(bytes);
    seq = reinterpret_cast<LV2_Atom_Sequence*>(storage.data());
}

void HostEngine::MidiBuffer::reset(LV2_URID atom_sequence_urid) {
    if (!seq) return;
    seq->atom.type = atom_sequence_urid;
    seq->atom.size = sizeof(LV2_Atom_Sequence_Body);
    seq->body.unit = 0;
    seq->body.pad = 0;
}

HostEngine::HostEngine(double sample_rate, int slot_count, int bank_size,
                       LV2_URID midi_event_urid, LV2_URID atom_sequence_urid)
    : sample_rate_(sample_rate),
      slot_count_(slot_count),
      bank_size_(bank_size),
      midi_event_urid_(midi_event_urid),
      atom_sequence_urid_(atom_sequence_urid) {
    world_ = lilv_world_new();
    lilv_world_load_all(world_);
    discover_plugins();

    slots_.resize(slot_count_);
    for (auto& slot : slots_) {
        slot.bank_map.assign(bank_size_, -1);
        slot.bank_values.assign(bank_size_, 0.0f);
        slot.bank_override.assign(bank_size_, false);
    }
}

HostEngine::~HostEngine() {
    for (int i = 0; i < slot_count_; ++i) {
        unload_slot(i);
    }
    if (world_) {
        lilv_world_free(world_);
    }
}

void HostEngine::discover_plugins() {
    plugin_uris_.clear();
    const LilvPlugins* plugins = lilv_world_get_all_plugins(world_);
    LILV_FOREACH(plugins, i, plugins) {
        const LilvPlugin* plugin = lilv_plugins_get(plugins, i);
        const LilvNode* uri = lilv_plugin_get_uri(plugin);
        if (uri && lilv_node_is_uri(uri)) {
            plugin_uris_.push_back(lilv_node_as_uri(uri));
        }
    }
}

std::vector<std::string> HostEngine::list_plugins() const {
    return plugin_uris_;
}

bool HostEngine::load_slot(int slot_index, const std::string& uri) {
    if (slot_index < 0 || slot_index >= slot_count_) return false;

    unload_slot(slot_index);
    Slot& slot = slots_[slot_index];

    LilvNode* uri_node = lilv_new_uri(world_, uri.c_str());
    if (!uri_node) return false;

    const LilvPlugins* plugins = lilv_world_get_all_plugins(world_);
    const LilvPlugin* plugin = lilv_plugins_get_by_uri(plugins, uri_node);
    lilv_node_free(uri_node);
    if (!plugin) return false;

    slot.plugin = plugin;
    slot.uri = uri;

    slot.instance = lilv_plugin_instantiate(plugin, sample_rate_, nullptr);
    if (!slot.instance) {
        slot.plugin = nullptr;
        slot.uri.clear();
        return false;
    }

    const uint32_t num_ports = lilv_plugin_get_num_ports(plugin);
    slot.audio_in_ports.clear();
    slot.audio_out_ports.clear();
    slot.midi_in_ports.clear();
    slot.midi_out_ports.clear();
    slot.control_in_ports.clear();
    slot.control_info.clear();

    LilvNode* audio_class = lilv_new_uri(world_, LV2_CORE__AudioPort);
    LilvNode* atom_class = lilv_new_uri(world_, LV2_ATOM__AtomPort);
    LilvNode* control_class = lilv_new_uri(world_, LV2_CORE__ControlPort);
    LilvNode* input_class = lilv_new_uri(world_, LV2_CORE__InputPort);
    LilvNode* output_class = lilv_new_uri(world_, LV2_CORE__OutputPort);
    LilvNode* midi_event = lilv_new_uri(world_, LV2_MIDI__MidiEvent);
    LilvNode* supports_uri = lilv_new_uri(world_, LV2_ATOM__supports);

    for (uint32_t i = 0; i < num_ports; ++i) {
        const LilvPort* port = lilv_plugin_get_port_by_index(plugin, i);
        if (lilv_port_is_a(plugin, port, audio_class)) {
            if (lilv_port_is_a(plugin, port, input_class)) {
                slot.audio_in_ports.push_back(i);
            } else if (lilv_port_is_a(plugin, port, output_class)) {
                slot.audio_out_ports.push_back(i);
            }
        } else if (lilv_port_is_a(plugin, port, atom_class)) {
            LilvNodes* supports = lilv_port_get_value(plugin, port, supports_uri);
            bool supports_midi = false;
            if (supports) {
                LILV_FOREACH(nodes, s, supports) {
                    const LilvNode* node = lilv_nodes_get(supports, s);
                    if (lilv_node_equals(node, midi_event)) {
                        supports_midi = true;
                        break;
                    }
                }
                lilv_nodes_free(supports);
            }
            if (supports_midi) {
                if (lilv_port_is_a(plugin, port, input_class)) {
                    slot.midi_in_ports.push_back(i);
                } else if (lilv_port_is_a(plugin, port, output_class)) {
                    slot.midi_out_ports.push_back(i);
                }
            }
        } else if (lilv_port_is_a(plugin, port, control_class) && lilv_port_is_a(plugin, port, input_class)) {
            slot.control_in_ports.push_back(i);
            const LilvNode* name_node = lilv_port_get_name(plugin, port);
            const char* name = name_node ? lilv_node_as_string(name_node) : "Param";
            HostPortInfo info {i, name ? name : "Param", 0.0f};

            LilvNode *def = nullptr, *min = nullptr, *max = nullptr;
            lilv_port_get_range(plugin, port, &def, &min, &max);
            if (def && lilv_node_is_float(def)) {
                info.def = lilv_node_as_float(def);
            } else if (def && lilv_node_is_int(def)) {
                info.def = (float)lilv_node_as_int(def);
            }
            if (def) lilv_node_free(def);
            if (min) lilv_node_free(min);
            if (max) lilv_node_free(max);

            slot.control_info.push_back(info);
        }
    }

    lilv_node_free(audio_class);
    lilv_node_free(atom_class);
    lilv_node_free(control_class);
    lilv_node_free(input_class);
    lilv_node_free(output_class);
    lilv_node_free(midi_event);
    lilv_node_free(supports_uri);

    slot.control_values.assign(slot.control_in_ports.size(), 0.0f);
    for (size_t i = 0; i < slot.control_values.size(); ++i) {
        slot.control_values[i] = slot.control_info[i].def;
    }

    slot.audio_out_left.assign(0, 0.0f);
    slot.audio_out_right.assign(0, 0.0f);
    slot.silent_buffer.assign(0, 0.0f);

    slot.midi_in_buf.resize(4096);
    slot.midi_out_buf.resize(4096);

    slot.active = true;
    return true;
}

void HostEngine::unload_slot(int slot_index) {
    if (slot_index < 0 || slot_index >= slot_count_) return;

    Slot& slot = slots_[slot_index];
    if (slot.instance) {
        lilv_instance_free(slot.instance);
        slot.instance = nullptr;
    }
    slot.plugin = nullptr;
    slot.uri.clear();
    slot.active = false;
}

std::string HostEngine::slot_uri(int slot) const {
    if (slot < 0 || slot >= slot_count_) return {};
    return slots_[slot].uri;
}

std::vector<HostPortInfo> HostEngine::slot_control_ports(int slot) const {
    if (slot < 0 || slot >= slot_count_) return {};
    return slots_[slot].control_info;
}

bool HostEngine::map_param(int slot, int bank_index, int control_port_index) {
    if (slot < 0 || slot >= slot_count_) return false;
    if (bank_index < 0 || bank_index >= bank_size_) return false;

    Slot& s = slots_[slot];
    if (control_port_index < 0 || control_port_index >= (int)s.control_in_ports.size()) return false;
    s.bank_map[bank_index] = control_port_index;
    return true;
}

int HostEngine::mapping(int slot, int bank_index) const {
    if (slot < 0 || slot >= slot_count_) return -1;
    if (bank_index < 0 || bank_index >= bank_size_) return -1;
    return slots_[slot].bank_map[bank_index];
}

void HostEngine::set_slot_params(int slot, const float* bank_values, int bank_size) {
    if (slot < 0 || slot >= slot_count_) return;
    Slot& s = slots_[slot];
    for (int i = 0; i < bank_size && i < bank_size_; ++i) {
        if (!s.bank_override[i]) {
            s.bank_values[i] = bank_values[i];
        }
    }
}

void HostEngine::set_slot_bypass_gain(int slot, float bypass, float gain) {
    if (slot < 0 || slot >= slot_count_) return;
    Slot& s = slots_[slot];
    s.bypass = bypass;
    s.gain = gain;
}

void HostEngine::set_param_override(int slot, int bank_index, float value) {
    if (slot < 0 || slot >= slot_count_) return;
    if (bank_index < 0 || bank_index >= bank_size_) return;
    Slot& s = slots_[slot];
    s.bank_values[bank_index] = value;
    s.bank_override[bank_index] = true;
}

void HostEngine::connect_slot_ports(Slot& slot,
                                    const float* in_l,
                                    const float* in_r,
                                    uint32_t nframes) {
    if (!slot.instance) return;

    if (slot.silent_buffer.size() < nframes) {
        slot.silent_buffer.assign(nframes, 0.0f);
    }

    if (slot.audio_out_left.size() < nframes) {
        slot.audio_out_left.assign(nframes, 0.0f);
    } else {
        std::fill(slot.audio_out_left.begin(), slot.audio_out_left.begin() + nframes, 0.0f);
    }

    if (slot.audio_out_right.size() < nframes) {
        slot.audio_out_right.assign(nframes, 0.0f);
    } else {
        std::fill(slot.audio_out_right.begin(), slot.audio_out_right.begin() + nframes, 0.0f);
    }

    for (size_t i = 0; i < slot.audio_in_ports.size(); ++i) {
        const float* buffer = (i == 0) ? in_l : (i == 1 ? in_r : slot.silent_buffer.data());
        lilv_instance_connect_port(slot.instance, slot.audio_in_ports[i], (void*)buffer);
    }

    for (size_t i = 0; i < slot.audio_out_ports.size(); ++i) {
        float* buffer = (i == 0) ? slot.audio_out_left.data() : (i == 1 ? slot.audio_out_right.data() : slot.silent_buffer.data());
        lilv_instance_connect_port(slot.instance, slot.audio_out_ports[i], buffer);
    }

    for (size_t i = 0; i < slot.control_in_ports.size(); ++i) {
        lilv_instance_connect_port(slot.instance, slot.control_in_ports[i], &slot.control_values[i]);
    }

    for (size_t i = 0; i < slot.midi_in_ports.size(); ++i) {
        lilv_instance_connect_port(slot.instance, slot.midi_in_ports[i], slot.midi_in_buf.seq);
    }

    for (size_t i = 0; i < slot.midi_out_ports.size(); ++i) {
        lilv_instance_connect_port(slot.instance, slot.midi_out_ports[i], slot.midi_out_buf.seq);
    }
}

void HostEngine::mix_slot_output(const Slot& slot, float* out_l, float* out_r, uint32_t nframes) {
    if (!out_l || !out_r) return;
    if (slot.audio_out_left.empty() && slot.audio_out_right.empty()) return;

    const float gain = slot.gain;
    const float bypass = slot.bypass;
    if (bypass >= 0.5f) return;

    for (uint32_t i = 0; i < nframes; ++i) {
        if (!slot.audio_out_left.empty()) out_l[i] += slot.audio_out_left[i] * gain;
        if (!slot.audio_out_right.empty()) out_r[i] += slot.audio_out_right[i] * gain;
    }
}

void HostEngine::merge_midi(const MidiBuffer& slot_out, LV2_Atom_Sequence* midi_out, uint32_t capacity) {
    if (!midi_out || !slot_out.seq) return;

    LV2_ATOM_SEQUENCE_FOREACH(slot_out.seq, ev) {
        if (ev->body.type != midi_event_urid_) continue;
        LV2_Atom_Event* appended = lv2_atom_sequence_append_event(midi_out, capacity, ev);
        if (!appended) break;
        uint8_t* dest = (uint8_t*)(appended + 1);
        const uint8_t* src = (const uint8_t*)(ev + 1);
        std::memcpy(dest, src, ev->body.size);
    }
}

void HostEngine::process(const float* in_l,
                         const float* in_r,
                         float* out_l,
                         float* out_r,
                         const LV2_Atom_Sequence* midi_in,
                         LV2_Atom_Sequence* midi_out,
                         uint32_t nframes,
                         uint64_t /*frame_time*/) {
    if (midi_out) {
        midi_out->atom.type = atom_sequence_urid_;
        midi_out->atom.size = sizeof(LV2_Atom_Sequence_Body);
        midi_out->body.unit = 0;
        midi_out->body.pad = 0;
    }

    for (int i = 0; i < slot_count_; ++i) {
        Slot& slot = slots_[i];
        if (!slot.active || !slot.instance) continue;

        slot.midi_in_buf.reset(atom_sequence_urid_);
        slot.midi_out_buf.reset(atom_sequence_urid_);

        if (midi_in && slot.midi_in_buf.seq) {
            const uint32_t capacity = (uint32_t)slot.midi_in_buf.storage.size();
            LV2_ATOM_SEQUENCE_FOREACH(midi_in, ev) {
                LV2_Atom_Event* appended = lv2_atom_sequence_append_event(slot.midi_in_buf.seq, capacity, ev);
                if (!appended) break;
                uint8_t* dest = (uint8_t*)(appended + 1);
                const uint8_t* src = (const uint8_t*)(ev + 1);
                std::memcpy(dest, src, ev->body.size);
            }
        }

        for (size_t b = 0; b < slot.bank_map.size(); ++b) {
            int cp = slot.bank_map[b];
            if (cp >= 0 && cp < (int)slot.control_values.size()) {
                slot.control_values[cp] = slot.bank_values[b];
            }
        }

        connect_slot_ports(slot, in_l, in_r, nframes);
        lilv_instance_run(slot.instance, nframes);

        mix_slot_output(slot, out_l, out_r, nframes);
        if (midi_out) {
            merge_midi(slot.midi_out_buf, midi_out, midi_out_capacity_);
        }
    }
}
