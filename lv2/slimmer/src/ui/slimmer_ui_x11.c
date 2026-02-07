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

#define SLIMMER_URI "https://danja.github.io/flues/plugins/slimmer"
#define SLIMMER_UI_URI SLIMMER_URI "#ui"

#define WINDOW_WIDTH 420
#define WINDOW_HEIGHT 300

#define MARGIN 18
#define TITLE_HEIGHT 18
#define SLIDER_WIDTH 24
#define SLIDER_HEIGHT 170
#define KNOB_HEIGHT 16
#define SLIDER_GAP_X 42
#define SLIDER_GAP_Y 40

#define PORT_MODE 2
#define PORT_GAP_MS 3
#define PORT_HOLD_MS 4
#define PORT_RETRIGGER 5

static const char* kModeNames[] = {
    "Highest",
    "Lowest",
    "Nearest",
    "Farthest",
    "Velocity",
    "Most Recent",
    "Oldest",
    "Center",
    "Round Robin",
    "Random"
};

static const int kModeCount = (int)(sizeof(kModeNames) / sizeof(kModeNames[0]));

static const char* kRetriggerNames[] = {"Off", "On"};

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

    float mode_value;
    float gap_value;
    float hold_value;
    float retrigger_value;

    bool dragging_mode;
    bool dragging_gap;
    bool dragging_hold;
    bool dragging_retrigger;

    volatile bool needs_redraw;
} SlimmerUI;

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

static float clamp_value(float value, float min_val, float max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

static int slider_x(int col, int total_width) {
    const int group_width = (2 * SLIDER_WIDTH) + SLIDER_GAP_X;
    const int start = (total_width - group_width) / 2;
    return start + (col * (SLIDER_WIDTH + SLIDER_GAP_X));
}

static int slider_y(int row) {
    return MARGIN + TITLE_HEIGHT + 8 + (row * (SLIDER_HEIGHT + SLIDER_GAP_Y));
}

static void send_value(SlimmerUI* ui, uint32_t port, float value) {
    if (!ui->write) return;
    ui->write(ui->controller, port, sizeof(float), 0, &value);
}

static void draw_slider(cairo_t* cr, int x, int y, float value, float max_value) {
    cairo_set_source_rgb(cr, 0.15, 0.16, 0.20);
    cairo_rectangle(cr, x, y, SLIDER_WIDTH, SLIDER_HEIGHT);
    cairo_fill(cr);

    float t = (max_value <= 0.0f) ? 0.0f : (value / max_value);
    float knob_y = y + (1.0f - t) * (SLIDER_HEIGHT - KNOB_HEIGHT);

    cairo_set_source_rgb(cr, 0.95, 0.75, 0.35);
    cairo_rectangle(cr, x - 2, knob_y, SLIDER_WIDTH + 4, KNOB_HEIGHT);
    cairo_fill(cr);
}

static void draw_ui(SlimmerUI* ui) {
    cairo_t* cr = cairo_create(ui->surface);

    cairo_set_source_rgb(cr, 0.08, 0.09, 0.11);
    cairo_paint(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 13.0);
    cairo_set_source_rgb(cr, 0.95, 0.75, 0.35);
    cairo_move_to(cr, MARGIN, MARGIN + 12);
    cairo_show_text(cr, "SLIMMER");

    const int mode_x = slider_x(0, ui->width);
    const int gap_x = slider_x(1, ui->width);
    const int hold_x = slider_x(0, ui->width);
    const int retrigger_x = slider_x(1, ui->width);

    const int top_y = slider_y(0);
    const int bottom_y = slider_y(1);

    draw_slider(cr, mode_x, top_y, ui->mode_value, (float)(kModeCount - 1));
    draw_slider(cr, gap_x, top_y, ui->gap_value, 500.0f);
    draw_slider(cr, hold_x, bottom_y, ui->hold_value, 1000.0f);
    draw_slider(cr, retrigger_x, bottom_y, ui->retrigger_value, 1.0f);

    cairo_set_source_rgb(cr, 0.85, 0.85, 0.90);
    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.0);

    int mode_index = (int)lroundf(ui->mode_value);
    if (mode_index < 0) mode_index = 0;
    if (mode_index >= kModeCount) mode_index = kModeCount - 1;

    char gap_label[32];
    char hold_label[32];
    snprintf(gap_label, sizeof(gap_label), "Gap: %d ms", (int)lroundf(ui->gap_value));
    snprintf(hold_label, sizeof(hold_label), "Hold: %d ms", (int)lroundf(ui->hold_value));

    cairo_move_to(cr, mode_x - 8, top_y + SLIDER_HEIGHT + 24);
    cairo_show_text(cr, kModeNames[mode_index]);

    cairo_move_to(cr, gap_x - 8, top_y + SLIDER_HEIGHT + 24);
    cairo_show_text(cr, gap_label);

    cairo_move_to(cr, hold_x - 8, bottom_y + SLIDER_HEIGHT + 24);
    cairo_show_text(cr, hold_label);

    int retrigger_index = (int)lroundf(ui->retrigger_value);
    if (retrigger_index < 0) retrigger_index = 0;
    if (retrigger_index > 1) retrigger_index = 1;

    cairo_move_to(cr, retrigger_x - 8, bottom_y + SLIDER_HEIGHT + 24);
    cairo_show_text(cr, kRetriggerNames[retrigger_index]);

    cairo_destroy(cr);
    cairo_surface_flush(ui->surface);
    XFlush(ui->display);
}

