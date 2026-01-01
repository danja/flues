// memone_ui_x11.c - Minimal X11/Cairo UI for Memone

#include <lv2/ui/ui.h>
#include <lv2/core/lv2.h>

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#define MEMONE_URI "https://danja.github.io/flues/plugins/memone"
#define MEMONE_UI_URI MEMONE_URI "#ui"

enum {
    PORT_CONTROL = 0,
    PORT_AUDIO_IN_L = 1,
    PORT_AUDIO_IN_R = 2,
    PORT_AUDIO_OUT_L = 3,
    PORT_AUDIO_OUT_R = 4,
    PORT_BPM = 5,
    PORT_BEATS_WARMUP = 6,
    PORT_PREDICT_GAIN = 7,
    PORT_PREDICT_HORIZON = 8,
    PORT_LEARNING_RATE = 9,
    PORT_BPTT_LENGTH = 10,
    PORT_GRAD_CLIP = 11,
    PORT_HIDDEN_SIZE = 12,
    PORT_RESET = 13,
    PORT_LR_CLAMP = 14,
    PORT_LR_MIN = 15,
    PORT_LR_MAX = 16,
    PORT_STATUS = 17
};

typedef struct {
    int x;
    int y;
    int width;
    int height;
    float min;
    float max;
    float value;
    uint32_t port;
    const char* label;
} Slider;

typedef struct {
    Display* display;
    Window window;
    cairo_surface_t* surface;
    int screen;

    pthread_t thread;
    pthread_mutex_t mutex;
    bool running;

    int width;
    int height;
    Slider sliders[11];
    int slider_count;
    int active_slider;
    bool needs_redraw;
    bool reset_pressed;
    float status_value;

    LV2UI_Write_Function write;
    LV2UI_Controller controller;
} MemoneUI;

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

static float clamp_value(const Slider* slider, float value) {
    if (value < slider->min) return slider->min;
    if (value > slider->max) return slider->max;
    return value;
}

static void notify_host(MemoneUI* ui, uint32_t port, float value) {
    if (ui->write) {
        ui->write(ui->controller, port, sizeof(float), 0, &value);
    }
}

static int slider_at(MemoneUI* ui, int x, int y) {
    for (int i = 0; i < ui->slider_count; ++i) {
        const Slider* slider = &ui->sliders[i];
        const int left = slider->x;
        const int top = slider->y;
        const int right = left + slider->width;
        const int bottom = top + slider->height;
        if (x >= left && x <= right && y >= top && y <= bottom) {
            return i;
        }
    }
    return -1;
}

static float slider_value_from_y(const Slider* slider, int y) {
    const int top = slider->y;
    const int bottom = slider->y + slider->height;
    float t = 1.0f - (float)(y - top) / (float)(bottom - top);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return slider->min + t * (slider->max - slider->min);
}

static bool reset_hit(MemoneUI* ui, int x, int y) {
    const int btn_w = 90;
    const int btn_h = 28;
    const int btn_x = ui->width - btn_w - 16;
    const int btn_y = ui->height - btn_h - 12;
    return x >= btn_x && x <= btn_x + btn_w && y >= btn_y && y <= btn_y + btn_h;
}

static void draw_slider(cairo_t* cr, const Slider* slider) {
    const float range = slider->max - slider->min;
    float t = range > 0.0f ? (slider->value - slider->min) / range : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_rectangle(cr, slider->x + slider->width / 2 - 3, slider->y, 6, slider->height);
    cairo_fill(cr);

    const float knob_y = slider->y + (1.0f - t) * slider->height;
    cairo_set_source_rgb(cr, 0.9, 0.7, 0.2);
    cairo_rectangle(cr, slider->x, knob_y - 6, slider->width, 12);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_move_to(cr, slider->x - 4, slider->y + slider->height + 16);
    cairo_show_text(cr, slider->label);

    char value_text[32];
    if (slider->max <= 0.1f) {
        snprintf(value_text, sizeof(value_text), "%.4f", slider->value);
    } else if (slider->max >= 100.0f) {
        snprintf(value_text, sizeof(value_text), "%.0f", slider->value);
    } else {
        snprintf(value_text, sizeof(value_text), "%.2f", slider->value);
    }
    cairo_move_to(cr, slider->x - 2, slider->y + slider->height + 30);
    cairo_show_text(cr, value_text);
}

