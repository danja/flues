#pragma once

#include <cstdint>

namespace outsider_client {

class OutsiderClientNet {
public:
    void configure(std::uint16_t session_slot,
                   std::uint16_t endpoint_slot,
                   bool authority,
                   bool reconnect);
    void start();
    void stop();

    bool connected() const;
    bool server_seen() const;
    bool authority_active() const;

private:
    std::uint16_t session_slot_ = 1;
    std::uint16_t endpoint_slot_ = 1;
    bool authority_requested_ = false;
    bool reconnect_enabled_ = true;
    bool running_ = false;
};

}  // namespace outsider_client

