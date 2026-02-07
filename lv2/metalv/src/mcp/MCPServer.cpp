#include "MCPServer.hpp"
#include "../host/HostEngine.hpp"

#include <cstdio>
#include <sstream>
#include <sys/select.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mutex>

MCPServer::MCPServer(HostEngine* engine, Backend backend, int tcp_port)
    : engine_(engine), backend_(backend), tcp_port_(tcp_port) {}

MCPServer::~MCPServer() {
    stop();
}

void MCPServer::start() {
    if (running_.exchange(true)) {
        return;
    }
    thread_ = std::thread(&MCPServer::run, this);
}

void MCPServer::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool MCPServer::extract_string(const std::string& src, const char* key, std::string& out) {
    std::string pattern = std::string("\"") + key + "\"";
    size_t pos = src.find(pattern);
    if (pos == std::string::npos) return false;
    pos = src.find(':', pos);
    if (pos == std::string::npos) return false;
    pos = src.find('"', pos);
    if (pos == std::string::npos) return false;
    size_t end = src.find('"', pos + 1);
    if (end == std::string::npos) return false;
    out = src.substr(pos + 1, end - pos - 1);
    return true;
}

bool MCPServer::extract_int(const std::string& src, const char* key, int& out) {
    std::string pattern = std::string("\"") + key + "\"";
    size_t pos = src.find(pattern);
    if (pos == std::string::npos) return false;
    pos = src.find(':', pos);
    if (pos == std::string::npos) return false;
    size_t start = src.find_first_of("-0123456789", pos);
    if (start == std::string::npos) return false;
    size_t end = src.find_first_not_of("-0123456789", start);
    out = std::atoi(src.substr(start, end - start).c_str());
    return true;
}

bool MCPServer::extract_float(const std::string& src, const char* key, float& out) {
    std::string pattern = std::string("\"") + key + "\"";
    size_t pos = src.find(pattern);
    if (pos == std::string::npos) return false;
    pos = src.find(':', pos);
    if (pos == std::string::npos) return false;
    size_t start = src.find_first_of("-0123456789.", pos);
    if (start == std::string::npos) return false;
    size_t end = src.find_first_not_of("-0123456789.", start);
    out = std::atof(src.substr(start, end - start).c_str());
    return true;
}

