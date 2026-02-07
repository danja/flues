#include <lv2/ui/ui.h>
#include <lv2/core/lv2.h>

#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#define METALV_URI "https://danja.github.io/flues/plugins/metalv"
#define METALV_UI_URI METALV_URI "#ui"

#define WINDOW_WIDTH 420
#define WINDOW_HEIGHT 280

#define MARGIN 16
#define ROW_HEIGHT 50
#define TOGGLE_SIZE 18
#define GAIN_WIDTH 180
#define GAIN_HEIGHT 10

static const int kSlotCount = 4;
static const int kBankSize = 8;

enum {
    PORT_IN_L = 0,
    PORT_IN_R = 1,
    PORT_OUT_L = 2,
    PORT_OUT_R = 3,
    PORT_MIDI_IN = 4,
    PORT_MIDI_OUT = 5,
    PORT_SLOT_0_BYPASS = 6
};

static int slot_port_base(int slot) {
    return PORT_SLOT_0_BYPASS + (slot * (2 + kBankSize));
}

typedef struct {
    LV2UI_Write_Function write;
    LV2UI_Controller controller;

    Display* display;
    int screen;
    Window window;
    cairo_surface_t* surface;

    pthread_t thread;
    pthread_mutex_t mutex;
    volatile bool running;

    int width;
    int height;

    float bypass[4];
    float gain[4];

    int active_gain_slot;

    volatile bool needs_redraw;
} MetaLVUI;

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

static float clamp_value(float v, float min_v, float max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

static void send_value(MetaLVUI* ui, uint32_t port, float value) {
    if (!ui->write) return;
    ui->write(ui->controller, port, sizeof(float), 0, &value);
}

static void draw_ui(MetaLVUI* ui) {
    cairo_t* cr = cairo_create(ui->surface);

    cairo_set_source_rgb(cr, 0.08, 0.09, 0.11);
    cairo_paint(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14.0);
    cairo_set_source_rgb(cr, 0.95, 0.75, 0.35);
    cairo_move_to(cr, MARGIN, MARGIN + 10);
    cairo_show_text(cr, "METALV");

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.0);

    for (int i = 0; i < kSlotCount; ++i) {
        int y = MARGIN + 24 + i * ROW_HEIGHT;

        char label[32];
        snprintf(label, sizeof(label), "Slot %d", i + 1);
        cairo_set_source_rgb(cr, 0.85, 0.85, 0.9);
        cairo_move_to(cr, MARGIN, y);
        cairo_show_text(cr, label);

        int toggle_x = MARGIN + 80;
        int toggle_y = y - 12;
        cairo_set_source_rgb(cr, 0.2, 0.2, 0.24);
        cairo_rectangle(cr, toggle_x, toggle_y, TOGGLE_SIZE, TOGGLE_SIZE);
        cairo_fill(cr);
        if (ui->bypass[i] >= 0.5f) {
            cairo_set_source_rgb(cr, 0.95, 0.35, 0.35);
        } else {
            cairo_set_source_rgb(cr, 0.35, 0.9, 0.45);
        }
        cairo_rectangle(cr, toggle_x + 3, toggle_y + 3, TOGGLE_SIZE - 6, TOGGLE_SIZE - 6);
        cairo_fill(cr);

        int gain_x = toggle_x + 40;
        int gain_y = y - 6;
        cairo_set_source_rgb(cr, 0.2, 0.2, 0.24);
        cairo_rectangle(cr, gain_x, gain_y, GAIN_WIDTH, GAIN_HEIGHT);
        cairo_fill(cr);

        float t = clamp_value(ui->gain[i] / 2.0f, 0.0f, 1.0f);
        cairo_set_source_rgb(cr, 0.95, 0.75, 0.35);
        cairo_rectangle(cr, gain_x, gain_y, (int)(GAIN_WIDTH * t), GAIN_HEIGHT);
        cairo_fill(cr);

        char gain_label[32];
        snprintf(gain_label, sizeof(gain_label), "%.2f", ui->gain[i]);
        cairo_set_source_rgb(cr, 0.75, 0.75, 0.8);
        cairo_move_to(cr, gain_x + GAIN_WIDTH + 10, y + 2);
        cairo_show_text(cr, gain_label);
    }

    cairo_destroy(cr);
    cairo_surface_flush(ui->surface);
    XFlush(ui->display);
}

