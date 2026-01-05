// flues_control_ui_x11.c - X11/Cairo UI for Flues Control (dynamic labels)

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
#include <math.h>
#include <unistd.h>

#define FLUES_CONTROL_URI "https://danja.github.io/flues/plugins/flues-control"
#define FLUES_CONTROL_UI_URI FLUES_CONTROL_URI "#ui"
#define LOG_PREFIX "[Flues Control UI] "

#define WINDOW_WIDTH 920
#define WINDOW_HEIGHT 360

#define HEADER_HEIGHT 48
#define SLIDER_AREA_TOP 72
#define SLIDER_HEIGHT 180
#define SLIDER_WIDTH 70
#define SLIDER_GAP 16
#define SLIDER_COUNT 9

#define PROGRAM_MIN 0
#define PROGRAM_MAX 30

enum {
    PORT_MIDI_IN = 0,
    PORT_MIDI_OUT = 1,
    PORT_PROGRAM = 2,
    PORT_SLIDER1 = 3,
    PORT_SLIDER2 = 4,
    PORT_SLIDER3 = 5,
    PORT_SLIDER4 = 6,
    PORT_SLIDER5 = 7,
    PORT_SLIDER6 = 8,
    PORT_SLIDER7 = 9,
    PORT_SLIDER8 = 10,
    PORT_SLIDER9 = 11
};

typedef struct {
    int x;
    int y;
    int width;
    int height;
    float value;
    uint32_t port;
} Slider;

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
    bool needs_redraw;

    int width;
    int height;

    Slider sliders[SLIDER_COUNT];
    int active_slider;

    int program_index;
    float program_value;

    int button_prev_x;
    int button_prev_y;
    int button_next_x;
    int button_next_y;
    int button_w;
    int button_h;
} FluesControlUI;

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

static float clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static void notify_host(FluesControlUI* ui, uint32_t port, float value) {
    if (ui->write) {
        ui->write(ui->controller, port, sizeof(float), 0, &value);
    }
}

static const char* kProgramNames[PROGRAM_MAX + 1] = {
    "Disyn Echo",
    "Disyn + Filter",
    "Trajectory Polygon",
    "Formant Voice",
    "Hybrid Speech",
    "Physical Model",
    "Full Hybrid",
    "Disyn Direct",
    "ModFM Formant",
    "DSF Inharmonic",
    "PAF Direct",
    "Cascaded DSF+PAF",
    "Tanh Spectral",
    "Hybrid DSF->Formant",
    "Feedback ModFM",
    "Dirichlet Explorer",
    "Multi-Algorithm",
    "Spectral Sculptor",
    "Hybrid Formant (Alg 7)",
    "Cascaded (Alg 8)",
    "Parallel Bank (Alg 9)",
    "Feedback (Alg 10)",
    "Morphing (Alg 11)",
    "Inharmonic (Alg 12)",
    "Adaptive Filter (Alg 13)",
    "Multi-Stage (Alg 14)",
    "Freq Asymmetry (Alg 15)",
    "Cross-Mod (Alg 16)",
    "Vocal Morph",
    "Taylor Series (Alg 17)",
    "Disyn + Delays (Legacy)"
};

