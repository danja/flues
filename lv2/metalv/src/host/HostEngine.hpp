#pragma once

#include <lilv/lilv.h>
#include <lv2/atom/atom.h>
#include <lv2/urid/urid.h>

#include <cstdint>
#include <string>
#include <vector>

struct HostPortInfo {
    uint32_t index;
    std::string name;
    float def;
};

class HostEngine {
public:
    HostEngine(double sample_rate, int slot_count, int bank_size, LV2_URID midi_event_urid, LV2_URID atom_sequence_urid);
    ~HostEngine();

    std::vector<std::string> list_plugins() const;
    bool load_slot(int slot, const std::string& uri);
    void unload_slot(int slot);
    std::string slot_uri(int slot) const;

    std::vector<HostPortInfo> slot_control_ports(int slot) const;
    bool map_param(int slot, int bank_index, int control_port_index);
    int mapping(int slot, int bank_index) const;

    void set_slot_params(int slot, const float* bank_values, int bank_size);
    void set_slot_bypass_gain(int slot, float bypass, float gain);
    void set_param_override(int slot, int bank_index, float value);

    void process(const float* in_l,
                 const float* in_r,
                 float* out_l,
                 float* out_r,
                 const LV2_Atom_Sequence* midi_in,
                 LV2_Atom_Sequence* midi_out,
                 uint32_t nframes,
                 uint64_t frame_time);

private:
    struct MidiBuffer {
        std::vector<uint8_t> storage;
        LV2_Atom_Sequence* seq = nullptr;

        void resize(size_t bytes);
        void reset(LV2_URID atom_sequence_urid);
    };

    struct Slot {
        bool active = false;
        std::string uri;
        const LilvPlugin* plugin = nullptr;
        LilvInstance* instance = nullptr;

        std::vector<uint32_t> audio_in_ports;
        std::vector<uint32_t> audio_out_ports;
        std::vector<uint32_t> midi_in_ports;
        std::vector<uint32_t> midi_out_ports;
        std::vector<uint32_t> control_in_ports;
        std::vector<HostPortInfo> control_info;
        std::vector<float> control_values;
        std::vector<float> audio_out_left;
        std::vector<float> audio_out_right;
        std::vector<float> silent_buffer;

        std::vector<int> bank_map;
        std::vector<float> bank_values;
        std::vector<bool> bank_override;

        MidiBuffer midi_in_buf;
        MidiBuffer midi_out_buf;

        float bypass = 0.0f;
        float gain = 1.0f;
    };

    void discover_plugins();
    void connect_slot_ports(Slot& slot,
                            const float* in_l,
                            const float* in_r,
                            uint32_t nframes);
    void mix_slot_output(const Slot& slot, float* out_l, float* out_r, uint32_t nframes);
    void merge_midi(const MidiBuffer& slot_out, LV2_Atom_Sequence* midi_out, uint32_t capacity);

    double sample_rate_;
    int slot_count_;
    int bank_size_;

    LilvWorld* world_ = nullptr;
    std::vector<std::string> plugin_uris_;

    std::vector<Slot> slots_;

    LV2_URID midi_event_urid_;
    LV2_URID atom_sequence_urid_;
    uint32_t midi_out_capacity_ = 8192;
};
