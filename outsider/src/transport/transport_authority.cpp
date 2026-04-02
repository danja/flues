#include "transport_authority.hpp"

#include "../session/session_registry.hpp"

namespace outsider {

void TransportAuthority::recompute(const SessionRegistry& registry) {
    current_ = {};
    for (const EndpointRecord& endpoint : registry.endpoints()) {
        if (!endpoint.connected || !endpoint.authority_claimed) {
            continue;
        }
        current_.valid = true;
        current_.session_slot = endpoint.session_slot;
        current_.endpoint_slot = endpoint.endpoint_slot;
        break;
    }
}

const AuthoritySelection& TransportAuthority::current() const {
    return current_;
}

}  // namespace outsider

