#pragma once

#include <cstdint>

namespace outsider {

struct TransportSnapshot {
    std::uint16_t session_slot = 1;
    std::uint16_t endpoint_slot = 0;
    bool authority = false;
    bool playing = false;
    std::uint64_t block_counter = 0;
    double bar = 0.0;
    double beat = 0.0;
    double beats_per_bar = 4.0;
    double bpm = 120.0;
    double sample_rate = 48000.0;
    std::uint32_t block_size = 256;
};

}  // namespace outsider

