#include <lv2/ui/ui.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define OUTSIDER_CLIENT_UI_URI "https://danja.github.io/flues/plugins/outsider-client#ui"

enum {
    PORT_CONTROL = 0,
    PORT_AUDIO_IN_L = 1,
    PORT_AUDIO_IN_R = 2,
    PORT_AUDIO_OUT_L = 3,
    PORT_AUDIO_OUT_R = 4,
    PORT_ENABLE = 5,
    PORT_SESSION_SLOT = 6,
    PORT_ENDPOINT_SLOT = 7,
    PORT_AUTHORITY = 8,
    PORT_RECONNECT = 9,
    PORT_FALLBACK_GAIN = 10,
    PORT_CONNECTED = 11,
    PORT_SERVER_SEEN = 12,
    PORT_CURRENT_GAIN = 13,
    PORT_CURRENT_STATE = 14,
    PORT_CURRENT_MODE = 15,
    PORT_AUTHORITY_ACTIVE = 16
};

typedef struct {
    LV2UI_Write_Function write;
    LV2UI_Controller controller;

    Display* display;
    int screen;
    Window window;
    Window parent;
    cairo_surface_t* surface;
    cairo_t* cr;
    cairo_surface_t* back_buffer;
    cairo_t* back_cr;

    pthread_t thread;
    pthread_mutex_t mutex;
    volatile int running;
    volatile int needs_redraw;

    float enable;
    float session_slot;
    float endpoint_slot;
    float authority;
    float reconnect;
    float fallback_gain;
    float connected;
    float server_seen;
    float current_gain;
    float current_state;
    float current_mode;
    float authority_active;

    int width;
    int height;
} OutsiderClientUI;

static pthread_mutex_t g_xlib_init_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_xlib_threads_ready = false;

static void ensure_xlib_threads(void) {
    pthread_mutex_lock(&g_xlib_init_lock);
    if (!g_xlib_threads_ready) {
        XInitThreads();
        g_xlib_threads_ready = true;
    }
    pthread_mutex_unlock(&g_xlib_init_lock);
}

static const char* mode_name(float value) {
    const int mode = (int)(value + 0.5f);
    switch (mode) {
        case 1: return "P-Mix";
        case 2: return "E-Mix";
        case 0:
        default: return "Bypass";
    }
}

static const char* state_name(float value) {
    const int state = (int)(value + 0.5f);
    switch (state) {
        case 1: return "Play";
        case 2: return "Mute";
        case 3: return "Fade In";
        case 4: return "Fade Out";
        case 0:
        default: return "Bypass";
    }
}

static void draw_badge(cairo_t* cr,
                       double x,
                       double y,
                       double w,
                       double h,
                       const char* label,
                       double r,
                       double g,
                       double b) {
    cairo_set_source_rgb(cr, r, g, b);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);

    cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.08, 0.08, 0.10);
    cairo_move_to(cr, x + 10, y + h - 7);
    cairo_show_text(cr, label);
}

