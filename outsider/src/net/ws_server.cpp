#include "ws_server.hpp"

#include "outsider/protocol.hpp"

namespace outsider {

WsServerStub::WsServerStub()
    : listen_uri_(kDefaultListenUri) {}

void WsServerStub::start() {
    running_ = true;
    log_event(std::string("Stub transport listening at ") + listen_uri_);
    log_event("Networking not implemented yet: UI/demo scaffold only.");
}

void WsServerStub::stop() {
    if (!running_) {
        return;
    }
    log_event("Stub transport stopped.");
    running_ = false;
}

bool WsServerStub::running() const {
    return running_;
}

const std::string& WsServerStub::listen_uri() const {
    return listen_uri_;
}

const std::vector<std::string>& WsServerStub::recent_events() const {
    return recent_events_;
}

void WsServerStub::log_event(const std::string& message) {
    recent_events_.push_back(message);
    if (recent_events_.size() > 8) {
        recent_events_.erase(recent_events_.begin());
    }
}

}  // namespace outsider

