#pragma once

#include "outsider/command_packet.hpp"

namespace outsider {

class SessionRegistry;
class TransportAuthority;
struct EndpointRecord;

class SemaphoreEngine {
public:
    CommandPacket preview_for(const EndpointRecord& endpoint,
                              const SessionRegistry& registry,
                              const TransportAuthority& authority) const;
};

}  // namespace outsider

