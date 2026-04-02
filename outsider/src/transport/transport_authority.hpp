#pragma once

#include <cstdint>
#include <mutex>

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
    AuthoritySelection current() const;

private:
    mutable std::mutex mutex_;
    AuthoritySelection current_{};
};

}  // namespace outsider
