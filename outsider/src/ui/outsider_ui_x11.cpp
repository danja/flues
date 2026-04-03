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
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

namespace outsider {

namespace {

constexpr int kWindowWidth = 1100;
constexpr int kWindowHeight = 800;
constexpr double kSemaphorePanelY = 512.0;
constexpr double kSemaphorePanelHeight = 276.0;
constexpr double kSemaphoreSelectedY = 546.0;
constexpr double kSemaphoreStatusY = 566.0;
constexpr double kSemaphoreEditRowY = 578.0;
constexpr double kSemaphoreEditLabelY = 592.0;
constexpr double kSemaphoreModeRowY = 608.0;
constexpr double kSemaphoreAssignRowY = 636.0;
constexpr double kSemaphorePreview1Y = 668.0;
constexpr double kSemaphorePreview2Y = 690.0;
constexpr double kSemaphoreParamRowY = 708.0;
constexpr double kSemaphoreParamTextY = 736.0;
constexpr double kSemaphoreGroupSummaryY = 764.0;

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

GroupRecord default_group_record(std::uint16_t session_slot, std::uint8_t group_slot) {
    GroupRecord group{};
    group.session_slot = session_slot;
    group.group_slot = group_slot;
    group.mode = OutsiderMode::Bypass;
    group.p_mix_params.granularity_bars = 2;
    group.p_mix_params.maintain_weight = 25.0f;
    group.p_mix_params.fade_weight = 45.0f;
    group.p_mix_params.cut_weight = 30.0f;
    group.p_mix_params.fade_dur_max_fraction = 0.5f;
    group.p_mix_params.bias_percent = 58.0f;
    group.e_mix_params.total_bars = 8;
    group.e_mix_params.division = 8;
    group.e_mix_params.steps = 5;
    group.e_mix_params.offset = 0;
    group.e_mix_params.fade_bars = 0.25f;
    return group;
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
        if (back_cr) {
            cairo_destroy(back_cr);
        }
        if (back_buffer) {
            cairo_surface_destroy(back_buffer);
        }
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
        back_buffer = cairo_image_surface_create(CAIRO_FORMAT_RGB24, kWindowWidth, kWindowHeight);
        back_cr = cairo_create(back_buffer);
        return cr != nullptr && back_cr != nullptr;
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
                    if (back_cr) {
                        cairo_destroy(back_cr);
                        back_cr = nullptr;
                    }
                    if (back_buffer) {
                        cairo_surface_destroy(back_buffer);
                        back_buffer = nullptr;
                    }
                    back_buffer = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
                    back_cr = cairo_create(back_buffer);
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
        if (!cr || !back_cr || !surface || !back_buffer) {
            return;
        }

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

        const std::uint16_t active_session =
            endpoints.empty() ? registry.selected_session() : endpoints[selected_endpoint_index].session_slot;

        cairo_t* const draw_cr = back_cr;

        cairo_set_source_rgb(draw_cr, 0.08, 0.09, 0.11);
        cairo_paint(draw_cr);

        draw_text(draw_cr, 22, 30, "Outsider", 20.0, 0.96, 0.96, 0.98, true);
        draw_text(draw_cr, 140, 30, "Live control build: localhost WebSocket server with transport-driven command dispatch", 12.0, 0.73, 0.78, 0.84, false);

        draw_panel(draw_cr, 18, 48, width - 36, 68, "Header");
        draw_panel(draw_cr, 18, 128, 360, 150, "Transport");
        draw_panel(draw_cr, 390, 128, 692, 270, "Endpoints");
        draw_panel(draw_cr, 18, 290, 360, 210, "Event Log");
        draw_panel(draw_cr, 18, kSemaphorePanelY, width - 36, kSemaphorePanelHeight, "Semaphore");

        char line[256];
        std::snprintf(line, sizeof(line), "Protocol v%u", kProtocolVersion);
        draw_badge(draw_cr, 30, 76, 92, 22, line, 0.94, 0.74, 0.30);

        std::snprintf(line, sizeof(line), "Session %u", registry.selected_session());
        draw_badge(draw_cr, 132, 76, 96, 22, line, 0.42, 0.78, 0.94);

        std::snprintf(line, sizeof(line), "%zu endpoints", endpoints.size());
        draw_badge(draw_cr, 238, 76, 118, 22, line, 0.44, 0.86, 0.56);

        draw_text(draw_cr, 30, 112, server.running() ? "Server: running (localhost WebSocket)" : "Server: stopped",
                  12.0, 0.94, 0.94, 0.98, false);
        draw_text(draw_cr, 250, 112, server.listen_uri().c_str(), 12.0, 0.74, 0.78, 0.84, false);

        std::snprintf(line, sizeof(line), "Authority: %s",
                      authority_selection.valid ? "selected" : "none");
        draw_text(draw_cr, 34, 168, line, 12.0, 0.92, 0.92, 0.95, true);
        if (authority_selection.valid) {
            std::snprintf(line, sizeof(line), "Endpoint %u.%u",
                          authority_selection.session_slot,
                          authority_selection.endpoint_slot);
            draw_text(draw_cr, 160, 168, line, 12.0, 0.70, 0.82, 0.94, false);
        }

        std::snprintf(line, sizeof(line), "Playing: %s", transport.playing ? "yes" : "no");
        draw_text(draw_cr, 34, 194, line, 12.0, 0.90, 0.90, 0.94, false);
        std::snprintf(line, sizeof(line), "Bar %.2f  Beat %.2f", transport.bar, transport.beat);
        draw_text(draw_cr, 34, 220, line, 12.0, 0.90, 0.90, 0.94, false);
        std::snprintf(line, sizeof(line), "Tempo %.1f BPM  %0.1f/%0.1f", transport.bpm, transport.beat, transport.beats_per_bar);
        draw_text(draw_cr, 34, 246, line, 12.0, 0.90, 0.90, 0.94, false);
        std::snprintf(line, sizeof(line), "Sample Rate %.0f  Block %u  Counter %llu",
                      transport.sample_rate,
                      transport.block_size,
                      static_cast<unsigned long long>(transport.block_counter));
        draw_text(draw_cr, 34, 272, line, 12.0, 0.90, 0.90, 0.94, false);

        draw_text(draw_cr, 406, 172, "Slot", 11.0, 0.80, 0.84, 0.90, true);
        draw_text(draw_cr, 472, 172, "Conn", 11.0, 0.80, 0.84, 0.90, true);
        draw_text(draw_cr, 528, 172, "Auth", 11.0, 0.80, 0.84, 0.90, true);
        draw_text(draw_cr, 584, 172, "Ctl", 11.0, 0.80, 0.84, 0.90, true);
        draw_text(draw_cr, 652, 172, "Mode", 11.0, 0.80, 0.84, 0.90, true);
        draw_text(draw_cr, 776, 172, "State", 11.0, 0.80, 0.84, 0.90, true);
        draw_text(draw_cr, 902, 172, "Gain", 11.0, 0.80, 0.84, 0.90, true);

        double row_y = 202.0;
        for (std::size_t i = 0; i < endpoints.size(); ++i) {
            const EndpointRecord& endpoint = endpoints[i];
            EffectiveSemaphoreState effective{};
            registry.effective_semaphore_state(endpoint.session_slot,
                                               endpoint.endpoint_slot,
                                               &effective);
            const bool selected = i == selected_endpoint_index;
            cairo_set_source_rgba(draw_cr,
                                  selected ? 0.22 : 1.0,
                                  selected ? 0.42 : 1.0,
                                  selected ? 0.56 : 1.0,
                                  selected ? 0.20 : 0.05);
            cairo_rectangle(draw_cr, 404, row_y - 16, 660, 28);
            cairo_fill(draw_cr);
            if (selected) {
                cairo_set_source_rgba(draw_cr, 0.48, 0.82, 0.94, 0.55);
                cairo_rectangle(draw_cr, 404, row_y - 16, 660, 28);
                cairo_stroke(draw_cr);
            }

            std::snprintf(line, sizeof(line), "%u.%u", endpoint.session_slot, endpoint.endpoint_slot);
            draw_text(draw_cr, 406, row_y, line, 12.0, 0.95, 0.95, 0.98, false);
            draw_text(draw_cr, 472, row_y, endpoint.connected ? "online" : "offline", 12.0,
                      endpoint.connected ? 0.45 : 0.76,
                      endpoint.connected ? 0.90 : 0.42,
                      endpoint.connected ? 0.52 : 0.42, false);
            draw_text(draw_cr, 528, row_y,
                      endpoint.authority_active ? "active" :
                      endpoint.authority_claimed ? "claim" : "-",
                      12.0, 0.82, 0.82, 0.88, false);
            if (endpoint.follows_group && endpoint.group_slot > 0) {
                std::snprintf(line, sizeof(line), "G%u", endpoint.group_slot);
            } else if (endpoint.group_slot > 0) {
                std::snprintf(line, sizeof(line), "Own:G%u", endpoint.group_slot);
            } else {
                std::snprintf(line, sizeof(line), "Own");
            }
            draw_text(draw_cr, 584, row_y, line, 11.0, 0.90, 0.90, 0.94, false);
            draw_text(draw_cr, 652, row_y, mode_name(effective.mode), 12.0, 0.92, 0.92, 0.96, false);
            draw_text(draw_cr, 776, row_y, runtime_state_name(endpoint.current_state), 12.0, 0.92, 0.92, 0.96, false);
            std::snprintf(line, sizeof(line), "%.2f", endpoint.current_gain);
            draw_text(draw_cr, 902, row_y, line, 12.0, 0.92, 0.92, 0.96, false);
            row_y += 34.0;
        }

        constexpr double log_x = 34.0;
        constexpr double log_y_start = 334.0;
        constexpr double log_line_height = 24.0;
        constexpr double log_clip_x = 28.0;
        constexpr double log_clip_y = 320.0;
        constexpr double log_clip_w = 340.0;
        constexpr double log_clip_h = 160.0;
        const std::size_t max_log_lines =
            static_cast<std::size_t>(std::max(1.0, std::floor(log_clip_h / log_line_height)));

        cairo_save(draw_cr);
        cairo_rectangle(draw_cr, log_clip_x, log_clip_y, log_clip_w, log_clip_h);
        cairo_clip(draw_cr);

        double log_y = log_y_start;
        if (recent_events.empty()) {
            draw_text(draw_cr, log_x, log_y, "No events yet.", 12.0, 0.90, 0.90, 0.94, false);
        } else {
            const std::size_t first_visible =
                recent_events.size() > max_log_lines ? recent_events.size() - max_log_lines : 0u;
            for (std::size_t i = first_visible; i < recent_events.size(); ++i) {
                draw_text(draw_cr, log_x, log_y, recent_events[i].c_str(), 12.0, 0.90, 0.90, 0.94, false);
                log_y += log_line_height;
            }
        }
        cairo_restore(draw_cr);

        if (!endpoints.empty()) {
            const EndpointRecord& selected = endpoints[selected_endpoint_index];
            EffectiveSemaphoreState effective{};
            registry.effective_semaphore_state(selected.session_slot,
                                               selected.endpoint_slot,
                                               &effective);
            if (selected_edit_group_slot > 4) {
                selected_edit_group_slot = 0;
            }

            GroupRecord edit_group = default_group_record(selected.session_slot,
                                                          selected_edit_group_slot == 0 ? 1 : selected_edit_group_slot);
            if (selected_edit_group_slot > 0) {
                registry.group_snapshot(selected.session_slot, selected_edit_group_slot, &edit_group);
            }

            const bool editing_group = selected_edit_group_slot > 0;
            const OutsiderMode editor_mode = editing_group ? edit_group.mode : selected.mode;
            CommandPacket preview = semaphore.preview_for(selected, registry, authority);
            std::snprintf(line, sizeof(line), "Selected endpoint: %u.%u", selected.session_slot, selected.endpoint_slot);
            draw_text(draw_cr, 34, kSemaphoreSelectedY, line, 13.0, 0.95, 0.95, 0.98, true);
            if (selected.follows_group && selected.group_slot > 0) {
                std::snprintf(line, sizeof(line), "Endpoint control: following group G%u", selected.group_slot);
            } else if (selected.group_slot > 0) {
                std::snprintf(line, sizeof(line), "Endpoint control: own settings, assigned to G%u", selected.group_slot);
            } else {
                std::snprintf(line, sizeof(line), "Endpoint control: own settings, no group");
            }
            draw_text(draw_cr, 320, kSemaphoreStatusY, line, 12.0, 0.82, 0.86, 0.92, false);

            draw_control_button(draw_cr, 34, kSemaphoreEditRowY, 56, 18, "Ep", !editing_group);
            draw_control_button(draw_cr, 100, kSemaphoreEditRowY, 56, 18, "G1", selected_edit_group_slot == 1);
            draw_control_button(draw_cr, 166, kSemaphoreEditRowY, 56, 18, "G2", selected_edit_group_slot == 2);
            draw_control_button(draw_cr, 232, kSemaphoreEditRowY, 56, 18, "G3", selected_edit_group_slot == 3);
            draw_control_button(draw_cr, 298, kSemaphoreEditRowY, 56, 18, "G4", selected_edit_group_slot == 4);

            if (editing_group) {
                std::snprintf(line, sizeof(line), "Editing: Group G%u", selected_edit_group_slot);
            } else {
                std::snprintf(line, sizeof(line), "Editing: Endpoint");
            }
            draw_text(draw_cr, 370, kSemaphoreEditLabelY, line, 12.0, 0.92, 0.92, 0.95, false);

            draw_control_button(draw_cr, 34, kSemaphoreModeRowY, 72, 18, "Bypass", editor_mode == OutsiderMode::Bypass);
            draw_control_button(draw_cr, 116, kSemaphoreModeRowY, 72, 18, "P-Mix", editor_mode == OutsiderMode::PMix);
            draw_control_button(draw_cr, 198, kSemaphoreModeRowY, 72, 18, "E-Mix", editor_mode == OutsiderMode::EMix);

            draw_control_button(draw_cr, 34, kSemaphoreAssignRowY, 60, 18, "None", selected.group_slot == 0);
            draw_control_button(draw_cr, 104, kSemaphoreAssignRowY, 48, 18, "G1", selected.group_slot == 1);
            draw_control_button(draw_cr, 162, kSemaphoreAssignRowY, 48, 18, "G2", selected.group_slot == 2);
            draw_control_button(draw_cr, 220, kSemaphoreAssignRowY, 48, 18, "G3", selected.group_slot == 3);
            draw_control_button(draw_cr, 278, kSemaphoreAssignRowY, 48, 18, "G4", selected.group_slot == 4);

            draw_control_button(draw_cr, 350, kSemaphoreAssignRowY, 62, 18, "Own", !selected.follows_group);
            draw_control_button(draw_cr, 422, kSemaphoreAssignRowY, 78, 18, "Follow", selected.follows_group);

            std::snprintf(line, sizeof(line), "Effective mode %s  ->  %s  gain %.2f  duration %.2f beats",
                          mode_name(preview.mode),
                          target_state_name(preview.target_state),
                          preview.target_gain,
                          preview.duration_beats);
            draw_text(draw_cr, 34, kSemaphorePreview1Y, line, 12.0, 0.90, 0.90, 0.94, false);

            std::snprintf(line, sizeof(line), "Apply at bar %u step16 %u",
                          preview.apply_at_bar,
                          preview.apply_at_step16);
            draw_text(draw_cr, 34, kSemaphorePreview2Y, line, 12.0, 0.90, 0.90, 0.94, false);

            if (editor_mode == OutsiderMode::PMix) {
                draw_control_button(draw_cr, 34, kSemaphoreParamRowY, 66, 18, "Bars -", false);
                draw_control_button(draw_cr, 110, kSemaphoreParamRowY, 66, 18, "Bars +", false);
                draw_control_button(draw_cr, 196, kSemaphoreParamRowY, 66, 18, "Bias -", false);
                draw_control_button(draw_cr, 272, kSemaphoreParamRowY, 66, 18, "Bias +", false);
                const PMixParams& params = editing_group ? edit_group.p_mix_params : selected.p_mix_params;
                std::snprintf(line, sizeof(line),
                              "P-Mix params: granularity %d maintain %.0f fade %.0f cut %.0f bias %.0f",
                              params.granularity_bars,
                              params.maintain_weight,
                              params.fade_weight,
                              params.cut_weight,
                              params.bias_percent);
            } else if (editor_mode == OutsiderMode::EMix) {
                draw_control_button(draw_cr, 34, kSemaphoreParamRowY, 72, 18, "Steps -", false);
                draw_control_button(draw_cr, 116, kSemaphoreParamRowY, 72, 18, "Steps +", false);
                draw_control_button(draw_cr, 208, kSemaphoreParamRowY, 76, 18, "Offset -", false);
                draw_control_button(draw_cr, 294, kSemaphoreParamRowY, 76, 18, "Offset +", false);
                const EMixParams& params = editing_group ? edit_group.e_mix_params : selected.e_mix_params;
                std::snprintf(line, sizeof(line),
                              "E-Mix params: total bars %d division %d steps %d offset %d fade %.2f",
                              params.total_bars,
                              params.division,
                              params.steps,
                              params.offset,
                              params.fade_bars);
            } else {
                std::snprintf(line, sizeof(line), "Bypass mode: server keeps the endpoint fully audible.");
            }
            draw_text(draw_cr, 360, kSemaphoreParamTextY, line, 12.0, 0.82, 0.86, 0.92, false);

            char group_summary[512];
            std::snprintf(group_summary, sizeof(group_summary), "Groups:");
            for (std::uint8_t slot = 1; slot <= 4; ++slot) {
                GroupRecord group = default_group_record(active_session, slot);
                registry.group_snapshot(active_session, slot, &group);
                int members = 0;
                int followers = 0;
                for (const EndpointRecord& endpoint : endpoints) {
                    if (endpoint.session_slot == active_session && endpoint.group_slot == slot) {
                        members++;
                        if (endpoint.follows_group) {
                            followers++;
                        }
                    }
                }
                char chunk[96];
                std::snprintf(chunk, sizeof(chunk), " G%u %d/%d %s",
                              slot,
                              followers,
                              members,
                              mode_name(group.mode));
                std::strncat(group_summary, chunk, sizeof(group_summary) - std::strlen(group_summary) - 1u);
            }
            draw_text(draw_cr, 34, kSemaphoreGroupSummaryY, group_summary, 12.0, 0.70, 0.76, 0.84, false);
        } else {
            draw_text(draw_cr, 34, 560,
                      "No endpoints connected yet. Start the LV2 client and disable Demo Mode to exercise the live control path.",
                      12.0, 0.82, 0.86, 0.92, false);
        }

        cairo_set_source_surface(cr, back_buffer, 0, 0);
        cairo_paint(cr);
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
                if (endpoints[i].follows_group && endpoints[i].group_slot > 0) {
                    selected_edit_group_slot = endpoints[i].group_slot;
                } else {
                    selected_edit_group_slot = 0;
                }
                return;
            }
        }

