#include "outsider_client_net.hpp"

#include "outsider/control_protocol.hpp"
#include "outsider/protocol.hpp"
#include "outsider/websocket_protocol.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace outsider_client {

void OutsiderClientNet::attach_queues(SpscQueue<outsider::TransportSnapshot, 128>* outbound_transport,
                                      SpscQueue<StatusSnapshot, 128>* outbound_status,
                                      SpscQueue<outsider::CommandPacket, 32>* inbound_commands,
                                      SpscQueue<ServerParamsSnapshot, 16>* inbound_params) {
    std::lock_guard<std::mutex> lock(mutex_);
    outbound_transport_ = outbound_transport;
    outbound_status_ = outbound_status;
    inbound_commands_ = inbound_commands;
    inbound_params_ = inbound_params;
}

void OutsiderClientNet::configure(std::uint16_t session_slot,
                                  std::uint16_t endpoint_slot,
                                  bool authority,
                                  bool reconnect) {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool identity_changed =
        session_slot_ != session_slot ||
        endpoint_slot_ != endpoint_slot ||
        authority_requested_ != authority;

    session_slot_ = session_slot;
    endpoint_slot_ = endpoint_slot;
    authority_requested_ = authority;
    reconnect_enabled_ = reconnect;

    if (identity_changed && connected_.load()) {
        close_connection();
    }
}

void OutsiderClientNet::start() {
    if (running_.exchange(true)) {
        return;
    }
    thread_ = std::thread(&OutsiderClientNet::thread_main, this);
}

void OutsiderClientNet::stop() {
    running_.store(false);
    if (thread_.joinable()) {
        thread_.join();
    }
    close_connection();
}

bool OutsiderClientNet::connected() const {
    return connected_.load();
}

bool OutsiderClientNet::server_seen() const {
    return server_seen_.load();
}

bool OutsiderClientNet::authority_active() const {
    return authority_active_.load();
}

std::uint64_t OutsiderClientNet::monotonic_time_ms() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

bool OutsiderClientNet::open_connection() {
    sock_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd_ < 0) {
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(outsider::kDefaultListenPort);
    if (::inet_pton(AF_INET, outsider::kDefaultListenHost, &addr.sin_addr) != 1) {
        ::close(sock_fd_);
        sock_fd_ = -1;
        return false;
    }

    if (::connect(sock_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(sock_fd_);
        sock_fd_ = -1;
        return false;
    }

    const std::uint64_t now_ms = monotonic_time_ms();
    mask_seed_ = static_cast<std::uint32_t>(now_ms ^ (static_cast<std::uint64_t>(session_slot_) << 16) ^
                                            static_cast<std::uint64_t>(endpoint_slot_));
    const std::string client_key = outsider::websocket_make_client_key(mask_seed_);
    const std::string request = outsider::websocket_build_client_request(
        outsider::kDefaultListenHost,
        outsider::kDefaultListenPort,
        outsider::kDefaultListenPath,
        client_key);
    if (!send_raw(request)) {
        ::close(sock_fd_);
        sock_fd_ = -1;
        return false;
    }

    std::string response;
    if (!receive_handshake_response(&response) ||
        !outsider::websocket_validate_server_response(response,
                                                      outsider::websocket_accept_value(client_key))) {
        ::close(sock_fd_);
        sock_fd_ = -1;
        return false;
    }

    ::fcntl(sock_fd_, F_SETFL, ::fcntl(sock_fd_, F_GETFL, 0) | O_NONBLOCK);
    const std::size_t header_end = response.find("\r\n\r\n");
    recv_buffer_.assign(header_end == std::string::npos ? "" : response.substr(header_end + 4u));
    hello_sent_ = false;
    heartbeat_interval_ms_ = 750;
    connected_.store(true);
    last_rx_ms_ = now_ms;
    last_tx_ms_ = 0;
    return true;
}

void OutsiderClientNet::close_connection() {
    if (sock_fd_ >= 0) {
        ::close(sock_fd_);
        sock_fd_ = -1;
    }
    recv_buffer_.clear();
    hello_sent_ = false;
    connected_.store(false);
    authority_active_.store(false);
}

bool OutsiderClientNet::send_raw(const std::string& bytes) {
    if (sock_fd_ < 0) {
        return false;
    }

    const char* data = bytes.c_str();
    std::size_t remaining = bytes.size();
    while (remaining > 0) {
        const ssize_t sent = ::send(sock_fd_, data, remaining, MSG_NOSIGNAL);
        if (sent > 0) {
            data += sent;
            remaining -= static_cast<std::size_t>(sent);
            continue;
        }
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            continue;
        }
        return false;
    }
    last_tx_ms_ = monotonic_time_ms();
    return true;
}

