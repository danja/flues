#include "ws_server.hpp"

#include "../semaphore/semaphore_engine.hpp"
#include "../session/session_registry.hpp"
#include "../transport/transport_authority.hpp"
#include "outsider/control_protocol.hpp"
#include "outsider/protocol.hpp"
#include "outsider/transport_snapshot.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <string>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace outsider {

WsServerStub::WsServerStub(SessionRegistry& registry,
                           TransportAuthority& authority,
                           const SemaphoreEngine& semaphore)
    : registry_(registry),
      authority_(authority),
      semaphore_(semaphore),
      listen_uri_(kDefaultListenUri) {}

WsServerStub::~WsServerStub() {
    stop();
}

void WsServerStub::start() {
    if (running_.exchange(true)) {
        return;
    }
    thread_ = std::thread(&WsServerStub::thread_main, this);
}

void WsServerStub::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool WsServerStub::running() const {
    return running_.load();
}

const std::string& WsServerStub::listen_uri() const {
    return listen_uri_;
}

std::vector<std::string> WsServerStub::recent_events_snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return recent_events_;
}

void WsServerStub::log_event(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    recent_events_.push_back(message);
    if (recent_events_.size() > 10) {
        recent_events_.erase(recent_events_.begin());
    }
}

std::uint64_t WsServerStub::monotonic_time_ms() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

std::uint64_t WsServerStub::command_key(const CommandPacket& command) {
    const std::uint64_t gain = static_cast<std::uint64_t>(std::clamp(command.target_gain, 0.0f, 1.0f) * 1000.0f);
    const std::uint64_t duration = static_cast<std::uint64_t>(std::max(command.duration_beats, 0.0f) * 1000.0f);
    std::uint64_t key = 1469598103934665603ull;
    auto mix = [&key](std::uint64_t value) {
        key ^= value + 0x9e3779b97f4a7c15ull + (key << 6) + (key >> 2);
    };
    mix(static_cast<std::uint64_t>(command.mode));
    mix(static_cast<std::uint64_t>(command.target_state));
    mix(static_cast<std::uint64_t>(command.apply_at_bar));
    mix(static_cast<std::uint64_t>(command.apply_at_step16));
    mix(gain);
    mix(duration);
    return key;
}

void WsServerStub::open_listener() {
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        log_event(std::string("Socket create failed: ") + std::strerror(errno));
        return;
    }

    const int yes = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    ::fcntl(listen_fd_, F_SETFL, ::fcntl(listen_fd_, F_GETFL, 0) | O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kDefaultListenPort);
    if (::inet_pton(AF_INET, kDefaultListenHost, &addr.sin_addr) != 1) {
        log_event("inet_pton failed for default listen host.");
        close_listener();
        return;
    }

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        log_event(std::string("Bind failed: ") + std::strerror(errno));
        close_listener();
        return;
    }

    if (::listen(listen_fd_, 8) < 0) {
        log_event(std::string("Listen failed: ") + std::strerror(errno));
        close_listener();
        return;
    }

    log_event(std::string("Control server listening on ") + kDefaultListenHost + ":" + std::to_string(kDefaultListenPort));
}

void WsServerStub::close_listener() {
    if (listen_fd_ >= 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
}

void WsServerStub::send_line(Connection& connection, const std::string& line) {
    if (connection.fd < 0) {
        return;
    }

    std::string payload = line;
    payload.push_back('\n');
    const char* data = payload.c_str();
    std::size_t remaining = payload.size();
    while (remaining > 0) {
        const ssize_t sent = ::send(connection.fd, data, remaining, MSG_NOSIGNAL);
        if (sent > 0) {
            data += sent;
            remaining -= static_cast<std::size_t>(sent);
            continue;
        }
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            continue;
        }
        break;
    }
    connection.last_tx_ms = monotonic_time_ms();
}

void WsServerStub::refresh_authority() {
    authority_.recompute(registry_);
    const AuthoritySelection current = authority_.current();
    registry_.set_authority_active(current.valid ? current.session_slot : 0,
                                   current.valid ? current.endpoint_slot : 0);

    for (Connection& connection : connections_) {
        if (!connection.hello_received || !connection.authority_requested) {
            continue;
        }
        const bool accepted = current.valid &&
                              current.session_slot == connection.session_slot &&
                              current.endpoint_slot == connection.endpoint_slot;
        if (accepted == connection.authority_accepted) {
            continue;
        }
        connection.authority_accepted = accepted;
        send_line(connection,
                  encode_welcome_message(connection.session_slot,
                                         connection.endpoint_slot,
                                         connection.authority_accepted,
                                         750));
    }
}

