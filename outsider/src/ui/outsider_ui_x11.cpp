#include "outsider_ui_x11.hpp"

#include "../net/ws_server.hpp"
#include "../semaphore/semaphore_engine.hpp"
#include "../session/session_registry.hpp"
#include "../transport/transport_authority.hpp"
#include "outsider/protocol.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

namespace outsider {

namespace {

constexpr int kWindowWidth = 1100;
constexpr int kWindowHeight = 720;

std::uint64_t monotonic_time_ms() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

void draw_text(cairo_t* cr, double x, double y, const char* text, double size, double r, double g, double b, bool bold) {
    cairo_select_font_face(cr,
                           "sans",
                           CAIRO_FONT_SLANT_NORMAL,
                           bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    cairo_set_source_rgb(cr, r, g, b);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);
}

void draw_panel(cairo_t* cr, double x, double y, double w, double h, const char* title) {
    cairo_set_source_rgb(cr, 0.12, 0.14, 0.17);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.08);
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    draw_text(cr, x + 12, y + 18, title, 13.0, 0.95, 0.95, 0.98, true);
}

void draw_badge(cairo_t* cr, double x, double y, double w, double h, const char* label, double r, double g, double b) {
    cairo_set_source_rgb(cr, r, g, b);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);
    draw_text(cr, x + 10, y + h - 7, label, 11.0, 0.05, 0.05, 0.05, true);
}

void draw_control_button(cairo_t* cr,
                         double x,
                         double y,
                         double w,
                         double h,
                         const char* label,
                         bool active) {
    cairo_set_source_rgb(cr,
                         active ? 0.42 : 0.20,
                         active ? 0.78 : 0.22,
                         active ? 0.94 : 0.26);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.14);
    cairo_rectangle(cr, x, y, w, h);
    cairo_stroke(cr);

    draw_text(cr, x + 8, y + h - 5, label, 10.0,
              active ? 0.06 : 0.86,
              active ? 0.08 : 0.90,
              active ? 0.10 : 0.94,
              true);
}

bool point_in_rect(int px, int py, double x, double y, double w, double h) {
    return px >= static_cast<int>(x) &&
           px <= static_cast<int>(x + w) &&
           py >= static_cast<int>(y) &&
           py <= static_cast<int>(y + h);
}

}  // namespace

class OutsiderUiX11::Impl {
public:
    Impl(SessionRegistry& registry,
         const TransportAuthority& authority,
         const WsServerStub& server,
         const SemaphoreEngine& semaphore)
        : registry(registry),
          authority(authority),
          server(server),
          semaphore(semaphore) {}

    ~Impl() {
        if (cr) {
            cairo_destroy(cr);
        }
        if (surface) {
            cairo_surface_destroy(surface);
        }
        if (display) {
            if (window) {
                XDestroyWindow(display, window);
            }
            XCloseDisplay(display);
        }
    }

    bool open() {
        display = XOpenDisplay(nullptr);
        if (!display) {
            return false;
        }

        screen = DefaultScreen(display);
        window = XCreateSimpleWindow(display,
                                     RootWindow(display, screen),
                                     0,
                                     0,
                                     kWindowWidth,
                                     kWindowHeight,
                                     1,
                                     BlackPixel(display, screen),
                                     BlackPixel(display, screen));
        XSelectInput(display, window, ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask);
        XStoreName(display, window, "Outsider");
        XMapWindow(display, window);

        Visual* visual = DefaultVisual(display, screen);
        surface = cairo_xlib_surface_create(display, window, visual, kWindowWidth, kWindowHeight);
        cr = cairo_create(surface);
        return cr != nullptr;
    }

    int run() {
        while (running) {
            while (XPending(display)) {
                XEvent event;
                XNextEvent(display, &event);
                if (event.type == Expose) {
                    draw();
                } else if (event.type == ConfigureNotify) {
                    width = event.xconfigure.width;
                    height = event.xconfigure.height;
                    cairo_xlib_surface_set_size(surface, width, height);
                    draw();
                } else if (event.type == ButtonPress) {
                    handle_button_press(event.xbutton.x, event.xbutton.y);
                    draw();
                } else if (event.type == KeyPress) {
                    KeySym key = XLookupKeysym(&event.xkey, 0);
                    if (key == XK_Escape || key == XK_q) {
                        running = false;
                    }
                }
            }
            const std::uint64_t now = monotonic_time_ms();
            if (now >= last_draw_ms + 100) {
                draw();
            }
            usleep(16000);
        }
        return 0;
    }

