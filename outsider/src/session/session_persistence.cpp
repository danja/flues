#include "session_persistence.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>

namespace outsider {

namespace {

constexpr const char* kPersistenceEnv = "OUTSIDER_SESSION_FILE";
constexpr const char* kPersistenceHeader = "# outsider-session-v1";

}  // namespace

SessionPersistence::SessionPersistence(SessionRegistry& registry)
    : registry_(registry),
      state_path_(resolve_state_path()) {}

SessionPersistence::~SessionPersistence() {
    stop();
}

void SessionPersistence::load() {
    PersistedSessionState state{};
    if (!read_state_file(state_path_, &state)) {
        last_saved_revision_ = registry_.persistent_revision();
        return;
    }

    registry_.restore_persisted_state(state);
    last_saved_revision_ = registry_.persistent_revision();
    std::fprintf(stderr, "outsider: loaded session state from %s\n", state_path_.c_str());
}

void SessionPersistence::start() {
    if (running_.exchange(true)) {
        return;
    }

    last_saved_revision_ = registry_.persistent_revision();
    thread_ = std::thread(&SessionPersistence::thread_main, this);
}

void SessionPersistence::stop() {
    running_.store(false);
    if (thread_.joinable()) {
        thread_.join();
    }
    save_if_dirty(true);
}

const std::filesystem::path& SessionPersistence::state_path() const {
    return state_path_;
}

void SessionPersistence::thread_main() {
    while (running_.load()) {
        save_if_dirty(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void SessionPersistence::save_if_dirty(bool force) {
    const std::uint64_t revision = registry_.persistent_revision();
    if (!force && revision == last_saved_revision_) {
        return;
    }

    const PersistedSessionState state = registry_.persistent_snapshot();
    if (write_state_file(state)) {
        last_saved_revision_ = revision;
    }
}

bool SessionPersistence::write_state_file(const PersistedSessionState& state) {
    std::error_code ec;
    const std::filesystem::path parent = state_path_.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            std::fprintf(stderr,
                         "outsider: failed to create state directory %s: %s\n",
                         parent.c_str(),
                         ec.message().c_str());
            return false;
        }
    }

    const std::filesystem::path temp_path = state_path_.string() + ".tmp";
    std::ofstream out(temp_path);
    if (!out.is_open()) {
        std::fprintf(stderr, "outsider: failed to open temp state file %s\n", temp_path.c_str());
        return false;
    }

    out << kPersistenceHeader << "\n";
    out << "selected_session " << state.selected_session << "\n";
    out << std::fixed << std::setprecision(6);
    for (const PersistedEndpointState& endpoint : state.endpoints) {
        out << "endpoint "
            << endpoint.session_slot << ' '
            << endpoint.endpoint_slot << ' '
            << mode_wire_name(endpoint.mode) << ' '
            << endpoint.p_mix_params.granularity_bars << ' '
            << endpoint.p_mix_params.maintain_weight << ' '
            << endpoint.p_mix_params.fade_weight << ' '
            << endpoint.p_mix_params.cut_weight << ' '
            << endpoint.p_mix_params.fade_dur_max_fraction << ' '
            << endpoint.p_mix_params.bias_percent << ' '
            << endpoint.e_mix_params.total_bars << ' '
            << endpoint.e_mix_params.division << ' '
            << endpoint.e_mix_params.steps << ' '
            << endpoint.e_mix_params.offset << ' '
            << endpoint.e_mix_params.fade_bars << '\n';
    }
    out.close();

    if (!out) {
        std::fprintf(stderr, "outsider: failed while writing state file %s\n", temp_path.c_str());
        return false;
    }

    std::filesystem::rename(temp_path, state_path_, ec);
    if (ec) {
        std::fprintf(stderr,
                     "outsider: failed to replace state file %s: %s\n",
                     state_path_.c_str(),
                     ec.message().c_str());
        std::filesystem::remove(temp_path, ec);
        return false;
    }

    return true;
}

std::filesystem::path SessionPersistence::resolve_state_path() {
    if (const char* override_path = std::getenv(kPersistenceEnv)) {
        if (override_path[0] != '\0') {
            return std::filesystem::path(override_path);
        }
    }

    if (const char* xdg = std::getenv("XDG_CONFIG_HOME")) {
        if (xdg[0] != '\0') {
            return std::filesystem::path(xdg) / "flues" / "outsider-session.state";
        }
    }

    if (const char* home = std::getenv("HOME")) {
        if (home[0] != '\0') {
            return std::filesystem::path(home) / ".config" / "flues" / "outsider-session.state";
        }
    }

    return std::filesystem::current_path() / "outsider-session.state";
}

bool SessionPersistence::read_state_file(const std::filesystem::path& path,
                                         PersistedSessionState* out) {
    if (!out || !std::filesystem::exists(path)) {
        return false;
    }

    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    PersistedSessionState state{};
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream iss(line);
        std::string tag;
        iss >> tag;
        if (tag == "selected_session") {
            int selected = 1;
            if (iss >> selected) {
                if (selected > 0 && selected <= 65535) {
                    state.selected_session = static_cast<std::uint16_t>(selected);
                }
            }
            continue;
        }

        if (tag != "endpoint") {
            continue;
        }

        PersistedEndpointState endpoint{};
        std::string mode_text;
        int session_slot = 0;
        int endpoint_slot = 0;
        if (!(iss >> session_slot >> endpoint_slot >> mode_text)) {
            continue;
        }
        if (session_slot < 1 || session_slot > 65535 || endpoint_slot < 1 || endpoint_slot > 65535) {
            continue;
        }
        if (!parse_mode_wire(mode_text, &endpoint.mode)) {
            continue;
        }

        if (!(iss >> endpoint.p_mix_params.granularity_bars
                  >> endpoint.p_mix_params.maintain_weight
                  >> endpoint.p_mix_params.fade_weight
                  >> endpoint.p_mix_params.cut_weight
                  >> endpoint.p_mix_params.fade_dur_max_fraction
                  >> endpoint.p_mix_params.bias_percent
                  >> endpoint.e_mix_params.total_bars
                  >> endpoint.e_mix_params.division
                  >> endpoint.e_mix_params.steps
                  >> endpoint.e_mix_params.offset
                  >> endpoint.e_mix_params.fade_bars)) {
            continue;
        }

        endpoint.session_slot = static_cast<std::uint16_t>(session_slot);
        endpoint.endpoint_slot = static_cast<std::uint16_t>(endpoint_slot);
        state.endpoints.push_back(endpoint);
    }

    *out = state;
    return true;
}

}  // namespace outsider