        if (selected_endpoint_index >= endpoints.size()) {
            selected_endpoint_index = endpoints.size() - 1;
        }
        const EndpointRecord& selected = endpoints[selected_endpoint_index];
        const bool editing_group = selected_edit_group_slot > 0;

        if (point_in_rect(x, y, 34, kSemaphoreEditRowY, 56, 18)) {
            selected_edit_group_slot = 0;
            return;
        }
        if (point_in_rect(x, y, 100, kSemaphoreEditRowY, 56, 18)) {
            selected_edit_group_slot = 1;
            return;
        }
        if (point_in_rect(x, y, 166, kSemaphoreEditRowY, 56, 18)) {
            selected_edit_group_slot = 2;
            return;
        }
        if (point_in_rect(x, y, 232, kSemaphoreEditRowY, 56, 18)) {
            selected_edit_group_slot = 3;
            return;
        }
        if (point_in_rect(x, y, 298, kSemaphoreEditRowY, 56, 18)) {
            selected_edit_group_slot = 4;
            return;
        }

        if (point_in_rect(x, y, 34, kSemaphoreModeRowY, 72, 18)) {
            if (editing_group) {
                registry.set_group_mode(selected.session_slot, selected_edit_group_slot, OutsiderMode::Bypass);
            } else {
                registry.set_endpoint_mode(selected.session_slot, selected.endpoint_slot, OutsiderMode::Bypass);
            }
            return;
        }
        if (point_in_rect(x, y, 116, kSemaphoreModeRowY, 72, 18)) {
            if (editing_group) {
                registry.set_group_mode(selected.session_slot, selected_edit_group_slot, OutsiderMode::PMix);
            } else {
                registry.set_endpoint_mode(selected.session_slot, selected.endpoint_slot, OutsiderMode::PMix);
            }
            return;
        }
        if (point_in_rect(x, y, 198, kSemaphoreModeRowY, 72, 18)) {
            if (editing_group) {
                registry.set_group_mode(selected.session_slot, selected_edit_group_slot, OutsiderMode::EMix);
            } else {
                registry.set_endpoint_mode(selected.session_slot, selected.endpoint_slot, OutsiderMode::EMix);
            }
            return;
        }

