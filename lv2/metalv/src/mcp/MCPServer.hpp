#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>
#include <mutex>

class HostEngine;

class MCPServer {
public:
    enum class Backend {
        Stdio,
        Tcp
    };

    MCPServer(HostEngine* engine, Backend backend, int tcp_port);
    ~MCPServer();

    void start();
    void stop();

private:
    void run();
    void handle_line(const std::string& line);

    void run_stdio();
    void run_tcp();

    static bool extract_string(const std::string& src, const char* key, std::string& out);
    static bool extract_int(const std::string& src, const char* key, int& out);
    static bool extract_float(const std::string& src, const char* key, float& out);

    HostEngine* engine_;
    Backend backend_;
    int tcp_port_;
    int client_fd_ = -1;
    std::mutex write_mutex_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};
