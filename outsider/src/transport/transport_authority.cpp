#include "transport_authority.hpp"

#include "../session/session_registry.hpp"

namespace outsider {

void TransportAuthority::recompute(const SessionRegistry& registry) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_ = {};
    for (const EndpointRecord& endpoint : registry.endpoints_snapshot()) {
        if (!endpoint.connected || !endpoint.authority_claimed) {
            continue;
        }
        current_.valid = true;
        current_.session_slot = endpoint.session_slot;
        current_.endpoint_slot = endpoint.endpoint_slot;
        break;
    }
}

AuthoritySelection TransportAuthority::current() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}

}  // namespace outsider