    void draw() {
        last_draw_ms = monotonic_time_ms();
        const std::vector<EndpointRecord> endpoints = registry.endpoints_snapshot();
        const TransportSnapshot transport = registry.transport_snapshot();
        const std::vector<std::string> recent_events = server.recent_events_snapshot();
        const AuthoritySelection authority_selection = authority.current();

        if (!endpoints.empty() && selected_endpoint_index >= endpoints.size()) {
            selected_endpoint_index = endpoints.size() - 1;
        } else if (endpoints.empty()) {
            selected_endpoint_index = 0;
        }

        cairo_set_source_rgb(cr, 0.08, 0.09, 0.11);
        cairo_paint(cr);

        draw_text(cr, 22, 30, "Outsider", 20.0, 0.96, 0.96, 0.98, true);
        draw_text(cr, 140, 30, "Live control build: localhost JSON-line server with transport-driven command dispatch", 12.0, 0.73, 0.78, 0.84, false);

        draw_panel(cr, 18, 48, width - 36, 68, "Header");
        draw_panel(cr, 18, 128, 360, 150, "Transport");
        draw_panel(cr, 390, 128, 692, 270, "Endpoints");
        draw_panel(cr, 18, 290, 360, 210, "Event Log");
        draw_panel(cr, 18, 512, width - 36, 186, "Semaphore");

        char line[256];
        std::snprintf(line, sizeof(line), "Protocol v%u", kProtocolVersion);
        draw_badge(cr, 30, 76, 92, 22, line, 0.94, 0.74, 0.30);

        std::snprintf(line, sizeof(line), "Session %u", registry.selected_session());
        draw_badge(cr, 132, 76, 96, 22, line, 0.42, 0.78, 0.94);

        std::snprintf(line, sizeof(line), "%zu endpoints", endpoints.size());
        draw_badge(cr, 238, 76, 118, 22, line, 0.44, 0.86, 0.56);

        draw_text(cr, 30, 112, server.running() ? "Server: running (localhost TCP)" : "Server: stopped",
                  12.0, 0.94, 0.94, 0.98, false);
        draw_text(cr, 250, 112, server.listen_uri().c_str(), 12.0, 0.74, 0.78, 0.84, false);

        std::snprintf(line, sizeof(line), "Authority: %s",
                      authority_selection.valid ? "selected" : "none");
        draw_text(cr, 34, 168, line, 12.0, 0.92, 0.92, 0.95, true);
        if (authority_selection.valid) {
            std::snprintf(line, sizeof(line), "Endpoint %u.%u",
                          authority_selection.session_slot,
                          authority_selection.endpoint_slot);
            draw_text(cr, 160, 168, line, 12.0, 0.70, 0.82, 0.94, false);
        }

        std::snprintf(line, sizeof(line), "Playing: %s", transport.playing ? "yes" : "no");
        draw_text(cr, 34, 194, line, 12.0, 0.90, 0.90, 0.94, false);
        std::snprintf(line, sizeof(line), "Bar %.2f  Beat %.2f", transport.bar, transport.beat);
        draw_text(cr, 34, 220, line, 12.0, 0.90, 0.90, 0.94, false);
        std::snprintf(line, sizeof(line), "Tempo %.1f BPM  %0.1f/%0.1f", transport.bpm, transport.beat, transport.beats_per_bar);
        draw_text(cr, 34, 246, line, 12.0, 0.90, 0.90, 0.94, false);
        std::snprintf(line, sizeof(line), "Sample Rate %.0f  Block %u  Counter %llu",
                      transport.sample_rate,
                      transport.block_size,
                      static_cast<unsigned long long>(transport.block_counter));
        draw_text(cr, 34, 272, line, 12.0, 0.90, 0.90, 0.94, false);

        draw_text(cr, 406, 172, "Slot", 11.0, 0.80, 0.84, 0.90, true);
        draw_text(cr, 472, 172, "Conn", 11.0, 0.80, 0.84, 0.90, true);
        draw_text(cr, 530, 172, "Auth", 11.0, 0.80, 0.84, 0.90, true);
        draw_text(cr, 590, 172, "Mode", 11.0, 0.80, 0.84, 0.90, true);
        draw_text(cr, 710, 172, "State", 11.0, 0.80, 0.84, 0.90, true);
        draw_text(cr, 805, 172, "Gain", 11.0, 0.80, 0.84, 0.90, true);
        draw_text(cr, 870, 172, "Last Cmd", 11.0, 0.80, 0.84, 0.90, true);

        double row_y = 202.0;
        for (std::size_t i = 0; i < endpoints.size(); ++i) {
            const EndpointRecord& endpoint = endpoints[i];
            const bool selected = i == selected_endpoint_index;
            cairo_set_source_rgba(cr,
                                  selected ? 0.22 : 1.0,
                                  selected ? 0.42 : 1.0,
                                  selected ? 0.56 : 1.0,
                                  selected ? 0.20 : 0.05);
            cairo_rectangle(cr, 404, row_y - 16, 660, 28);
            cairo_fill(cr);
            if (selected) {
                cairo_set_source_rgba(cr, 0.48, 0.82, 0.94, 0.55);
                cairo_rectangle(cr, 404, row_y - 16, 660, 28);
                cairo_stroke(cr);
            }

            std::snprintf(line, sizeof(line), "%u.%u", endpoint.session_slot, endpoint.endpoint_slot);
            draw_text(cr, 406, row_y, line, 12.0, 0.95, 0.95, 0.98, false);
            draw_text(cr, 472, row_y, endpoint.connected ? "online" : "offline", 12.0,
                      endpoint.connected ? 0.45 : 0.76,
                      endpoint.connected ? 0.90 : 0.42,
                      endpoint.connected ? 0.52 : 0.42, false);
            draw_text(cr, 530, row_y,
                      endpoint.authority_active ? "active" :
                      endpoint.authority_claimed ? "claim" : "-",
                      12.0, 0.82, 0.82, 0.88, false);
            draw_text(cr, 590, row_y, mode_name(endpoint.mode), 12.0, 0.92, 0.92, 0.96, false);
            draw_text(cr, 710, row_y, runtime_state_name(endpoint.current_state), 12.0, 0.92, 0.92, 0.96, false);
            std::snprintf(line, sizeof(line), "%.2f", endpoint.current_gain);
            draw_text(cr, 805, row_y, line, 12.0, 0.92, 0.92, 0.96, false);
            std::snprintf(line, sizeof(line), "%llu",
                          static_cast<unsigned long long>(endpoint.last_command_id));
            draw_text(cr, 870, row_y, line, 12.0, 0.92, 0.92, 0.96, false);
            row_y += 34.0;
        }

        double log_y = 334.0;
        if (recent_events.empty()) {
            draw_text(cr, 34, log_y, "No events yet.", 12.0, 0.90, 0.90, 0.94, false);
        } else {
            for (const std::string& event : recent_events) {
                draw_text(cr, 34, log_y, event.c_str(), 12.0, 0.90, 0.90, 0.94, false);
                log_y += 24.0;
            }
        }

        if (!endpoints.empty()) {
            const EndpointRecord& selected = endpoints[selected_endpoint_index];
            CommandPacket preview = semaphore.preview_for(selected, registry, authority);
            std::snprintf(line, sizeof(line), "Selected endpoint: %u.%u", selected.session_slot, selected.endpoint_slot);
            draw_text(cr, 34, 544, line, 13.0, 0.95, 0.95, 0.98, true);

            draw_control_button(cr, 34, 552, 72, 18, "Bypass", selected.mode == OutsiderMode::Bypass);
            draw_control_button(cr, 116, 552, 72, 18, "P-Mix", selected.mode == OutsiderMode::PMix);
            draw_control_button(cr, 198, 552, 72, 18, "E-Mix", selected.mode == OutsiderMode::EMix);

            std::snprintf(line, sizeof(line), "Mode %s  ->  %s  gain %.2f  duration %.2f beats",
                          mode_name(preview.mode),
                          target_state_name(preview.target_state),
                          preview.target_gain,
                          preview.duration_beats);
            draw_text(cr, 34, 592, line, 12.0, 0.90, 0.90, 0.94, false);

            std::snprintf(line, sizeof(line), "Apply at bar %u step16 %u",
                          preview.apply_at_bar,
                          preview.apply_at_step16);
            draw_text(cr, 34, 618, line, 12.0, 0.90, 0.90, 0.94, false);

            if (selected.mode == OutsiderMode::PMix) {
                draw_control_button(cr, 306, 552, 66, 18, "Bars -", false);
                draw_control_button(cr, 382, 552, 66, 18, "Bars +", false);
                draw_control_button(cr, 468, 552, 66, 18, "Bias -", false);
                draw_control_button(cr, 544, 552, 66, 18, "Bias +", false);
                std::snprintf(line, sizeof(line),
                              "P-Mix params: granularity %d maintain %.0f fade %.0f cut %.0f bias %.0f",
                              selected.p_mix_params.granularity_bars,
                              selected.p_mix_params.maintain_weight,
                              selected.p_mix_params.fade_weight,
                              selected.p_mix_params.cut_weight,
                              selected.p_mix_params.bias_percent);
            } else if (selected.mode == OutsiderMode::EMix) {
                draw_control_button(cr, 306, 552, 72, 18, "Steps -", false);
                draw_control_button(cr, 388, 552, 72, 18, "Steps +", false);
                draw_control_button(cr, 480, 552, 76, 18, "Offset -", false);
                draw_control_button(cr, 566, 552, 76, 18, "Offset +", false);
                std::snprintf(line, sizeof(line),
                              "E-Mix params: total bars %d division %d steps %d offset %d fade %.2f",
                              selected.e_mix_params.total_bars,
                              selected.e_mix_params.division,
                              selected.e_mix_params.steps,
                              selected.e_mix_params.offset,
                              selected.e_mix_params.fade_bars);
            } else {
                std::snprintf(line, sizeof(line), "Bypass mode: server would keep the endpoint fully audible.");
            }
            draw_text(cr, 34, 646, line, 12.0, 0.82, 0.86, 0.92, false);
            draw_text(cr, 34, 674,
                      "Click endpoint rows above, then use the mode and parameter buttons here.",
                      12.0, 0.70, 0.76, 0.84, false);
        } else {
            draw_text(cr, 34, 556,
                      "No endpoints connected yet. Start the LV2 client and disable Demo Mode to exercise the live control path.",
                      12.0, 0.82, 0.86, 0.92, false);
        }

        cairo_surface_flush(surface);
        XFlush(display);
    }

