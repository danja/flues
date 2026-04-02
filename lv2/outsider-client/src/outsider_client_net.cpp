#include "outsider_client_net.hpp"

namespace outsider_client {

void OutsiderClientNet::configure(std::uint16_t session_slot,
                                  std::uint16_t endpoint_slot,
                                  bool authority,
                                  bool reconnect) {
    session_slot_ = session_slot;
    endpoint_slot_ = endpoint_slot;
    authority_requested_ = authority;
    reconnect_enabled_ = reconnect;
}

void OutsiderClientNet::start() {
    running_ = true;
}

void OutsiderClientNet::stop() {
    running_ = false;
}

bool OutsiderClientNet::connected() const {
    (void)session_slot_;
    (void)endpoint_slot_;
    (void)authority_requested_;
    (void)reconnect_enabled_;
    return false;
}

bool OutsiderClientNet::server_seen() const {
    return false;
}

bool OutsiderClientNet::authority_active() const {
    return false;
}

}  // namespace outsider_client