static const char* kSliderLabels[PROGRAM_MAX + 1][SLIDER_COUNT] = {
    {"Alg", "Param1", "Param2", "Interface", "Intensity", "Tuning", "Delay1 FB", "Attack", "Release"},
    {"Filter\nFreq", "Filter Q", "Filter\nShape", "Disyn\nLevel", "Intensity", "Tuning", "Ratio", "Attack", "Release"},
    {"Sides", "Start\nPos", "Start\nAngle", "Master", "Clip", "-", "-", "Attack", "Release"},
    {"F1", "F2", "F3", "F4", "Noise", "Nasal", "Master", "Attack", "Release"},
    {"F1", "F2", "F3", "F4", "Disyn\nLevel", "Noise", "Master", "Attack", "Release"},
    {"Delay1 FB", "Delay2 FB", "Filter FB", "Interface", "Intensity", "Tuning", "Ratio", "Attack", "Release"},
    {"Delay1 FB", "Delay2 FB", "Filter FB", "Interface", "Intensity", "Tuning", "Ratio", "Attack", "Release"},
    {"Alg", "Param1", "Param2", "Disyn\nLevel", "Intensity", "Tuning", "Ratio", "Attack", "Release"},
    {"Index", "Ratio", "F1", "F2", "F3", "F4", "Master", "Attack", "Release"},
    {"Decay", "Ratio", "Delay1 FB", "Delay2 FB", "Intensity", "Tuning", "Delay\nRatio", "Attack", "Release"},
    {"Formant", "Bandw", "Filter\nFreq", "Filter Q", "Filter\nShape", "Tuning", "Master", "Attack", "Release"},
    {"Decay", "Ratio", "F1", "F2", "F3", "Tuning", "Master", "Attack", "Release"},
    {"Drive", "Blend", "Filter\nFreq", "Filter Q", "Filter\nShape", "Filter FB", "Intensity", "Attack", "Release"},
    {"Decay", "Ratio", "F1", "F2", "F3", "F4", "Master", "Attack", "Release"},
    {"Index", "Ratio", "Delay1 FB", "Delay2 FB", "Filter FB", "Tuning", "Delay\nRatio", "Attack", "Release"},
    {"Harmonics", "Tilt", "Filter\nFreq", "Filter Q", "Filter\nShape", "Tuning", "Master", "Attack", "Release"},
    {"Alg", "Param1", "Param2", "Disyn\nLevel", "Intensity", "Tuning", "Master", "Attack", "Release"},
    {"Decay", "Ratio", "Filter\nFreq", "Filter Q", "Filter\nShape", "Delay1 FB", "Filter FB", "Attack", "Release"},
    {"ModFM\nIdx", "PAF\nBandw", "Formant\nSpace", "Disyn\nLevel", "Intensity", "Tuning", "Master", "Attack", "Release"},
    {"DSF\nDecay", "Asym\nRatio", "Tanh\nDrive", "Filter\nFreq", "Filter Q", "Intensity", "Master", "Attack", "Release"},
    {"ModFM\nIdx", "PAF\nBandw", "Mix", "Intensity", "Tuning", "Filter\nFreq", "Master", "Attack", "Release"},
    {"ModFM\nIdx", "Feedback\nGain", "Feedback\nLP", "Intensity", "Tuning", "Filter\nFreq", "Master", "Attack", "Release"},
    {"Morph", "Character", "Curve", "Intensity", "Tuning", "Filter\nFreq", "Master", "Attack", "Release"},
    {"DSF\nDecay", "PAF\nShift", "PAF\nFormant", "Tuning", "Intensity", "Delay1 FB", "Master", "Attack", "Release"},
    {"Cutoff", "Reson", "Character", "Filter\nFreq", "Filter Q", "LFO\nFreq", "Master", "Attack", "Release"},
    {"Tanh\nDrive", "Exp\nDepth", "Ring\nCarrier", "Filter\nFreq", "Filter Q", "Intensity", "Master", "Attack", "Release"},
    {"Low r", "High r", "Xover", "Filter\nFreq", "Filter Q", "LFO\nFreq", "Master", "Attack", "Release"},
    {"DSF->FM", "PAF->Asym", "FM->DSF", "Intensity", "Tuning", "Filter\nFreq", "Master", "Attack", "Release"},
    {"Index", "Ratio", "F1", "F2", "Nasal", "Sing", "Shout", "Attack", "Release"},
    {"First\nTerms", "Second\nTerms", "Blend", "Master", "-", "-", "-", "Attack", "Release"},
    {"Delay1 FB", "Delay2 FB", "Filter FB", "Disyn\nLevel", "Intensity", "Tuning", "Ratio", "Attack", "Release"}
};

static void init_layout(FluesControlUI* ui) {
    int total_width = SLIDER_COUNT * SLIDER_WIDTH + (SLIDER_COUNT - 1) * SLIDER_GAP;
    int start_x = (ui->width - total_width) / 2;
    int y = SLIDER_AREA_TOP;

    for (int i = 0; i < SLIDER_COUNT; i++) {
        ui->sliders[i].x = start_x + i * (SLIDER_WIDTH + SLIDER_GAP);
        ui->sliders[i].y = y;
        ui->sliders[i].width = SLIDER_WIDTH;
        ui->sliders[i].height = SLIDER_HEIGHT;
        ui->sliders[i].port = PORT_SLIDER1 + i;
    }

    ui->button_w = 28;
    ui->button_h = 22;
    ui->button_prev_x = ui->width - 110;
    ui->button_prev_y = 16;
    ui->button_next_x = ui->width - 70;
    ui->button_next_y = 16;
}