        if (point_in_rect(x, y, 34, kSemaphoreAssignRowY, 60, 18)) {
            registry.assign_endpoint_group(selected.session_slot, selected.endpoint_slot, 0);
            return;
        }
        if (point_in_rect(x, y, 104, kSemaphoreAssignRowY, 48, 18)) {
            registry.assign_endpoint_group(selected.session_slot, selected.endpoint_slot, 1);
            return;
        }
        if (point_in_rect(x, y, 162, kSemaphoreAssignRowY, 48, 18)) {
            registry.assign_endpoint_group(selected.session_slot, selected.endpoint_slot, 2);
            return;
        }
        if (point_in_rect(x, y, 220, kSemaphoreAssignRowY, 48, 18)) {
            registry.assign_endpoint_group(selected.session_slot, selected.endpoint_slot, 3);
            return;
        }
        if (point_in_rect(x, y, 278, kSemaphoreAssignRowY, 48, 18)) {
            registry.assign_endpoint_group(selected.session_slot, selected.endpoint_slot, 4);
            return;
        }
        if (point_in_rect(x, y, 350, kSemaphoreAssignRowY, 62, 18)) {
            registry.set_endpoint_follows_group(selected.session_slot, selected.endpoint_slot, false);
            return;
        }
        if (point_in_rect(x, y, 422, kSemaphoreAssignRowY, 78, 18)) {
            registry.set_endpoint_follows_group(selected.session_slot, selected.endpoint_slot, true);
            return;
        }

