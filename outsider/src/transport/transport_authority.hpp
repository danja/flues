#pragma once

#include <cstdint>

namespace outsider {

class SessionRegistry;

struct AuthoritySelection {
    bool valid = false;
    std::uint16_t session_slot = 0;
    std::uint16_t endpoint_slot = 0;
};

class TransportAuthority {
public:
    void recompute(const SessionRegistry& registry);
    const AuthoritySelection& current() const;

private:
    AuthoritySelection current_{};
};

}  // namespace outsider