static bool slider_hit(int x, int y, int sx, int sy) {
    return (x >= sx - 4 && x <= sx + SLIDER_WIDTH + 4 &&
            y >= sy - 4 && y <= sy + SLIDER_HEIGHT + 4);
}

static float value_from_y(int y, int sy, float max_value) {
    float t = 1.0f - ((float)(y - sy) / (float)(SLIDER_HEIGHT - KNOB_HEIGHT));
    float value = t * max_value;
    return clamp_value(value, 0.0f, max_value);
}

static void handle_button_press(SlimmerUI* ui, XButtonEvent* ev) {
    pthread_mutex_lock(&ui->mutex);

    const int top_y = slider_y(0);
    const int bottom_y = slider_y(1);

    const int mode_x = slider_x(0, ui->width);
    const int gap_x = slider_x(1, ui->width);
    const int hold_x = slider_x(0, ui->width);
    const int retrigger_x = slider_x(1, ui->width);

    if (slider_hit(ev->x, ev->y, mode_x, top_y)) {
        ui->dragging_mode = true;
        ui->mode_value = value_from_y(ev->y, top_y, (float)(kModeCount - 1));
        send_value(ui, PORT_MODE, ui->mode_value);
        ui->needs_redraw = true;
    } else if (slider_hit(ev->x, ev->y, gap_x, top_y)) {
        ui->dragging_gap = true;
        ui->gap_value = value_from_y(ev->y, top_y, 500.0f);
        send_value(ui, PORT_GAP_MS, ui->gap_value);
        ui->needs_redraw = true;
    } else if (slider_hit(ev->x, ev->y, hold_x, bottom_y)) {
        ui->dragging_hold = true;
        ui->hold_value = value_from_y(ev->y, bottom_y, 1000.0f);
        send_value(ui, PORT_HOLD_MS, ui->hold_value);
        ui->needs_redraw = true;
    } else if (slider_hit(ev->x, ev->y, retrigger_x, bottom_y)) {
        ui->dragging_retrigger = true;
        ui->retrigger_value = value_from_y(ev->y, bottom_y, 1.0f);
        send_value(ui, PORT_RETRIGGER, ui->retrigger_value);
        ui->needs_redraw = true;
    }

    pthread_mutex_unlock(&ui->mutex);
}