static void draw_ui(MemoneUI* ui) {
    cairo_t* cr = cairo_create(ui->surface);

    cairo_set_source_rgb(cr, 0.08, 0.08, 0.1);
    cairo_paint(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14.0);

    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_move_to(cr, 16, 18);
    cairo_show_text(cr, "Memone");

    for (int i = 0; i < ui->slider_count; ++i) {
        draw_slider(cr, &ui->sliders[i]);
    }

    const char* status_text = ui->status_value >= 0.5f ? "Predict" : "Warmup";
    const double status_r = ui->status_value >= 0.5f ? 0.2 : 0.7;
    const double status_g = ui->status_value >= 0.5f ? 0.8 : 0.6;
    const double status_b = ui->status_value >= 0.5f ? 0.3 : 0.2;

    cairo_set_source_rgb(cr, status_r, status_g, status_b);
    cairo_arc(cr, 24, ui->height - 20, 6, 0, 2 * M_PI);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_move_to(cr, 38, ui->height - 16);
    cairo_show_text(cr, status_text);

    const int btn_w = 90;
    const int btn_h = 28;
    const int btn_x = ui->width - btn_w - 16;
    const int btn_y = ui->height - btn_h - 12;
    if (ui->reset_pressed) {
        cairo_set_source_rgb(cr, 0.8, 0.3, 0.2);
    } else {
        cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    }
    cairo_rectangle(cr, btn_x, btn_y, btn_w, btn_h);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
    cairo_move_to(cr, btn_x + 20, btn_y + 18);
    cairo_show_text(cr, "Reset");

    cairo_destroy(cr);
}

static void setup_layout(MemoneUI* ui) {
    ui->width = 1070;
    ui->height = 200;
    ui->slider_count = 11;

    const int slider_width = 40;
    const int slider_height = 120;
    const int top = 40;
    const int gap = 30;
    const int start_x = 30;

    ui->sliders[0] = (Slider){
        .x = start_x,
        .y = top,
        .width = slider_width,
        .height = slider_height,
        .min = 40.0f,
        .max = 240.0f,
        .value = 120.0f,
        .port = PORT_BPM,
        .label = "BPM"
    };

    ui->sliders[1] = (Slider){
        .x = start_x + (slider_width + gap),
        .y = top,
        .width = slider_width,
        .height = slider_height,
        .min = 0.0f,
        .max = 16.0f,
        .value = 4.0f,
        .port = PORT_BEATS_WARMUP,
        .label = "Warmup"
    };

    ui->sliders[2] = (Slider){
        .x = start_x + 2 * (slider_width + gap),
        .y = top,
        .width = slider_width,
        .height = slider_height,
        .min = 0.0f,
        .max = 2.0f,
        .value = 1.0f,
        .port = PORT_PREDICT_GAIN,
        .label = "Gain"
    };

    ui->sliders[3] = (Slider){
        .x = start_x + 3 * (slider_width + gap),
        .y = top,
        .width = slider_width,
        .height = slider_height,
        .min = 1.0f,
        .max = 256.0f,
        .value = 64.0f,
        .port = PORT_PREDICT_HORIZON,
        .label = "Horizon"
    };

    ui->sliders[4] = (Slider){
        .x = start_x + 4 * (slider_width + gap),
        .y = top,
        .width = slider_width,
        .height = slider_height,
        .min = 0.0f,
        .max = 0.01f,
        .value = 0.0005f,
        .port = PORT_LEARNING_RATE,
        .label = "Learn"
    };

    ui->sliders[5] = (Slider){
        .x = start_x + 5 * (slider_width + gap),
        .y = top,
        .width = slider_width,
        .height = slider_height,
        .min = 1.0f,
        .max = 64.0f,
        .value = 32.0f,
        .port = PORT_BPTT_LENGTH,
        .label = "BPTT"
    };

    ui->sliders[6] = (Slider){
        .x = start_x + 6 * (slider_width + gap),
        .y = top,
        .width = slider_width,
        .height = slider_height,
        .min = 0.1f,
        .max = 20.0f,
        .value = 5.0f,
        .port = PORT_GRAD_CLIP,
        .label = "Clip"
    };

    ui->sliders[7] = (Slider){
        .x = start_x + 7 * (slider_width + gap),
        .y = top,
        .width = slider_width,
        .height = slider_height,
        .min = 0.0f,
        .max = 1.0f,
        .value = 1.0f,
        .port = PORT_LR_CLAMP,
        .label = "LR Clamp"
    };

    ui->sliders[8] = (Slider){
        .x = start_x + 8 * (slider_width + gap),
        .y = top,
        .width = slider_width,
        .height = slider_height,
        .min = 0.0f,
        .max = 0.01f,
        .value = 0.0001f,
        .port = PORT_LR_MIN,
        .label = "LR Min"
    };

    ui->sliders[9] = (Slider){
        .x = start_x + 9 * (slider_width + gap),
        .y = top,
        .width = slider_width,
        .height = slider_height,
        .min = 0.0f,
        .max = 0.05f,
        .value = 0.005f,
        .port = PORT_LR_MAX,
        .label = "LR Max"
    };

    ui->sliders[10] = (Slider){
        .x = start_x + 10 * (slider_width + gap),
        .y = top,
        .width = slider_width,
        .height = slider_height,
        .min = 1.0f,
        .max = 16.0f,
        .value = 8.0f,
        .port = PORT_HIDDEN_SIZE,
        .label = "Hidden"
    };
}

