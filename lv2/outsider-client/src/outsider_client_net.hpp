#pragma once

#include "outsider/command_packet.hpp"
#include "outsider/protocol.hpp"
#include "outsider/transport_snapshot.hpp"
#include "outsider_client_state.hpp"
#include "spsc_queue.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace outsider_client {

struct StatusSnapshot {
    std::uint16_t session_slot = 1;
    std::uint16_t endpoint_slot = 1;
    outsider::RuntimeState current_state = outsider::RuntimeState::Bypass;
    float current_gain = 1.0f;
    std::uint64_t last_command_id = 0;
    float peak_l = 0.0f;
    float peak_r = 0.0f;
};

class OutsiderClientNet {
public:
    void attach_queues(SpscQueue<outsider::TransportSnapshot, 128>* outbound_transport,
                       SpscQueue<StatusSnapshot, 128>* outbound_status,
                       SpscQueue<outsider::CommandPacket, 32>* inbound_commands,
                       SpscQueue<ServerParamsSnapshot, 16>* inbound_params);
    void configure(std::uint16_t session_slot,
                   std::uint16_t endpoint_slot,
                   bool authority,
                   bool reconnect);
    void start();
    void stop();

    bool connected() const;
    bool server_seen() const;
    bool authority_active() const;

private:
    void thread_main();
    bool open_connection();
    void close_connection();
    void send_line(const std::string& line);
    void poll_read();
    static std::uint64_t monotonic_time_ms();

    SpscQueue<outsider::TransportSnapshot, 128>* outbound_transport_ = nullptr;
    SpscQueue<StatusSnapshot, 128>* outbound_status_ = nullptr;
    SpscQueue<outsider::CommandPacket, 32>* inbound_commands_ = nullptr;
    SpscQueue<ServerParamsSnapshot, 16>* inbound_params_ = nullptr;

    std::uint16_t session_slot_ = 1;
    std::uint16_t endpoint_slot_ = 1;
    bool authority_requested_ = false;
    bool reconnect_enabled_ = true;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::atomic<bool> server_seen_{false};
    std::atomic<bool> authority_active_{false};
    std::mutex mutex_;
    std::thread thread_;
    int sock_fd_ = -1;
    bool hello_sent_ = false;
    std::uint32_t heartbeat_interval_ms_ = 750;
    std::uint64_t last_connect_attempt_ms_ = 0;
    std::uint64_t last_tx_ms_ = 0;
    std::uint64_t last_rx_ms_ = 0;
    std::string recv_buffer_;
};

}  // namespace outsider_client