void WsServerStub::maybe_dispatch_commands(std::uint16_t session_slot) {
    const AuthoritySelection current = authority_.current();
    if (!current.valid || current.session_slot != session_slot) {
        return;
    }

    const std::vector<EndpointRecord> endpoints = registry_.endpoints_snapshot();
    for (const EndpointRecord& endpoint : endpoints) {
        if (!endpoint.connected || endpoint.session_slot != session_slot) {
            continue;
        }

        CommandPacket command = semaphore_.preview_for(endpoint, registry_, authority_);
        const std::uint64_t key = command_key(command);

        for (Connection& connection : connections_) {
            if (!connection.hello_received ||
                connection.session_slot != endpoint.session_slot ||
                connection.endpoint_slot != endpoint.endpoint_slot) {
                continue;
            }
            if (connection.last_command_key == key) {
                continue;
            }

            connection.last_command_key = key;
            command.command_id = next_command_id_++;
            send_line(connection,
                      encode_command_message(endpoint.session_slot,
                                             endpoint.endpoint_slot,
                                             command));
        }
    }
}

void WsServerStub::disconnect_connection(std::size_t index, const std::string& reason) {
    if (index >= connections_.size()) {
        return;
    }

    Connection connection = connections_[index];
    if (connection.hello_received) {
        registry_.mark_goodbye(connection.session_slot, connection.endpoint_slot);
        refresh_authority();
        log_event("Disconnected endpoint " +
                  std::to_string(connection.session_slot) + "." +
                  std::to_string(connection.endpoint_slot) + ": " + reason);
    }

    if (connection.fd >= 0) {
        ::close(connection.fd);
    }
    connections_.erase(connections_.begin() + static_cast<std::ptrdiff_t>(index));
}

void WsServerStub::handle_message(Connection& connection,
                                  const std::string& line,
                                  std::size_t connection_index) {
    ControlMessage msg{};
    if (!parse_control_message(line, &msg)) {
        send_line(connection, encode_error_message("bad_message", "Unable to parse message", true));
        throw std::runtime_error("bad_message");
    }

    connection.last_rx_ms = monotonic_time_ms();

    switch (msg.type) {
        case ControlMessageType::Hello: {
            if (msg.protocol_version != kProtocolVersion) {
                send_line(connection,
                          encode_error_message("protocol_version_unsupported",
                                               "Protocol version 1 required",
                                               true,
                                               msg.session_slot,
                                               msg.endpoint_slot));
                throw std::runtime_error("protocol_version_unsupported");
            }
            if (msg.session_slot == 0 || msg.endpoint_slot == 0) {
                send_line(connection,
                          encode_error_message("invalid_identity",
                                               "session_slot and endpoint_slot must be positive",
                                               true));
                throw std::runtime_error("invalid_identity");
            }

            for (std::size_t i = 0; i < connections_.size(); ++i) {
                if (i == connection_index) {
                    continue;
                }
                const Connection& other = connections_[i];
                if (other.hello_received &&
                    other.session_slot == msg.session_slot &&
                    other.endpoint_slot == msg.endpoint_slot) {
                    send_line(connection,
                              encode_error_message("duplicate_endpoint",
                                                   "Endpoint already connected",
                                                   true,
                                                   msg.session_slot,
                                                   msg.endpoint_slot));
                    throw std::runtime_error("duplicate_endpoint");
                }
            }

            connection.hello_received = true;
            connection.session_slot = msg.session_slot;
            connection.endpoint_slot = msg.endpoint_slot;
            connection.authority_requested = msg.authority;
            registry_.mark_hello(msg.session_slot, msg.endpoint_slot, msg.authority);
            refresh_authority();

            const AuthoritySelection current = authority_.current();
            connection.authority_accepted = current.valid &&
                                            current.session_slot == msg.session_slot &&
                                            current.endpoint_slot == msg.endpoint_slot;
            send_line(connection,
                      encode_welcome_message(msg.session_slot,
                                             msg.endpoint_slot,
                                             connection.authority_accepted,
                                             750));
            log_event("Hello from endpoint " + std::to_string(msg.session_slot) + "." +
                      std::to_string(msg.endpoint_slot) +
                      (msg.authority ? " (authority requested)" : ""));
            break;
        }

        case ControlMessageType::Transport: {
            if (!connection.hello_received) {
                send_line(connection, encode_error_message("bad_message", "transport before hello", true));
                throw std::runtime_error("transport_before_hello");
            }
            if (!connection.authority_accepted) {
                break;
            }

            TransportSnapshot snapshot{};
            snapshot.session_slot = msg.session_slot;
            snapshot.endpoint_slot = msg.endpoint_slot;
            snapshot.authority = connection.authority_requested;
            snapshot.playing = msg.playing;
            snapshot.block_counter = msg.block_counter;
            snapshot.bar = msg.bar;
            snapshot.beat = msg.beat;
            snapshot.beats_per_bar = msg.beats_per_bar > 0.0 ? msg.beats_per_bar : 4.0;
            snapshot.bpm = msg.bpm > 0.0 ? msg.bpm : 120.0;
            snapshot.sample_rate = msg.sample_rate > 0.0 ? msg.sample_rate : 48000.0;
            snapshot.block_size = msg.block_size > 0 ? msg.block_size : 256;
            registry_.update_transport(snapshot);
            maybe_dispatch_commands(snapshot.session_slot);
            break;
        }

        case ControlMessageType::Status: {
            if (!connection.hello_received) {
                send_line(connection, encode_error_message("bad_message", "status before hello", true));
                throw std::runtime_error("status_before_hello");
            }
            registry_.update_status(msg.session_slot,
                                    msg.endpoint_slot,
                                    msg.current_state,
                                    msg.current_gain,
                                    msg.last_command_id,
                                    msg.peak_l,
                                    msg.peak_r);
            break;
        }

        case ControlMessageType::Heartbeat: {
            break;
        }

        case ControlMessageType::Goodbye: {
            throw std::runtime_error("client_goodbye");
        }

        default:
            break;
    }
}

