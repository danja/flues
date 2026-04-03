#pragma once

#include "session_registry.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <thread>

namespace outsider {

class SessionPersistence {
public:
    explicit SessionPersistence(SessionRegistry& registry);
    ~SessionPersistence();

    void load();
    void start();
    void stop();

    const std::filesystem::path& state_path() const;

private:
    void thread_main();
    void save_if_dirty(bool force);
    bool write_state_file(const PersistedSessionState& state);
    static std::filesystem::path resolve_state_path();
    static bool read_state_file(const std::filesystem::path& path,
                                PersistedSessionState* out);

    SessionRegistry& registry_;
    std::filesystem::path state_path_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::uint64_t last_saved_revision_ = 0;
};

}  // namespace outsider