    void handle_button_press(int x, int y) {
        const std::vector<EndpointRecord> endpoints = registry.endpoints_snapshot();
        if (endpoints.empty()) {
            return;
        }

        for (std::size_t i = 0; i < endpoints.size(); ++i) {
            const double row_y = 202.0 + 34.0 * static_cast<double>(i);
            if (point_in_rect(x, y, 404, row_y - 16.0, 660, 28.0)) {
                selected_endpoint_index = i;
                return;
            }
        }

        if (selected_endpoint_index >= endpoints.size()) {
            selected_endpoint_index = endpoints.size() - 1;
        }
        const EndpointRecord& selected = endpoints[selected_endpoint_index];

        if (point_in_rect(x, y, 34, 552, 72, 18)) {
            registry.set_endpoint_mode(selected.session_slot, selected.endpoint_slot, OutsiderMode::Bypass);
            return;
        }
        if (point_in_rect(x, y, 116, 552, 72, 18)) {
            registry.set_endpoint_mode(selected.session_slot, selected.endpoint_slot, OutsiderMode::PMix);
            return;
        }
        if (point_in_rect(x, y, 198, 552, 72, 18)) {
            registry.set_endpoint_mode(selected.session_slot, selected.endpoint_slot, OutsiderMode::EMix);
            return;
        }

        if (selected.mode == OutsiderMode::PMix) {
            if (point_in_rect(x, y, 306, 552, 66, 18)) {
                registry.adjust_p_mix_granularity(selected.session_slot, selected.endpoint_slot, -1);
                return;
            }
            if (point_in_rect(x, y, 382, 552, 66, 18)) {
                registry.adjust_p_mix_granularity(selected.session_slot, selected.endpoint_slot, 1);
                return;
            }
            if (point_in_rect(x, y, 468, 552, 66, 18)) {
                registry.adjust_p_mix_bias(selected.session_slot, selected.endpoint_slot, -5.0f);
                return;
            }
            if (point_in_rect(x, y, 544, 552, 66, 18)) {
                registry.adjust_p_mix_bias(selected.session_slot, selected.endpoint_slot, 5.0f);
                return;
            }
        } else if (selected.mode == OutsiderMode::EMix) {
            if (point_in_rect(x, y, 306, 552, 72, 18)) {
                registry.adjust_e_mix_steps(selected.session_slot, selected.endpoint_slot, -1);
                return;
            }
            if (point_in_rect(x, y, 388, 552, 72, 18)) {
                registry.adjust_e_mix_steps(selected.session_slot, selected.endpoint_slot, 1);
                return;
            }
            if (point_in_rect(x, y, 480, 552, 76, 18)) {
                registry.adjust_e_mix_offset(selected.session_slot, selected.endpoint_slot, -1);
                return;
            }
            if (point_in_rect(x, y, 566, 552, 76, 18)) {
                registry.adjust_e_mix_offset(selected.session_slot, selected.endpoint_slot, 1);
                return;
            }
        }
    }

    SessionRegistry& registry;
    const TransportAuthority& authority;
    const WsServerStub& server;
    const SemaphoreEngine& semaphore;
    Display* display = nullptr;
    int screen = 0;
    Window window = 0;
    cairo_surface_t* surface = nullptr;
    cairo_t* cr = nullptr;
    int width = kWindowWidth;
    int height = kWindowHeight;
    bool running = true;
    std::size_t selected_endpoint_index = 0;
    std::uint64_t last_draw_ms = 0;
};

OutsiderUiX11::OutsiderUiX11(SessionRegistry& registry,
                             const TransportAuthority& authority,
                             const WsServerStub& server,
                             const SemaphoreEngine& semaphore)
    : impl_(new Impl(registry, authority, server, semaphore)) {}

OutsiderUiX11::~OutsiderUiX11() {
    delete impl_;
}

bool OutsiderUiX11::open() {
    return impl_->open();
}

int OutsiderUiX11::run() {
    return impl_->run();
}

}  // namespace outsider