bool OutsiderClientNet::receive_handshake_response(std::string* response) {
    if (!response || sock_fd_ < 0) {
        return false;
    }

    timeval timeout{};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    ::setsockopt(sock_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    ::setsockopt(sock_fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    response->clear();
    char buffer[2048];
    while (response->find("\r\n\r\n") == std::string::npos) {
        const ssize_t n = ::recv(sock_fd_, buffer, sizeof(buffer), 0);
        if (n > 0) {
            response->append(buffer, static_cast<std::size_t>(n));
            continue;
        }
        if (n == 0) {
            return false;
        }
        if (errno == EINTR) {
            continue;
        }
        return false;
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    ::setsockopt(sock_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    ::setsockopt(sock_fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    return true;
}

void OutsiderClientNet::send_frame(outsider::WebSocketOpcode opcode,
                                   const std::string& payload) {
    if (sock_fd_ < 0) {
        return;
    }

    mask_seed_ = outsider::websocket_xorshift32(mask_seed_ + 0x9e3779b9u);
    const std::string frame = outsider::websocket_encode_frame(opcode, payload, true, mask_seed_);
    if (!send_raw(frame)) {
        close_connection();
    }
}

void OutsiderClientNet::send_line(const std::string& line) {
    send_frame(outsider::WebSocketOpcode::Text, line);
}

void OutsiderClientNet::poll_read() {
    if (sock_fd_ < 0) {
        return;
    }

    while (true) {
        char buffer[2048];
        const ssize_t n = ::recv(sock_fd_, buffer, sizeof(buffer), 0);
        if (n > 0) {
            recv_buffer_.append(buffer, static_cast<std::size_t>(n));
            last_rx_ms_ = monotonic_time_ms();
            server_seen_.store(true);
            continue;
        }
        if (n == 0) {
            close_connection();
            return;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        close_connection();
        return;
    }

    while (true) {
        outsider::WebSocketFrame frame{};
        std::size_t consumed = 0;
        const outsider::WebSocketParseResult result =
            outsider::websocket_decode_frame(recv_buffer_, false, &consumed, &frame);
        if (result == outsider::WebSocketParseResult::Incomplete) {
            break;
        }
        if (result == outsider::WebSocketParseResult::ProtocolError) {
            close_connection();
            return;
        }

        recv_buffer_.erase(0, consumed);
        if (frame.opcode == outsider::WebSocketOpcode::Ping) {
            send_frame(outsider::WebSocketOpcode::Pong, frame.payload);
            continue;
        }
        if (frame.opcode == outsider::WebSocketOpcode::Close) {
            close_connection();
            return;
        }
        if (frame.opcode != outsider::WebSocketOpcode::Text || frame.payload.empty()) {
            continue;
        }

        outsider::ControlMessage message{};
        if (!outsider::parse_control_message(frame.payload, &message)) {
            continue;
        }

        switch (message.type) {
            case outsider::ControlMessageType::Welcome:
                server_seen_.store(true);
                authority_active_.store(message.authority_accepted);
                heartbeat_interval_ms_ = std::max<std::uint32_t>(250, message.heartbeat_interval_ms);
                break;

            case outsider::ControlMessageType::Command:
                if (inbound_commands_) {
                    inbound_commands_->push(message.command);
                }
                break;

            case outsider::ControlMessageType::Params:
                if (inbound_params_) {
                    ServerParamsSnapshot snapshot{};
                    snapshot.mode = message.mode;
                    snapshot.p_mix_granularity_bars = message.p_mix_params.granularity_bars;
                    snapshot.p_mix_bias_percent = message.p_mix_params.bias_percent;
                    snapshot.e_mix_steps = message.e_mix_params.steps;
                    snapshot.e_mix_division = message.e_mix_params.division;
                    snapshot.e_mix_offset = message.e_mix_params.offset;
                    inbound_params_->push(snapshot);
                }
                break;

            case outsider::ControlMessageType::Heartbeat:
                server_seen_.store(true);
                break;

            case outsider::ControlMessageType::Error:
                if (message.fatal) {
                    close_connection();
                    return;
                }
                break;

            default:
                break;
        }
    }
}

void OutsiderClientNet::thread_main() {
    outsider::TransportSnapshot latest_transport{};
    StatusSnapshot latest_status{};

    while (running_.load()) {
        std::uint16_t session_slot = 1;
        std::uint16_t endpoint_slot = 1;
        bool authority_requested = false;
        bool reconnect_enabled = true;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            session_slot = session_slot_;
            endpoint_slot = endpoint_slot_;
            authority_requested = authority_requested_;
            reconnect_enabled = reconnect_enabled_;
        }

        const std::uint64_t now_ms = monotonic_time_ms();

        if (!connected_.load()) {
            if (!reconnect_enabled) {
                ::usleep(10000);
                continue;
            }
            if (now_ms < last_connect_attempt_ms_ + 500) {
                ::usleep(10000);
                continue;
            }
            last_connect_attempt_ms_ = now_ms;
            open_connection();
            if (!connected_.load()) {
                ::usleep(10000);
                continue;
            }
        }

        if (connected_.load() && !hello_sent_) {
            send_line(outsider::encode_hello_message(session_slot, endpoint_slot, authority_requested));
            hello_sent_ = connected_.load();
        }

        if (outbound_transport_) {
            while (outbound_transport_->pop(&latest_transport)) {
            }
            if (latest_transport.block_counter > 0 && connected_.load()) {
                latest_transport.session_slot = session_slot;
                latest_transport.endpoint_slot = endpoint_slot;
                latest_transport.authority = authority_requested;
                send_line(outsider::encode_transport_message(latest_transport));
            }
        }

        if (outbound_status_) {
            bool have_status = false;
            while (outbound_status_->pop(&latest_status)) {
                have_status = true;
            }
            if (have_status && connected_.load()) {
                latest_status.session_slot = session_slot;
                latest_status.endpoint_slot = endpoint_slot;
                send_line(outsider::encode_status_message(session_slot,
                                                         endpoint_slot,
                                                         connected_.load(),
                                                         latest_status.current_gain,
                                                         latest_status.current_state,
                                                         latest_status.last_command_id,
                                                         latest_status.peak_l,
                                                         latest_status.peak_r));
            }
        }

        poll_read();

        if (connected_.load() && now_ms > last_tx_ms_ + heartbeat_interval_ms_) {
            send_line(outsider::encode_heartbeat_message(session_slot, endpoint_slot, 0));
        }

        ::usleep(5000);
    }

    std::uint16_t session_slot = 1;
    std::uint16_t endpoint_slot = 1;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_slot = session_slot_;
        endpoint_slot = endpoint_slot_;
    }
    if (connected_.load()) {
        send_line(outsider::encode_goodbye_message(session_slot, endpoint_slot));
        send_frame(outsider::WebSocketOpcode::Close, "");
    }
}

}  // namespace outsider_client