        EffectiveSemaphoreState editor_state{};
        if (editing_group) {
            GroupRecord group{};
            if (registry.group_snapshot(selected.session_slot, selected_edit_group_slot, &group)) {
                editor_state.mode = group.mode;
                editor_state.p_mix_params = group.p_mix_params;
                editor_state.e_mix_params = group.e_mix_params;
            } else {
                GroupRecord fallback = default_group_record(selected.session_slot, selected_edit_group_slot);
                editor_state.mode = fallback.mode;
                editor_state.p_mix_params = fallback.p_mix_params;
                editor_state.e_mix_params = fallback.e_mix_params;
            }
        } else {
            editor_state.mode = selected.mode;
            editor_state.p_mix_params = selected.p_mix_params;
            editor_state.e_mix_params = selected.e_mix_params;
        }

        if (editor_state.mode == OutsiderMode::PMix) {
            if (point_in_rect(x, y, 34, kSemaphoreParamRowY, 66, 18)) {
                if (editing_group) {
                    registry.adjust_group_p_mix_granularity(selected.session_slot, selected_edit_group_slot, -1);
                } else {
                    registry.adjust_p_mix_granularity(selected.session_slot, selected.endpoint_slot, -1);
                }
                return;
            }
            if (point_in_rect(x, y, 110, kSemaphoreParamRowY, 66, 18)) {
                if (editing_group) {
                    registry.adjust_group_p_mix_granularity(selected.session_slot, selected_edit_group_slot, 1);
                } else {
                    registry.adjust_p_mix_granularity(selected.session_slot, selected.endpoint_slot, 1);
                }
                return;
            }
            if (point_in_rect(x, y, 196, kSemaphoreParamRowY, 66, 18)) {
                if (editing_group) {
                    registry.adjust_group_p_mix_bias(selected.session_slot, selected_edit_group_slot, -5.0f);
                } else {
                    registry.adjust_p_mix_bias(selected.session_slot, selected.endpoint_slot, -5.0f);
                }
                return;
            }
            if (point_in_rect(x, y, 272, kSemaphoreParamRowY, 66, 18)) {
                if (editing_group) {
                    registry.adjust_group_p_mix_bias(selected.session_slot, selected_edit_group_slot, 5.0f);
                } else {
                    registry.adjust_p_mix_bias(selected.session_slot, selected.endpoint_slot, 5.0f);
                }
                return;
            }
        } else if (editor_state.mode == OutsiderMode::EMix) {
            if (point_in_rect(x, y, 34, kSemaphoreParamRowY, 72, 18)) {
                if (editing_group) {
                    registry.adjust_group_e_mix_steps(selected.session_slot, selected_edit_group_slot, -1);
                } else {
                    registry.adjust_e_mix_steps(selected.session_slot, selected.endpoint_slot, -1);
                }
                return;
            }
            if (point_in_rect(x, y, 116, kSemaphoreParamRowY, 72, 18)) {
                if (editing_group) {
                    registry.adjust_group_e_mix_steps(selected.session_slot, selected_edit_group_slot, 1);
                } else {
                    registry.adjust_e_mix_steps(selected.session_slot, selected.endpoint_slot, 1);
                }
                return;
            }
            if (point_in_rect(x, y, 208, kSemaphoreParamRowY, 76, 18)) {
                if (editing_group) {
                    registry.adjust_group_e_mix_offset(selected.session_slot, selected_edit_group_slot, -1);
                } else {
                    registry.adjust_e_mix_offset(selected.session_slot, selected.endpoint_slot, -1);
                }
                return;
            }
            if (point_in_rect(x, y, 294, kSemaphoreParamRowY, 76, 18)) {
                if (editing_group) {
                    registry.adjust_group_e_mix_offset(selected.session_slot, selected_edit_group_slot, 1);
                } else {
                    registry.adjust_e_mix_offset(selected.session_slot, selected.endpoint_slot, 1);
                }
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
    cairo_surface_t* back_buffer = nullptr;
    cairo_t* back_cr = nullptr;
    int width = kWindowWidth;
    int height = kWindowHeight;
    bool running = true;
    std::size_t selected_endpoint_index = 0;
    std::uint8_t selected_edit_group_slot = 0;
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