void WsServerStub::poll_accept() {
    if (listen_fd_ < 0) {
        return;
    }

    while (true) {
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        const int fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len);
        if (fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            log_event(std::string("Accept failed: ") + std::strerror(errno));
            return;
        }

        ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        Connection connection{};
        connection.fd = fd;
        connection.last_rx_ms = monotonic_time_ms();
        connections_.push_back(connection);
        log_event("Client connected to control server.");
    }
}

void WsServerStub::poll_connections(std::uint64_t now_ms) {
    std::size_t index = 0;
    while (index < connections_.size()) {
        Connection& connection = connections_[index];
        bool disconnected = false;
        std::string disconnect_reason;

        while (!disconnected) {
            char buffer[2048];
            const ssize_t n = ::recv(connection.fd, buffer, sizeof(buffer), 0);
            if (n > 0) {
                connection.recv_buffer.append(buffer, static_cast<std::size_t>(n));
                continue;
            }
            if (n == 0) {
                disconnected = true;
                disconnect_reason = "peer closed";
                break;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            disconnected = true;
            disconnect_reason = std::string("recv failed: ") + std::strerror(errno);
        }

        while (!disconnected) {
            const std::size_t newline = connection.recv_buffer.find('\n');
            if (newline == std::string::npos) {
                break;
            }
            std::string line = connection.recv_buffer.substr(0, newline);
            connection.recv_buffer.erase(0, newline + 1);
            if (line.empty()) {
                continue;
            }
            try {
                handle_message(connection, line, index);
            } catch (const std::runtime_error& error) {
                disconnected = true;
                disconnect_reason = error.what();
            }
        }

        if (!disconnected && connection.hello_received && now_ms > connection.last_tx_ms + 750) {
            send_line(connection,
                      encode_heartbeat_message(connection.session_slot,
                                               connection.endpoint_slot,
                                               0));
        }

        if (disconnected) {
            disconnect_connection(index, disconnect_reason);
            continue;
        }

        ++index;
    }
}

void WsServerStub::thread_main() {
    open_listener();
    if (listen_fd_ < 0) {
        running_.store(false);
        return;
    }

    while (running_.load()) {
        const std::uint64_t now = monotonic_time_ms();
        poll_accept();
        poll_connections(now);
        ::usleep(5000);
    }

    while (!connections_.empty()) {
        disconnect_connection(0, "server stopping");
    }
    close_listener();
    log_event("Control server stopped.");
}

}  // namespace outsider