static void* event_thread_main(void* arg) {
    MemoneUI* ui = (MemoneUI*)arg;

    while (ui->running) {
        while (XPending(ui->display)) {
            XEvent ev;
            XNextEvent(ui->display, &ev);

            if (ev.type == Expose) {
                pthread_mutex_lock(&ui->mutex);
                ui->needs_redraw = true;
                pthread_mutex_unlock(&ui->mutex);
            } else if (ev.type == ButtonPress) {
                const int x = ev.xbutton.x;
                const int y = ev.xbutton.y;
                pthread_mutex_lock(&ui->mutex);
                const int idx = slider_at(ui, x, y);
                if (idx >= 0) {
                    ui->active_slider = idx;
                    Slider* slider = &ui->sliders[idx];
                    slider->value = clamp_value(slider, slider_value_from_y(slider, y));
                    notify_host(ui, slider->port, slider->value);
                    ui->needs_redraw = true;
                } else if (reset_hit(ui, x, y)) {
                    ui->reset_pressed = true;
                    notify_host(ui, PORT_RESET, 1.0f);
                    ui->needs_redraw = true;
                }
                pthread_mutex_unlock(&ui->mutex);
            } else if (ev.type == ButtonRelease) {
                pthread_mutex_lock(&ui->mutex);
                ui->active_slider = -1;
                if (ui->reset_pressed) {
                    ui->reset_pressed = false;
                    notify_host(ui, PORT_RESET, 0.0f);
                    ui->needs_redraw = true;
                }
                pthread_mutex_unlock(&ui->mutex);
            } else if (ev.type == MotionNotify) {
                pthread_mutex_lock(&ui->mutex);
                if (ui->active_slider >= 0) {
                    Slider* slider = &ui->sliders[ui->active_slider];
                    slider->value = clamp_value(slider, slider_value_from_y(slider, ev.xmotion.y));
                    notify_host(ui, slider->port, slider->value);
                    ui->needs_redraw = true;
                }
                pthread_mutex_unlock(&ui->mutex);
            }
        }

        pthread_mutex_lock(&ui->mutex);
        if (ui->needs_redraw) {
            draw_ui(ui);
            ui->needs_redraw = false;
        }
        pthread_mutex_unlock(&ui->mutex);

        usleep(16000);
    }

    return NULL;
}

static LV2UI_Handle ui_instantiate(
    const LV2UI_Descriptor*,
    const char*,
    const char*,
    LV2UI_Write_Function write_function,
    LV2UI_Controller controller,
    LV2UI_Widget* widget,
    const LV2_Feature* const* features
) {
    ensure_xlib_threads();

    MemoneUI* ui = (MemoneUI*)calloc(1, sizeof(MemoneUI));
    if (!ui) {
        return NULL;
    }

    pthread_mutex_init(&ui->mutex, NULL);
    ui->write = write_function;
    ui->controller = controller;
    ui->active_slider = -1;

    ui->display = XOpenDisplay(NULL);
    if (!ui->display) {
        pthread_mutex_destroy(&ui->mutex);
        free(ui);
        return NULL;
    }
    ui->screen = DefaultScreen(ui->display);

    Window parent = DefaultRootWindow(ui->display);
    for (int i = 0; features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_UI__parent)) {
            parent = (Window)(uintptr_t)features[i]->data;
        }
    }

    setup_layout(ui);

    XSetWindowAttributes attrs;
    attrs.event_mask = ExposureMask | StructureNotifyMask |
                       ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
    attrs.background_pixel = BlackPixel(ui->display, ui->screen);

    ui->window = XCreateWindow(ui->display, parent, 0, 0, ui->width, ui->height, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWBackPixel | CWEventMask, &attrs);

    Cursor handCursor = XCreateFontCursor(ui->display, XC_hand2);
    XDefineCursor(ui->display, ui->window, handCursor);

    XMapWindow(ui->display, ui->window);

    ui->surface = cairo_xlib_surface_create(ui->display, ui->window,
                                            DefaultVisual(ui->display, ui->screen),
                                            ui->width, ui->height);

    ui->running = true;
    ui->needs_redraw = true;
    pthread_create(&ui->thread, NULL, event_thread_main, ui);

    *widget = (LV2UI_Widget)(uintptr_t)ui->window;
    return ui;
}

static void ui_cleanup(LV2UI_Handle handle) {
    MemoneUI* ui = (MemoneUI*)handle;
    if (!ui) {
        return;
    }

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

static void ui_port_event(
    LV2UI_Handle handle,
    uint32_t port_index,
    uint32_t buffer_size,
    uint32_t format,
    const void* buffer
) {
    MemoneUI* ui = (MemoneUI*)handle;
    if (!ui || !buffer || format != 0) {
        return;
    }
    (void)buffer_size;

    const float value = *((const float*)buffer);

    pthread_mutex_lock(&ui->mutex);
    if (port_index == PORT_STATUS) {
        ui->status_value = value;
        ui->needs_redraw = true;
    } else {
        for (int i = 0; i < ui->slider_count; ++i) {
            if (ui->sliders[i].port == port_index) {
                ui->sliders[i].value = clamp_value(&ui->sliders[i], value);
                ui->needs_redraw = true;
                break;
            }
        }
    }
    pthread_mutex_unlock(&ui->mutex);
}

static const LV2UI_Descriptor descriptor = {
    MEMONE_UI_URI,
    ui_instantiate,
    ui_cleanup,
    ui_port_event,
    NULL
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : NULL;
}