static int slider_at(FluesControlUI* ui, int x, int y) {
    for (int i = 0; i < SLIDER_COUNT; i++) {
        Slider* slider = &ui->sliders[i];
        int left = slider->x;
        int right = slider->x + slider->width;
        int top = slider->y;
        int bottom = slider->y + slider->height;
        if (x >= left && x <= right && y >= top && y <= bottom) {
            return i;
        }
    }
    return -1;
}

static float slider_value_from_y(const Slider* slider, int y) {
    int top = slider->y;
    int bottom = slider->y + slider->height;
    float t = 1.0f - (float)(y - top) / (float)(bottom - top);
    return clamp01(t);
}

static bool hit_button(int x, int y, int bx, int by, int bw, int bh) {
    return x >= bx && x <= (bx + bw) && y >= by && y <= (by + bh);
}

static void draw_multiline(cairo_t* cr, float x, float y, const char* text, float line_height) {
    if (!text) return;
    const char* line_start = text;
    const char* p = text;
    float offset = 0.0f;

    while (true) {
        if (*p == '\n' || *p == '\0') {
            size_t len = (size_t)(p - line_start);
            char buffer[64];
            if (len >= sizeof(buffer)) len = sizeof(buffer) - 1;
            memcpy(buffer, line_start, len);
            buffer[len] = '\0';
            cairo_move_to(cr, x, y + offset);
            cairo_show_text(cr, buffer);
            offset += line_height;
            if (*p == '\0') break;
            line_start = p + 1;
        }
        p++;
    }
}

static void draw_slider(cairo_t* cr, const Slider* slider, const char* label) {
    float t = clamp01(slider->value);

    cairo_set_source_rgb(cr, 0.18, 0.18, 0.2);
    cairo_rectangle(cr, slider->x + slider->width / 2 - 3, slider->y, 6, slider->height);
    cairo_fill(cr);

    float knob_y = slider->y + (1.0f - t) * slider->height;
    cairo_set_source_rgb(cr, 0.2, 0.8, 0.8);
    cairo_rectangle(cr, slider->x, knob_y - 6, slider->width, 12);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    draw_multiline(cr, slider->x - 2, slider->y + slider->height + 16, label, 12.0f);
}

static void format_slider_value(char* out, size_t out_size, int program, int slider_index, float value) {
    const float t = clamp01(value);

    if (program == 2) {
        switch (slider_index) {
            case 0: {
                int sides = 3 + (int)roundf(t * 9.0f);
                snprintf(out, out_size, "%d", sides);
                return;
            }
            case 1:
            case 2: {
                int degrees = (int)roundf(t * 360.0f);
                snprintf(out, out_size, "%d", degrees);
                return;
            }
            case 3: {
                int percent = (int)roundf(t * 100.0f);
                snprintf(out, out_size, "%d%%", percent);
                return;
            }
            case 4: {
                float drive = 1.0f + t * 4.0f;
                snprintf(out, out_size, "x%.1f", drive);
                return;
            }
            default:
                break;
        }
    }

    snprintf(out, out_size, "%d", (int)roundf(t * 127.0f));
}

static void draw_ui(FluesControlUI* ui) {
    cairo_t* cr = cairo_create(ui->surface);

    cairo_set_source_rgb(cr, 0.06, 0.07, 0.09);
    cairo_paint(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 16.0);
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_move_to(cr, 20, 28);
    cairo_show_text(cr, "Flues Control");

    cairo_set_font_size(cr, 13.0);
    cairo_set_source_rgb(cr, 0.75, 0.85, 0.9);
    cairo_move_to(cr, 20, 46);
    cairo_show_text(cr, "Program:");

    const char* program_name = kProgramNames[ui->program_index];
    cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
    cairo_move_to(cr, 100, 46);
    cairo_show_text(cr, program_name);

    cairo_set_source_rgb(cr, 0.2, 0.3, 0.35);
    cairo_rectangle(cr, ui->button_prev_x, ui->button_prev_y, ui->button_w, ui->button_h);
    cairo_fill(cr);
    cairo_rectangle(cr, ui->button_next_x, ui->button_next_y, ui->button_w, ui->button_h);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
    cairo_move_to(cr, ui->button_prev_x + 9, ui->button_prev_y + 16);
    cairo_show_text(cr, "<");
    cairo_move_to(cr, ui->button_next_x + 9, ui->button_next_y + 16);
    cairo_show_text(cr, ">");

    char program_num[12];
    snprintf(program_num, sizeof(program_num), "%02d", ui->program_index);
    cairo_move_to(cr, ui->button_prev_x - 38, ui->button_prev_y + 16);
    cairo_show_text(cr, program_num);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.0);

    for (int i = 0; i < SLIDER_COUNT; i++) {
        draw_slider(cr, &ui->sliders[i], kSliderLabels[ui->program_index][i]);

        char value_text[32];
        format_slider_value(value_text, sizeof(value_text), ui->program_index, i, ui->sliders[i].value);
        cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
        cairo_move_to(cr, ui->sliders[i].x + 8, ui->sliders[i].y - 8);
        cairo_show_text(cr, value_text);
    }

    cairo_destroy(cr);
    cairo_surface_flush(ui->surface);
}