static void draw_ui(OutsiderClientUI* ui) {
    cairo_t* cr = ui->back_cr;
    cairo_set_source_rgb(cr, 0.08, 0.09, 0.11);
    cairo_paint(cr);

    cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 18.0);
    cairo_set_source_rgb(cr, 0.96, 0.96, 0.98);
    cairo_move_to(cr, 18, 28);
    cairo_show_text(cr, "Outsider Client");

    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.72, 0.76, 0.84);
    cairo_move_to(cr, 178, 28);
    cairo_show_text(cr, "Scaffold build: pass-through LV2 endpoint with queue/net stubs");

    cairo_set_source_rgb(cr, 0.13, 0.14, 0.17);
    cairo_rectangle(cr, 16, 48, ui->width - 32, 92);
    cairo_fill(cr);
    cairo_rectangle(cr, 16, 152, ui->width - 32, ui->height - 168);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.08);
    cairo_rectangle(cr, 16, 48, ui->width - 32, 92);
    cairo_stroke(cr);
    cairo_rectangle(cr, 16, 152, ui->width - 32, ui->height - 168);
    cairo_stroke(cr);

    char line[256];
    snprintf(line, sizeof(line), "Session %d", (int)(ui->session_slot + 0.5f));
    draw_badge(cr, 28, 66, 90, 22, line, 0.42, 0.78, 0.94);
    snprintf(line, sizeof(line), "Endpoint %d", (int)(ui->endpoint_slot + 0.5f));
    draw_badge(cr, 128, 66, 108, 22, line, 0.46, 0.86, 0.58);
    draw_badge(cr, 246, 66, 90, 22, ui->enable >= 0.5f ? "Enabled" : "Disabled", 0.94, 0.74, 0.32);

    cairo_set_source_rgb(cr, 0.90, 0.90, 0.94);
    cairo_set_font_size(cr, 12.0);
    snprintf(line, sizeof(line), "Authority request: %s", ui->authority >= 0.5f ? "on" : "off");
    cairo_move_to(cr, 30, 114);
    cairo_show_text(cr, line);
    snprintf(line, sizeof(line), "Reconnect: %s    Fallback Gain: %.2f", ui->reconnect >= 0.5f ? "on" : "off", ui->fallback_gain);
    cairo_move_to(cr, 250, 114);
    cairo_show_text(cr, line);

    cairo_set_source_rgb(cr, 0.96, 0.96, 0.98);
    cairo_set_font_size(cr, 13.0);
    cairo_move_to(cr, 30, 180);
    cairo_show_text(cr, "Runtime Status");

    snprintf(line, sizeof(line), "Connected: %s", ui->connected >= 0.5f ? "yes" : "no");
    cairo_move_to(cr, 30, 212);
    cairo_show_text(cr, line);
    snprintf(line, sizeof(line), "Server Seen: %s", ui->server_seen >= 0.5f ? "yes" : "no");
    cairo_move_to(cr, 30, 238);
    cairo_show_text(cr, line);
    snprintf(line, sizeof(line), "Authority Active: %s", ui->authority_active >= 0.5f ? "yes" : "no");
    cairo_move_to(cr, 30, 264);
    cairo_show_text(cr, line);

    snprintf(line, sizeof(line), "Mode: %s", mode_name(ui->current_mode));
    cairo_move_to(cr, 280, 212);
    cairo_show_text(cr, line);
    snprintf(line, sizeof(line), "State: %s", state_name(ui->current_state));
    cairo_move_to(cr, 280, 238);
    cairo_show_text(cr, line);
    snprintf(line, sizeof(line), "Current Gain: %.2f", ui->current_gain);
    cairo_move_to(cr, 280, 264);
    cairo_show_text(cr, line);

    cairo_set_source_rgb(cr, 0.78, 0.82, 0.88);
    cairo_move_to(cr, 30, 310);
    cairo_show_text(cr, "Notes");
    cairo_set_source_rgb(cr, 0.90, 0.90, 0.94);
    cairo_move_to(cr, 30, 338);
    cairo_show_text(cr, "- Audio is local pass-through in this scaffold.");
    cairo_move_to(cr, 30, 362);
    cairo_show_text(cr, "- No live WebSocket transport exists yet; connected/server seen stay off.");
    cairo_move_to(cr, 30, 386);
    cairo_show_text(cr, "- Session and endpoint slots are persisted through LV2 state.");
    cairo_move_to(cr, 30, 410);
    cairo_show_text(cr, "- Next step is wiring loopback commands, then the real protocol.");

    cairo_set_source_surface(ui->cr, ui->back_buffer, 0, 0);
    cairo_paint(ui->cr);
    cairo_surface_flush(ui->surface);
    XFlush(ui->display);
}

static void request_redraw(OutsiderClientUI* ui) {
    ui->needs_redraw = 1;
}

static void* event_thread_main(void* arg) {
    OutsiderClientUI* ui = (OutsiderClientUI*)arg;

    while (ui->running) {
        while (XPending(ui->display)) {
            XEvent event;
            XNextEvent(ui->display, &event);
            if (event.type == Expose) {
                request_redraw(ui);
            } else if (event.type == ConfigureNotify) {
                ui->width = event.xconfigure.width;
                ui->height = event.xconfigure.height;
                if (ui->surface) cairo_surface_destroy(ui->surface);
                if (ui->cr) cairo_destroy(ui->cr);
                if (ui->back_cr) cairo_destroy(ui->back_cr);
                if (ui->back_buffer) cairo_surface_destroy(ui->back_buffer);

                Visual* visual = DefaultVisual(ui->display, ui->screen);
                ui->surface = cairo_xlib_surface_create(ui->display, ui->window, visual, ui->width, ui->height);
                ui->cr = cairo_create(ui->surface);
                ui->back_buffer = cairo_image_surface_create(CAIRO_FORMAT_RGB24, ui->width, ui->height);
                ui->back_cr = cairo_create(ui->back_buffer);
                request_redraw(ui);
            }
        }

        if (ui->needs_redraw) {
            pthread_mutex_lock(&ui->mutex);
            draw_ui(ui);
            ui->needs_redraw = 0;
            pthread_mutex_unlock(&ui->mutex);
        }

        usleep(16000);
    }
    return NULL;
}

