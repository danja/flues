#include <lv2/ui/ui.h>
#include <lv2/core/lv2.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#define MIDI_FLIP_URI "https://danja.github.io/flues/plugins/midi-flip"
#define MIDI_FLIP_UI_URI MIDI_FLIP_URI "#ui"

#define WINDOW_WIDTH 220
#define WINDOW_HEIGHT 280

#define MARGIN 18
#define TITLE_HEIGHT 18
#define SLIDER_WIDTH 24
#define SLIDER_HEIGHT 170
#define KNOB_HEIGHT 16

#define PORT_PIVOT 2

static const char* kNoteNames[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

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

    float value;
    bool dragging;
    double drag_start_y;
    float drag_start_value;

    volatile bool needs_redraw;
} MidiFlipUI;

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

static float clamp_value(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 127.0f) return 127.0f;
    return value;
}

static void send_value(MidiFlipUI* ui) {
    if (!ui->write) return;
    const float value = ui->value;
    ui->write(ui->controller, PORT_PIVOT, sizeof(float), 0, &value);
}

static void draw_ui(MidiFlipUI* ui) {
    cairo_t* cr = cairo_create(ui->surface);

    cairo_set_source_rgb(cr, 0.08, 0.09, 0.11);
    cairo_paint(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 13.0);
    cairo_set_source_rgb(cr, 0.95, 0.75, 0.35);
    cairo_move_to(cr, MARGIN, MARGIN + 12);
    cairo_show_text(cr, "MIDI FLIP");

    const int slider_x = (ui->width - SLIDER_WIDTH) / 2;
    const int slider_y = MARGIN + TITLE_HEIGHT + 8;

    cairo_set_source_rgb(cr, 0.15, 0.16, 0.20);
    cairo_rectangle(cr, slider_x, slider_y, SLIDER_WIDTH, SLIDER_HEIGHT);
    cairo_fill(cr);

    const float t = (ui->value / 127.0f);
    const float knob_y = slider_y + (1.0f - t) * (SLIDER_HEIGHT - KNOB_HEIGHT);

    cairo_set_source_rgb(cr, 0.95, 0.75, 0.35);
    cairo_rectangle(cr, slider_x - 2, knob_y, SLIDER_WIDTH + 4, KNOB_HEIGHT);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.85, 0.85, 0.90);
    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.0);
    const int note = (int)lroundf(ui->value);
    const int octave = (note / 12) - 1;
    const char* name = kNoteNames[note % 12];
    char label[48];
    snprintf(label, sizeof(label), "Pivot: %d (%s%d)", note, name, octave);
    cairo_move_to(cr, MARGIN, slider_y + SLIDER_HEIGHT + 24);
    cairo_show_text(cr, label);

    cairo_destroy(cr);
    cairo_surface_flush(ui->surface);
    XFlush(ui->display);
}

static bool slider_hit(MidiFlipUI* ui, int x, int y) {
    const int slider_x = (ui->width - SLIDER_WIDTH) / 2;
    const int slider_y = MARGIN + TITLE_HEIGHT + 8;

    return (x >= slider_x - 4 && x <= slider_x + SLIDER_WIDTH + 4 &&
            y >= slider_y - 4 && y <= slider_y + SLIDER_HEIGHT + 4);
}

static void update_from_y(MidiFlipUI* ui, int y) {
    const int slider_y = MARGIN + TITLE_HEIGHT + 8;
    float t = 1.0f - ((float)(y - slider_y) / (float)(SLIDER_HEIGHT - KNOB_HEIGHT));
    float value = t * 127.0f;
    value = clamp_value(value);
    if (fabsf(value - ui->value) > 0.001f) {
        ui->value = value;
        send_value(ui);
        ui->needs_redraw = true;
    }
}

static void handle_button_press(MidiFlipUI* ui, XButtonEvent* ev) {
    pthread_mutex_lock(&ui->mutex);
    if (slider_hit(ui, ev->x, ev->y)) {
        ui->dragging = true;
        ui->drag_start_y = ev->y;
        ui->drag_start_value = ui->value;
        update_from_y(ui, ev->y);
    }
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_button_release(MidiFlipUI* ui) {
    pthread_mutex_lock(&ui->mutex);
    ui->dragging = false;
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_motion(MidiFlipUI* ui, XMotionEvent* ev) {
    pthread_mutex_lock(&ui->mutex);
    if (ui->dragging) {
        update_from_y(ui, ev->y);
    }
    pthread_mutex_unlock(&ui->mutex);
}

static void* event_thread_main(void* data) {
    MidiFlipUI* ui = (MidiFlipUI*)data;

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

    MidiFlipUI* ui = (MidiFlipUI*)calloc(1, sizeof(MidiFlipUI));
    if (!ui) {
        return NULL;
    }

    ui->write = write_function;
    ui->controller = controller;
    ui->width = WINDOW_WIDTH;
    ui->height = WINDOW_HEIGHT;
    ui->value = 60.0f;

    void* parent = NULL;
    for (const LV2_Feature* const* f = features; f && *f; ++f) {
        if (!strcmp((*f)->URI, LV2_UI__parent)) {
            parent = (*f)->data;
        }
    }

    if (!parent) {
        fprintf(stderr, "midi-flip-ui: no parent window provided\n");
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

    XStoreName(display, ui->window, "MIDI Flip");
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
    MidiFlipUI* ui = (MidiFlipUI*)handle;
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
    MidiFlipUI* ui = (MidiFlipUI*)handle;
    if (!ui || !buffer || format != 0 || buffer_size < sizeof(float)) {
        return;
    }

    if (port_index != PORT_PIVOT) {
        return;
    }

    pthread_mutex_lock(&ui->mutex);
    float value = *(const float*)buffer;
    value = clamp_value(value);
    if (fabsf(value - ui->value) > 0.001f) {
        ui->value = value;
        ui->needs_redraw = true;
    }
    pthread_mutex_unlock(&ui->mutex);
}

static const void* ui_extension_data(const char* uri) {
    (void)uri;
    return NULL;
}

static const LV2UI_Descriptor ui_descriptor = {
    MIDI_FLIP_UI_URI,
    ui_instantiate,
    ui_cleanup,
    ui_port_event,
    ui_extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return (index == 0) ? &ui_descriptor : NULL;
}