static void set_program(FluesControlUI* ui, int program, bool notify) {
    if (program < PROGRAM_MIN) program = PROGRAM_MIN;
    if (program > PROGRAM_MAX) program = PROGRAM_MAX;
    ui->program_index = program;
    ui->program_value = (float)program;
    if (notify) {
        notify_host(ui, PORT_PROGRAM, ui->program_value);
    }
    ui->needs_redraw = true;
}

static void handle_button_press(FluesControlUI* ui, int x, int y) {
    if (hit_button(x, y, ui->button_prev_x, ui->button_prev_y, ui->button_w, ui->button_h)) {
        set_program(ui, ui->program_index - 1, true);
        return;
    }
    if (hit_button(x, y, ui->button_next_x, ui->button_next_y, ui->button_w, ui->button_h)) {
        set_program(ui, ui->program_index + 1, true);
        return;
    }

    int slider_index = slider_at(ui, x, y);
    if (slider_index >= 0) {
        ui->active_slider = slider_index;
        Slider* slider = &ui->sliders[slider_index];
        float value = slider_value_from_y(slider, y);
        slider->value = value;
        notify_host(ui, slider->port, slider->value);
        ui->needs_redraw = true;
    }
}

static void handle_motion(FluesControlUI* ui, int x, int y) {
    if (ui->active_slider < 0) return;
    Slider* slider = &ui->sliders[ui->active_slider];
    float value = slider_value_from_y(slider, y);
    if (fabsf(value - slider->value) > 0.0001f) {
        slider->value = value;
        notify_host(ui, slider->port, slider->value);
        ui->needs_redraw = true;
    }
}