static void handle_button_release(SlimmerUI* ui) {
    pthread_mutex_lock(&ui->mutex);
    ui->dragging_mode = false;
    ui->dragging_gap = false;
    ui->dragging_hold = false;
    ui->dragging_retrigger = false;
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_motion(SlimmerUI* ui, XMotionEvent* ev) {
    pthread_mutex_lock(&ui->mutex);

    const int top_y = slider_y(0);
    const int bottom_y = slider_y(1);

    if (ui->dragging_mode) {
        ui->mode_value = value_from_y(ev->y, top_y, (float)(kModeCount - 1));
        send_value(ui, PORT_MODE, ui->mode_value);
        ui->needs_redraw = true;
    } else if (ui->dragging_gap) {
        ui->gap_value = value_from_y(ev->y, top_y, 500.0f);
        send_value(ui, PORT_GAP_MS, ui->gap_value);
        ui->needs_redraw = true;
    } else if (ui->dragging_hold) {
        ui->hold_value = value_from_y(ev->y, bottom_y, 1000.0f);
        send_value(ui, PORT_HOLD_MS, ui->hold_value);
        ui->needs_redraw = true;
    } else if (ui->dragging_retrigger) {
        ui->retrigger_value = value_from_y(ev->y, bottom_y, 1.0f);
        send_value(ui, PORT_RETRIGGER, ui->retrigger_value);
        ui->needs_redraw = true;
    }

    pthread_mutex_unlock(&ui->mutex);
}

static void* event_thread_main(void* data) {
    SlimmerUI* ui = (SlimmerUI*)data;

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

    SlimmerUI* ui = (SlimmerUI*)calloc(1, sizeof(SlimmerUI));
    if (!ui) return NULL;

    ui->write = write_function;
    ui->controller = controller;
    ui->width = WINDOW_WIDTH;
    ui->height = WINDOW_HEIGHT;
    ui->mode_value = 0.0f;
    ui->gap_value = 30.0f;
    ui->hold_value = 120.0f;
    ui->retrigger_value = 0.0f;

    void* parent = NULL;
    for (const LV2_Feature* const* f = features; f && *f; ++f) {
        if (!strcmp((*f)->URI, LV2_UI__parent)) {
            parent = (*f)->data;
        }
    }

    if (!parent) {
        fprintf(stderr, "slimmer-ui: no parent window provided\n");
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

    XStoreName(display, ui->window, "Slimmer");
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
    SlimmerUI* ui = (SlimmerUI*)handle;
    if (!ui) return;

    ui->running = false;
    pthread_join(ui->thread, NULL);

    if (ui->surface) {
        cairo_surface_destroy(ui->surface);
    }
    if (ui->window) {
        XDestroyWindow(ui->display, ui->window);
    }
    if (ui->display) {
        XCloseDisplay(ui->display);
    }
    pthread_mutex_destroy(&ui->mutex);
    free(ui);
}

static void ui_port_event(LV2UI_Handle handle,
                          uint32_t port_index,
                          uint32_t buffer_size,
                          uint32_t format,
                          const void* buffer) {
    SlimmerUI* ui = (SlimmerUI*)handle;
    if (!ui || !buffer || format != 0 || buffer_size < sizeof(float)) return;

    pthread_mutex_lock(&ui->mutex);
    float value = *(const float*)buffer;
    if (port_index == PORT_MODE) {
        ui->mode_value = clamp_value(value, 0.0f, (float)(kModeCount - 1));
        ui->needs_redraw = true;
    } else if (port_index == PORT_GAP_MS) {
        ui->gap_value = clamp_value(value, 0.0f, 500.0f);
        ui->needs_redraw = true;
    } else if (port_index == PORT_HOLD_MS) {
        ui->hold_value = clamp_value(value, 0.0f, 1000.0f);
        ui->needs_redraw = true;
    } else if (port_index == PORT_RETRIGGER) {
        ui->retrigger_value = clamp_value(value, 0.0f, 1.0f);
        ui->needs_redraw = true;
    }
    pthread_mutex_unlock(&ui->mutex);
}

static const void* ui_extension_data(const char* uri) {
    (void)uri;
    return NULL;
}

static const LV2UI_Descriptor ui_descriptor = {
    SLIMMER_UI_URI,
    ui_instantiate,
    ui_cleanup,
    ui_port_event,
    ui_extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return (index == 0) ? &ui_descriptor : NULL;
}
