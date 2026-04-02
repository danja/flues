#pragma once

#include "command_packet.hpp"
#include "transport_snapshot.hpp"

namespace outsider {

struct PMixParams {
    int granularity_bars = 4;
    float maintain_weight = 50.0f;
    float fade_weight = 25.0f;
    float cut_weight = 25.0f;
    float fade_dur_max_fraction = 1.0f;
    float bias_percent = 50.0f;
    std::uint32_t seed = 0x12345678u;
};

struct EMixParams {
    int total_bars = 128;
    int division = 16;
    int steps = 8;
    int offset = 0;
    float fade_bars = 0.0f;
};

struct SemaphoreDecision {
    CommandPacket command;
    bool active = false;
};

class PMixModel {
public:
    SemaphoreDecision preview(const TransportSnapshot& transport,
                              const PMixParams& params,
                              bool currently_audible) const;
};

class EMixModel {
public:
    SemaphoreDecision preview(const TransportSnapshot& transport,
                              const EMixParams& params) const;
};

}  // namespace outsider