static void* event_thread_main(void* arg) {
    FluesControlUI* ui = (FluesControlUI*)arg;

    while (ui->running) {
        while (XPending(ui->display)) {
            XEvent event;
            XNextEvent(ui->display, &event);

            switch (event.type) {
                case Expose:
                    ui->needs_redraw = true;
                    break;
                case ButtonPress: {
                    pthread_mutex_lock(&ui->mutex);
                    handle_button_press(ui, event.xbutton.x, event.xbutton.y);
                    pthread_mutex_unlock(&ui->mutex);
                    break;
                }
                case ButtonRelease:
                    pthread_mutex_lock(&ui->mutex);
                    ui->active_slider = -1;
                    pthread_mutex_unlock(&ui->mutex);
                    break;
                case MotionNotify:
                    pthread_mutex_lock(&ui->mutex);
                    handle_motion(ui, event.xmotion.x, event.xmotion.y);
                    pthread_mutex_unlock(&ui->mutex);
                    break;
                case ConfigureNotify:
                    pthread_mutex_lock(&ui->mutex);
                    if (event.xconfigure.width != ui->width || event.xconfigure.height != ui->height) {
                        ui->width = event.xconfigure.width;
                        ui->height = event.xconfigure.height;
                        cairo_xlib_surface_set_size(ui->surface, ui->width, ui->height);
                        init_layout(ui);
                        ui->needs_redraw = true;
                    }
                    pthread_mutex_unlock(&ui->mutex);
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

static LV2UI_Handle ui_instantiate(const LV2UI_Descriptor* descriptor,
                                   const char* plugin_uri,
                                   const char* bundle_path,
                                   LV2UI_Write_Function write_function,
                                   LV2UI_Controller controller,
                                   LV2UI_Widget* widget,
                                   const LV2_Feature* const* features) {
    (void)descriptor;
    (void)bundle_path;

    if (strcmp(plugin_uri, FLUES_CONTROL_URI) != 0) {
        fprintf(stderr, LOG_PREFIX "Plugin URI mismatch (%s)\n", plugin_uri);
        return NULL;
    }

    ensure_xlib_threads();

    FluesControlUI* ui = calloc(1, sizeof(FluesControlUI));
    if (!ui) return NULL;

    pthread_mutex_init(&ui->mutex, NULL);
    ui->write = write_function;
    ui->controller = controller;
    ui->width = WINDOW_WIDTH;
    ui->height = WINDOW_HEIGHT;
    ui->active_slider = -1;
    ui->needs_redraw = true;

    Display* display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, LOG_PREFIX "Failed to open X display\n");
        pthread_mutex_destroy(&ui->mutex);
        free(ui);
        return NULL;
    }
    ui->display = display;
    ui->screen = DefaultScreen(display);

    Window parent = DefaultRootWindow(display);
    for (int i = 0; features && features[i]; i++) {
        if (!strcmp(features[i]->URI, LV2_UI__parent)) {
            parent = (Window)(uintptr_t)features[i]->data;
        }
    }

    XSetWindowAttributes attrs;
    attrs.background_pixel = BlackPixel(display, ui->screen);
    attrs.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

    ui->window = XCreateWindow(display,
                               parent,
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
        fprintf(stderr, LOG_PREFIX "Failed to create X window\n");
        XCloseDisplay(display);
        pthread_mutex_destroy(&ui->mutex);
        free(ui);
        return NULL;
    }

    XStoreName(display, ui->window, "Flues Control");
    XMapWindow(display, ui->window);
    XFlush(display);

    ui->surface = cairo_xlib_surface_create(display,
                                            ui->window,
                                            DefaultVisual(display, ui->screen),
                                            ui->width,
                                            ui->height);
    if (!ui->surface) {
        fprintf(stderr, LOG_PREFIX "Failed to create Cairo surface\n");
        XDestroyWindow(display, ui->window);
        XCloseDisplay(display);
        pthread_mutex_destroy(&ui->mutex);
        free(ui);
        return NULL;
    }

    init_layout(ui);

    for (int i = 0; i < SLIDER_COUNT; i++) {
        ui->sliders[i].value = 0.5f;
    }
    set_program(ui, 0, false);

    ui->running = true;
    if (pthread_create(&ui->thread, NULL, event_thread_main, ui) != 0) {
        fprintf(stderr, LOG_PREFIX "Failed to start event thread\n");
        cairo_surface_destroy(ui->surface);
        XDestroyWindow(display, ui->window);
        XCloseDisplay(display);
        pthread_mutex_destroy(&ui->mutex);
        free(ui);
        return NULL;
    }

    *widget = (LV2UI_Widget)(uintptr_t)ui->window;
    fprintf(stderr, LOG_PREFIX "UI instantiated, window=0x%lx\n", ui->window);
    return ui;
}

static void ui_cleanup(LV2UI_Handle handle) {
    FluesControlUI* ui = (FluesControlUI*)handle;
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
    FluesControlUI* ui = (FluesControlUI*)handle;
    if (!ui || !buffer || format != 0 || buffer_size < sizeof(float)) return;

    float value = *((const float*)buffer);

    pthread_mutex_lock(&ui->mutex);
    if (port_index == PORT_PROGRAM) {
        int program = (int)roundf(value);
        if (program < PROGRAM_MIN) program = PROGRAM_MIN;
        if (program > PROGRAM_MAX) program = PROGRAM_MAX;
        if (program != ui->program_index) {
            ui->program_index = program;
            ui->program_value = (float)program;
            ui->needs_redraw = true;
        }
    } else if (port_index >= PORT_SLIDER1 && port_index <= PORT_SLIDER9) {
        int slider_index = (int)(port_index - PORT_SLIDER1);
        if (slider_index >= 0 && slider_index < SLIDER_COUNT) {
            float clamped = clamp01(value);
            if (fabsf(clamped - ui->sliders[slider_index].value) > 0.0001f) {
                ui->sliders[slider_index].value = clamped;
                ui->needs_redraw = true;
            }
        }
    }
    pthread_mutex_unlock(&ui->mutex);
}

static const void* ui_extension_data(const char* uri) {
    (void)uri;
    return NULL;
}

static const LV2UI_Descriptor ui_descriptor = {
    FLUES_CONTROL_UI_URI,
    ui_instantiate,
    ui_cleanup,
    ui_port_event,
    ui_extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return (index == 0) ? &ui_descriptor : NULL;
}
