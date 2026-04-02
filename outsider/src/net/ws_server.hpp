#pragma once

#include "outsider/command_packet.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace outsider {

class SemaphoreEngine;
class SessionRegistry;
class TransportAuthority;

class WsServerStub {
public:
    WsServerStub(SessionRegistry& registry,
                 TransportAuthority& authority,
                 const SemaphoreEngine& semaphore);
    ~WsServerStub();

    void start();
    void stop();
    bool running() const;

    const std::string& listen_uri() const;
    std::vector<std::string> recent_events_snapshot() const;
    void log_event(const std::string& message);

private:
    struct Connection {
        int fd = -1;
        std::string recv_buffer;
        std::uint16_t session_slot = 0;
        std::uint16_t endpoint_slot = 0;
        bool hello_received = false;
        bool authority_requested = false;
        bool authority_accepted = false;
        std::uint64_t last_rx_ms = 0;
        std::uint64_t last_tx_ms = 0;
        std::uint64_t last_command_key = 0;
        std::uint64_t last_params_revision_sent = 0;
    };

    void thread_main();
    void open_listener();
    void close_listener();
    void poll_accept();
    void poll_connections(std::uint64_t now_ms);
    void handle_message(Connection& connection, const std::string& line, std::size_t connection_index);
    void disconnect_connection(std::size_t index, const std::string& reason);
    void refresh_authority();
    void maybe_dispatch_commands(std::uint16_t session_slot);
    void maybe_send_params(Connection& connection);
    void maybe_send_preview_command(Connection& connection);
    void send_line(Connection& connection, const std::string& line);
    static std::uint64_t monotonic_time_ms();
    static std::uint64_t command_key(const CommandPacket& command);

    SessionRegistry& registry_;
    TransportAuthority& authority_;
    const SemaphoreEngine& semaphore_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    int listen_fd_ = -1;
    mutable std::mutex mutex_;
    std::string listen_uri_;
    std::vector<std::string> recent_events_;
    std::vector<Connection> connections_;
    std::uint64_t next_command_id_ = 1;
};

}  // namespace outsider