static int hit_slot_toggle(int x, int y) {
    for (int i = 0; i < kSlotCount; ++i) {
        int row_y = MARGIN + 24 + i * ROW_HEIGHT;
        int toggle_x = MARGIN + 80;
        int toggle_y = row_y - 12;
        if (x >= toggle_x && x <= toggle_x + TOGGLE_SIZE &&
            y >= toggle_y && y <= toggle_y + TOGGLE_SIZE) {
            return i;
        }
    }
    return -1;
}

static int hit_slot_gain(int x, int y) {
    for (int i = 0; i < kSlotCount; ++i) {
        int row_y = MARGIN + 24 + i * ROW_HEIGHT;
        int gain_x = MARGIN + 120;
        int gain_y = row_y - 6;
        if (x >= gain_x && x <= gain_x + GAIN_WIDTH &&
            y >= gain_y && y <= gain_y + GAIN_HEIGHT) {
            return i;
        }
    }
    return -1;
}

static void set_gain_from_x(MetaLVUI* ui, int slot, int x) {
    int gain_x = MARGIN + 120;
    float t = (float)(x - gain_x) / (float)GAIN_WIDTH;
    float value = clamp_value(t * 2.0f, 0.0f, 2.0f);
    ui->gain[slot] = value;
    int port = slot_port_base(slot) + 1;
    send_value(ui, port, value);
    ui->needs_redraw = true;
}