static LV2UI_Handle instantiate(const LV2UI_Descriptor*,
                                const char*,
                                const char*,
                                LV2UI_Write_Function write_function,
                                LV2UI_Controller controller,
                                LV2UI_Widget* widget,
                                const LV2_Feature* const* features) {
    (void)write_function;
    (void)controller;

    OutsiderClientUI* ui = (OutsiderClientUI*)calloc(1, sizeof(OutsiderClientUI));
    if (!ui) {
        return NULL;
    }

    ui->width = 640;
    ui->height = 450;
    ui->enable = 1.0f;
    ui->session_slot = 1.0f;
    ui->endpoint_slot = 1.0f;
    ui->reconnect = 1.0f;
    ui->fallback_gain = 1.0f;
    ui->current_gain = 1.0f;
    pthread_mutex_init(&ui->mutex, NULL);

    ensure_xlib_threads();
    ui->display = XOpenDisplay(NULL);
    if (!ui->display) {
        pthread_mutex_destroy(&ui->mutex);
        free(ui);
        return NULL;
    }

    ui->screen = DefaultScreen(ui->display);
    ui->parent = RootWindow(ui->display, ui->screen);

    for (int i = 0; features && features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_UI__parent)) {
            ui->parent = (Window)(uintptr_t)features[i]->data;
        }
    }

    XSetWindowAttributes attrs;
    attrs.event_mask = ExposureMask | StructureNotifyMask;
    attrs.background_pixel = BlackPixel(ui->display, ui->screen);

    ui->window = XCreateWindow(ui->display,
                               ui->parent,
                               0,
                               0,
                               ui->width,
                               ui->height,
                               0,
                               CopyFromParent,
                               InputOutput,
                               CopyFromParent,
                               CWBackPixel | CWEventMask,
                               &attrs);

    XStoreName(ui->display, ui->window, "Outsider Client");
    XMapWindow(ui->display, ui->window);

    Visual* visual = DefaultVisual(ui->display, ui->screen);
    ui->surface = cairo_xlib_surface_create(ui->display, ui->window, visual, ui->width, ui->height);
    ui->cr = cairo_create(ui->surface);
    ui->back_buffer = cairo_image_surface_create(CAIRO_FORMAT_RGB24, ui->width, ui->height);
    ui->back_cr = cairo_create(ui->back_buffer);
    ui->running = 1;
    ui->needs_redraw = 1;

    *widget = (LV2UI_Widget)(intptr_t)ui->window;
    draw_ui(ui);
    pthread_create(&ui->thread, NULL, event_thread_main, ui);
    return (LV2UI_Handle)ui;
}

static void cleanup(LV2UI_Handle handle) {
    OutsiderClientUI* ui = (OutsiderClientUI*)handle;
    if (!ui) return;

    ui->running = 0;
    if (ui->thread) {
        pthread_join(ui->thread, NULL);
    }

    if (ui->back_cr) cairo_destroy(ui->back_cr);
    if (ui->back_buffer) cairo_surface_destroy(ui->back_buffer);
    if (ui->cr) cairo_destroy(ui->cr);
    if (ui->surface) cairo_surface_destroy(ui->surface);

    if (ui->display) {
        if (ui->window) {
            XUnmapWindow(ui->display, ui->window);
            XSync(ui->display, False);
            XDestroyWindow(ui->display, ui->window);
        }
        XCloseDisplay(ui->display);
    }

    pthread_mutex_destroy(&ui->mutex);
    free(ui);
}

static void port_event(LV2UI_Handle handle,
                       uint32_t port_index,
                       uint32_t buffer_size,
                       uint32_t format,
                       const void* buffer) {
    (void)format;
    OutsiderClientUI* ui = (OutsiderClientUI*)handle;
    if (!ui || !buffer || buffer_size < sizeof(float)) {
        return;
    }

    const float value = *(const float*)buffer;
    pthread_mutex_lock(&ui->mutex);
    switch (port_index) {
        case PORT_ENABLE: ui->enable = value; break;
        case PORT_SESSION_SLOT: ui->session_slot = value; break;
        case PORT_ENDPOINT_SLOT: ui->endpoint_slot = value; break;
        case PORT_AUTHORITY: ui->authority = value; break;
        case PORT_RECONNECT: ui->reconnect = value; break;
        case PORT_FALLBACK_GAIN: ui->fallback_gain = value; break;
        case PORT_CONNECTED: ui->connected = value; break;
        case PORT_SERVER_SEEN: ui->server_seen = value; break;
        case PORT_CURRENT_GAIN: ui->current_gain = value; break;
        case PORT_CURRENT_STATE: ui->current_state = value; break;
        case PORT_CURRENT_MODE: ui->current_mode = value; break;
        case PORT_AUTHORITY_ACTIVE: ui->authority_active = value; break;
        default: break;
    }
    request_redraw(ui);
    pthread_mutex_unlock(&ui->mutex);
}

static const void* extension_data(const char* uri) {
    (void)uri;
    return NULL;
}

static const LV2UI_Descriptor descriptor = {
    OUTSIDER_CLIENT_UI_URI,
    instantiate,
    cleanup,
    port_event,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : NULL;
}

