#pragma once

namespace outsider {

class SessionRegistry;
class TransportAuthority;
class WsServerStub;
class SemaphoreEngine;

class OutsiderUiX11 {
public:
    OutsiderUiX11(SessionRegistry& registry,
                  const TransportAuthority& authority,
                  const WsServerStub& server,
                  const SemaphoreEngine& semaphore);
    ~OutsiderUiX11();

    bool open();
    int run();

private:
    class Impl;
    Impl* impl_ = nullptr;
};

}  // namespace outsider

