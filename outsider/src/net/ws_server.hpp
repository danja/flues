#pragma once

#include <string>
#include <vector>

namespace outsider {

class WsServerStub {
public:
    WsServerStub();

    void start();
    void stop();
    bool running() const;

    const std::string& listen_uri() const;
    const std::vector<std::string>& recent_events() const;
    void log_event(const std::string& message);

private:
    bool running_ = false;
    std::string listen_uri_;
    std::vector<std::string> recent_events_;
};

}  // namespace outsider