static void handle_button_press(MetaLVUI* ui, XButtonEvent* ev) {
    pthread_mutex_lock(&ui->mutex);

    int slot = hit_slot_toggle(ev->x, ev->y);
    if (slot >= 0) {
        ui->bypass[slot] = ui->bypass[slot] >= 0.5f ? 0.0f : 1.0f;
        int port = slot_port_base(slot);
        send_value(ui, port, ui->bypass[slot]);
        ui->needs_redraw = true;
        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    slot = hit_slot_gain(ev->x, ev->y);
    if (slot >= 0) {
        ui->active_gain_slot = slot;
        set_gain_from_x(ui, slot, ev->x);
    }

    pthread_mutex_unlock(&ui->mutex);
}

static void handle_button_release(MetaLVUI* ui) {
    pthread_mutex_lock(&ui->mutex);
    ui->active_gain_slot = -1;
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_motion(MetaLVUI* ui, XMotionEvent* ev) {
    pthread_mutex_lock(&ui->mutex);
    if (ui->active_gain_slot >= 0) {
        set_gain_from_x(ui, ui->active_gain_slot, ev->x);
    }
    pthread_mutex_unlock(&ui->mutex);
}

static void* event_thread_main(void* data) {
    MetaLVUI* ui = (MetaLVUI*)data;

    while (ui->running) {
        while (XPending(ui->display)) {
            XEvent ev;
            XNextEvent(ui->display, &ev);
            switch (ev.type) {
                case Expose:
                    ui->needs_redraw = true;
                    break;
                case ConfigureNotify:
                    pthread_mutex_lock(&ui->mutex);
                    if (ev.xconfigure.width != ui->width || ev.xconfigure.height != ui->height) {
                        ui->width = ev.xconfigure.width;
                        ui->height = ev.xconfigure.height;
                        cairo_xlib_surface_set_size(ui->surface, ui->width, ui->height);
                        ui->needs_redraw = true;
                    }
                    pthread_mutex_unlock(&ui->mutex);
                    break;
                case DestroyNotify:
                    ui->running = false;
                    break;
                case ButtonPress:
                    handle_button_press(ui, &ev.xbutton);
                    break;
                case ButtonRelease:
                    handle_button_release(ui);
                    break;
                case MotionNotify:
                    handle_motion(ui, &ev.xmotion);
                    break;
                default:
                    break;
            }
        }

        if (ui->needs_redraw) {
            pthread_mutex_lock(&ui->mutex);
            draw_ui(ui);
            ui->needs_redraw = false;
            pthread_mutex_unlock(&ui->mutex);
        }

        usleep(16000);
    }

    return NULL;
}

static LV2UI_Handle ui_instantiate(const LV2UI_Descriptor* /*descriptor*/,
                                   const char* /*plugin_uri*/,
                                   const char* /*bundle_path*/,
                                   LV2UI_Write_Function write_function,
                                   LV2UI_Controller controller,
                                   LV2UI_Widget* widget,
                                   const LV2_Feature* const* features) {
    ensure_xlib_threads();

    MetaLVUI* ui = (MetaLVUI*)calloc(1, sizeof(MetaLVUI));
    if (!ui) return NULL;

    ui->write = write_function;
    ui->controller = controller;
    ui->width = WINDOW_WIDTH;
    ui->height = WINDOW_HEIGHT;
    ui->active_gain_slot = -1;

    for (int i = 0; i < kSlotCount; ++i) {
        ui->bypass[i] = 0.0f;
        ui->gain[i] = 1.0f;
    }

    void* parent = NULL;
    for (const LV2_Feature* const* f = features; f && *f; ++f) {
        if (!strcmp((*f)->URI, LV2_UI__parent)) {
            parent = (*f)->data;
        }
    }

    if (!parent) {
        fprintf(stderr, "metalv-ui: no parent window provided\n");
        free(ui);
        return NULL;
    }

    Display* display = XOpenDisplay(NULL);
    if (!display) {
        free(ui);
        return NULL;
    }

    ui->display = display;
    ui->screen = DefaultScreen(display);

    XSetWindowAttributes attrs;
    attrs.background_pixel = BlackPixel(display, ui->screen);
    attrs.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask |
                       ButtonReleaseMask | PointerMotionMask;

    ui->window = XCreateWindow(
        display,
        (Window)(uintptr_t)parent,
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

    if (!ui->window) {
        XCloseDisplay(display);
        free(ui);
        return NULL;
    }

    XStoreName(display, ui->window, "MetaLV");
    XMapWindow(display, ui->window);

    ui->surface = cairo_xlib_surface_create(display, ui->window,
                                            DefaultVisual(display, ui->screen),
                                            ui->width, ui->height);
    cairo_xlib_surface_set_size(ui->surface, ui->width, ui->height);

    pthread_mutex_init(&ui->mutex, NULL);
    ui->running = true;
    ui->needs_redraw = true;

    if (pthread_create(&ui->thread, NULL, event_thread_main, ui) != 0) {
        cairo_surface_destroy(ui->surface);
        XDestroyWindow(display, ui->window);
        XCloseDisplay(display);
        pthread_mutex_destroy(&ui->mutex);
        free(ui);
        return NULL;
    }

    *widget = (LV2UI_Widget)(uintptr_t)ui->window;
    return ui;
}

static void ui_cleanup(LV2UI_Handle handle) {
    MetaLVUI* ui = (MetaLVUI*)handle;
    if (!ui) return;

    ui->running = false;
    pthread_join(ui->thread, NULL);

    if (ui->surface) cairo_surface_destroy(ui->surface);
    if (ui->window) XDestroyWindow(ui->display, ui->window);
    if (ui->display) XCloseDisplay(ui->display);

    pthread_mutex_destroy(&ui->mutex);
    free(ui);
}

static void ui_port_event(LV2UI_Handle handle,
                          uint32_t port_index,
                          uint32_t buffer_size,
                          uint32_t format,
                          const void* buffer) {
    MetaLVUI* ui = (MetaLVUI*)handle;
    if (!ui || !buffer || format != 0 || buffer_size < sizeof(float)) return;

    pthread_mutex_lock(&ui->mutex);
    float value = *(const float*)buffer;

    for (int s = 0; s < kSlotCount; ++s) {
        int base = slot_port_base(s);
        if (port_index == (uint32_t)base) {
            ui->bypass[s] = clamp_value(value, 0.0f, 1.0f);
            ui->needs_redraw = true;
            break;
        }
        if (port_index == (uint32_t)(base + 1)) {
            ui->gain[s] = clamp_value(value, 0.0f, 2.0f);
            ui->needs_redraw = true;
            break;
        }
    }

    pthread_mutex_unlock(&ui->mutex);
}

static const void* ui_extension_data(const char* uri) {
    (void)uri;
    return NULL;
}

static const LV2UI_Descriptor ui_descriptor = {
    METALV_UI_URI,
    ui_instantiate,
    ui_cleanup,
    ui_port_event,
    ui_extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return (index == 0) ? &ui_descriptor : NULL;
}