void MCPServer::handle_line(const std::string& line) {
    if (!engine_) return;

    auto send = [&](const std::string& payload) {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (backend_ == Backend::Stdio) {
            std::printf("%s\n", payload.c_str());
            std::fflush(stdout);
            return;
        }
        if (client_fd_ >= 0) {
            std::string line = payload + "\n";
            ::write(client_fd_, line.c_str(), line.size());
            return;
        }
        std::printf("%s\n", payload.c_str());
        std::fflush(stdout);
    };

    std::string cmd;
    if (!extract_string(line, "cmd", cmd)) {
        send("{\"ok\":false,\"error\":\"missing cmd\"}");
        return;
    }

    if (cmd == "list_plugins") {
        auto list = engine_->list_plugins();
        std::string out = "{\"ok\":true,\"plugins\":[";
        for (size_t i = 0; i < list.size(); ++i) {
            out += "\"";
            out += list[i];
            out += "\"";
            if (i + 1 < list.size()) out += ",";
        }
        out += "]}";
        send(out);
        return;
    }

    if (cmd == "load_slot") {
        int slot = -1;
        std::string uri;
        if (!extract_int(line, "slot", slot) || !extract_string(line, "uri", uri)) {
            send("{\"ok\":false,\"error\":\"missing slot/uri\"}");
            return;
        }
        bool ok = engine_->load_slot(slot, uri);
        send(std::string("{\"ok\":") + (ok ? "true" : "false") + "}");
        return;
    }

    if (cmd == "list_slot_ports") {
        int slot = -1;
        if (!extract_int(line, "slot", slot)) {
            send("{\"ok\":false,\"error\":\"missing slot\"}");
            return;
        }
        auto ports = engine_->slot_control_ports(slot);
        std::string out = "{\"ok\":true,\"ports\":[";
        for (size_t i = 0; i < ports.size(); ++i) {
            out += "{\"index\":";
            out += std::to_string(ports[i].index);
            out += ",\"name\":\"";
            out += ports[i].name;
            out += "\"}";
            if (i + 1 < ports.size()) out += ",";
        }
        out += "]}";
        send(out);
        return;
    }

    if (cmd == "map_param") {
        int slot = -1;
        int bank = -1;
        int port = -1;
        if (!extract_int(line, "slot", slot) || !extract_int(line, "bank", bank) || !extract_int(line, "port", port)) {
            send("{\"ok\":false,\"error\":\"missing slot/bank/port\"}");
            return;
        }
        bool ok = engine_->map_param(slot, bank, port);
        send(std::string("{\"ok\":") + (ok ? "true" : "false") + "}");
        return;
    }

    if (cmd == "set_param") {
        int slot = -1;
        int bank = -1;
        float value = 0.0f;
        if (!extract_int(line, "slot", slot) || !extract_int(line, "bank", bank) || !extract_float(line, "value", value)) {
            send("{\"ok\":false,\"error\":\"missing slot/bank/value\"}");
            return;
        }
        engine_->set_param_override(slot, bank, value);
        send("{\"ok\":true}");
        return;
    }

    if (cmd == "slot_state") {
        int slot = -1;
        if (!extract_int(line, "slot", slot)) {
            send("{\"ok\":false,\"error\":\"missing slot\"}");
            return;
        }
        std::string uri = engine_->slot_uri(slot);
        send(std::string("{\"ok\":true,\"uri\":\"") + uri + "\"}");
        return;
    }

    send("{\"ok\":false,\"error\":\"unknown cmd\"}");
}

void MCPServer::run() {
    while (running_) {
        if (backend_ == Backend::Stdio) {
            run_stdio();
        } else {
            run_tcp();
        }
    }
}

void MCPServer::run_stdio() {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200000;

    int rc = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &tv);
    if (rc <= 0) {
        return;
    }

    if (FD_ISSET(STDIN_FILENO, &readfds)) {
        char buffer[4096];
        if (!std::fgets(buffer, sizeof(buffer), stdin)) {
            return;
        }
        std::string line(buffer);
        if (!line.empty()) {
            handle_line(line);
        }
    }
}

void MCPServer::run_tcp() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(tcp_port_));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(server_fd);
        return;
    }

    if (listen(server_fd, 1) < 0) {
        close(server_fd);
        return;
    }

    while (running_) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200000;

        int rc = select(server_fd + 1, &readfds, nullptr, nullptr, &tv);
        if (rc <= 0) {
            continue;
        }

        if (!FD_ISSET(server_fd, &readfds)) {
            continue;
        }

        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            continue;
        }
        {
            std::lock_guard<std::mutex> lock(write_mutex_);
            client_fd_ = client_fd;
        }

        char buffer[4096];
        std::string line_buf;
        while (running_) {
            fd_set client_set;
            FD_ZERO(&client_set);
            FD_SET(client_fd, &client_set);

            struct timeval ctv;
            ctv.tv_sec = 0;
            ctv.tv_usec = 200000;

            int crc = select(client_fd + 1, &client_set, nullptr, nullptr, &ctv);
            if (crc <= 0) {
                continue;
            }

            if (FD_ISSET(client_fd, &client_set)) {
                ssize_t len = read(client_fd, buffer, sizeof(buffer));
                if (len <= 0) {
                    break;
                }
                line_buf.append(buffer, buffer + len);

                size_t pos = 0;
                while ((pos = line_buf.find('\n')) != std::string::npos) {
                    std::string line = line_buf.substr(0, pos);
                    line_buf.erase(0, pos + 1);
                    if (!line.empty()) {
                        handle_line(line);
                    }
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(write_mutex_);
            client_fd_ = -1;
        }
        close(client_fd);
    }

    close(server_fd);
}
