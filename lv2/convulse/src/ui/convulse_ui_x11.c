#include <lv2/core/lv2.h>
#include <lv2/ui/ui.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

#define CONVULSE_URI "https://danja.github.io/flues/plugins/convulse"
#define CONVULSE_UI_URI CONVULSE_URI "#ui"

#define MARGIN 18
#define TITLE_HEIGHT 20
#define KNOB_DIAMETER 54
#define KNOB_HEIGHT 80
#define EDIT_HEIGHT 22
#define KNOB_SPACING 14
#define EDIT_PADDING 6
#define LABEL_HEIGHT 14

typedef enum {
    PORT_CONTROL = 0,
    PORT_AUDIO_IN_L,
    PORT_AUDIO_IN_R,
    PORT_AUDIO_OUT_L,
    PORT_AUDIO_OUT_R,
    PORT_DRY_WET,
    PORT_KERNEL_SIZE,
    PORT_MODE,
    PORT_PITCH,
    PORT_SHAPE,
    PORT_DECAY,
    PORT_REFRESH,
    PORT_STEREO_WIDTH,
    PORT_FEEDBACK,
    PORT_TOTAL_COUNT
} PortIndex;

typedef struct {
    const char* label;
    uint32_t port;
    float min;
    float max;
    float def;
    bool is_int;
} ControlDesc;

typedef struct {
    uint32_t port;
    const char* label;
    float min;
    float max;
    float def;
    float value;
    bool is_int;
    int x;
    int y;
    int width;
    int height;
    int edit_x;
    int edit_y;
    int edit_w;
    int edit_h;
    char edit_text[32];
} Knob;

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

    Knob knobs[12];
    int knob_count;

    volatile bool needs_redraw;
    int active_knob;
    double drag_start_y;
    float drag_start_value;

    int active_edit;
    size_t edit_cursor;
} ConvulseUI;

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

static const ControlDesc kControls[] = {
    { "DRY", PORT_DRY_WET, 0.0f, 1.0f, 0.5f, false },
    { "SIZE", PORT_KERNEL_SIZE, 32.0f, 1024.0f, 256.0f, true },
    { "MODE", PORT_MODE, 0.0f, 5.0f, 0.0f, true },
    { "PITCH", PORT_PITCH, 20.0f, 2000.0f, 220.0f, false },
    { "SHAPE", PORT_SHAPE, 0.0f, 1.0f, 0.5f, false },
    { "DECAY", PORT_DECAY, 0.0f, 1.0f, 0.6f, false },
    { "REFRESH", PORT_REFRESH, 0.0f, 8.0f, 0.5f, false },
    { "WIDTH", PORT_STEREO_WIDTH, 0.0f, 1.0f, 0.3f, false },
    { "FB", PORT_FEEDBACK, 0.0f, 95.0f, 10.0f, true }
};

static void send_value(ConvulseUI* ui, const Knob* knob);

static const char* mode_name_from_value(float value) {
    switch ((int)lroundf(value)) {
        case 0: return "Sine";
        case 1: return "Saw";
        case 2: return "Pulse";
        case 3: return "Noise";
        case 4: return "FM";
        case 5: return "Chirp";
        default: return "Sine";
    }
}

static bool parse_mode_name(const char* text, float* out_value) {
    if (!text || !out_value) {
        return false;
    }

    if (strcasecmp(text, "sine") == 0) {
        *out_value = 0.0f;
        return true;
    }
    if (strcasecmp(text, "saw") == 0) {
        *out_value = 1.0f;
        return true;
    }
    if (strcasecmp(text, "pulse") == 0) {
        *out_value = 2.0f;
        return true;
    }
    if (strcasecmp(text, "noise") == 0) {
        *out_value = 3.0f;
        return true;
    }
    if (strcasecmp(text, "fm") == 0) {
        *out_value = 4.0f;
        return true;
    }
    if (strcasecmp(text, "chirp") == 0) {
        *out_value = 5.0f;
        return true;
    }

    return false;
}

static float clamp_value(const Knob* knob, float value) {
    if (value < knob->min) return knob->min;
    if (value > knob->max) return knob->max;
    return value;
}

static void format_value(const Knob* knob, float value, char* out, size_t out_size) {
    if (knob->port == PORT_MODE) {
        snprintf(out, out_size, "%s", mode_name_from_value(value));
        return;
    }
    if (knob->is_int) {
        snprintf(out, out_size, "%.0f", value);
    } else if (knob->max > 10.0f) {
        snprintf(out, out_size, "%.1f", value);
    } else {
        snprintf(out, out_size, "%.3f", value);
    }
}

static void update_edit_text(Knob* knob) {
    format_value(knob, knob->value, knob->edit_text, sizeof(knob->edit_text));
}

static void draw_knob(cairo_t* cr, const Knob* knob, bool active, bool editing) {
    const double x = knob->x;
    const double y = knob->y;
    const double diameter = KNOB_DIAMETER;
    const double radius = diameter / 2.0;
    const double center_x = x + radius;
    const double center_y = y + radius;

    cairo_save(cr);
    cairo_set_source_rgb(cr, 0.12, 0.13, 0.16);
    cairo_rectangle(cr, x - 6, y - 6, diameter + 12, KNOB_HEIGHT + EDIT_HEIGHT + LABEL_HEIGHT + 12);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.22, 0.23, 0.26);
    cairo_arc(cr, center_x, center_y, radius, 0, 2 * M_PI);
    cairo_fill(cr);

    const double t = (knob->value - knob->min) / (knob->max - knob->min);
    const double start_angle = -0.75 * M_PI;
    const double end_angle = start_angle + (1.5 * M_PI * t);
    cairo_set_line_width(cr, 4.0);
    cairo_set_source_rgb(cr, 0.95, 0.75, 0.35);
    cairo_arc(cr, center_x, center_y, radius - 5, start_angle, end_angle);
    cairo_stroke(cr);

    cairo_set_line_width(cr, 3.0);
    cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
    cairo_move_to(cr, center_x, center_y);
    cairo_line_to(cr,
                  center_x + cos(end_angle) * (radius - 8),
                  center_y + sin(end_angle) * (radius - 8));
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.85, 0.85, 0.9);
    cairo_move_to(cr, x, y + diameter + LABEL_HEIGHT - 2);
    cairo_show_text(cr, knob->label);

    cairo_rectangle(cr, knob->edit_x, knob->edit_y, knob->edit_w, knob->edit_h);
    cairo_set_source_rgb(cr, editing ? 0.18 : 0.12, editing ? 0.22 : 0.14, editing ? 0.30 : 0.18);
    cairo_fill(cr);

    cairo_rectangle(cr, knob->edit_x, knob->edit_y, knob->edit_w, knob->edit_h);
    cairo_set_source_rgb(cr, editing ? 0.95 : 0.35, editing ? 0.75 : 0.35, editing ? 0.35 : 0.40);
    cairo_set_line_width(cr, active ? 2.0 : 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
    cairo_move_to(cr, knob->edit_x + EDIT_PADDING, knob->edit_y + 15);
    cairo_show_text(cr, knob->edit_text);

    cairo_restore(cr);
}

static void draw_ui(ConvulseUI* ui) {
    cairo_t* cr = cairo_create(ui->surface);

    cairo_set_source_rgb(cr, 0.08, 0.09, 0.11);
    cairo_paint(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 13.0);
    cairo_set_source_rgb(cr, 0.95, 0.75, 0.35);
    cairo_move_to(cr, MARGIN, MARGIN + 10);
    cairo_show_text(cr, "CONVULSE SYNTH KERNEL CONVOLVER");

    for (int i = 0; i < ui->knob_count; ++i) {
        draw_knob(cr, &ui->knobs[i], ui->active_knob == i, ui->active_edit == i);
    }

    cairo_destroy(cr);
    cairo_surface_flush(ui->surface);
    XFlush(ui->display);
}

static void send_value(ConvulseUI* ui, const Knob* knob) {
    if (!ui->write) {
        return;
    }
    const float value = knob->value;
    ui->write(ui->controller, knob->port, sizeof(float), 0, &value);
}

static int knob_hit_test(const Knob* knob, int x, int y) {
    return x >= knob->x && x <= knob->x + knob->width &&
           y >= knob->y && y <= knob->y + knob->height;
}

static int edit_hit_test(const Knob* knob, int x, int y) {
    return x >= knob->edit_x && x <= knob->edit_x + knob->edit_w &&
           y >= knob->edit_y && y <= knob->edit_y + knob->edit_h;
}

static void handle_button_press(ConvulseUI* ui, XButtonEvent* ev) {
    pthread_mutex_lock(&ui->mutex);
    for (int i = 0; i < ui->knob_count; ++i) {
        Knob* knob = &ui->knobs[i];
        if (edit_hit_test(knob, ev->x, ev->y)) {
            ui->active_edit = i;
            update_edit_text(knob);
            ui->edit_cursor = strlen(knob->edit_text);
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
            return;
        }
        if (knob_hit_test(knob, ev->x, ev->y)) {
            ui->active_knob = i;
            ui->active_edit = -1;
            ui->drag_start_y = ev->y;
            ui->drag_start_value = knob->value;
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
            return;
        }
    }
    ui->active_edit = -1;
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_button_release(ConvulseUI* ui) {
    pthread_mutex_lock(&ui->mutex);
    ui->active_knob = -1;
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_motion(ConvulseUI* ui, XMotionEvent* ev) {
    pthread_mutex_lock(&ui->mutex);
    if (ui->active_knob < 0) {
        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    Knob* knob = &ui->knobs[ui->active_knob];
    const float delta = (float)(ui->drag_start_y - ev->y) / 140.0f;
    float value = ui->drag_start_value + delta * (knob->max - knob->min);
    value = clamp_value(knob, value);
    if (fabsf(value - knob->value) > 0.0001f) {
        knob->value = value;
        update_edit_text(knob);
        send_value(ui, knob);
        ui->needs_redraw = true;
    }

    pthread_mutex_unlock(&ui->mutex);
}

static void commit_edit(ConvulseUI* ui, Knob* knob) {
    float value = 0.0f;
    if (knob->port == PORT_MODE) {
        if (!parse_mode_name(knob->edit_text, &value)) {
            char* end = NULL;
            value = strtof(knob->edit_text, &end);
            if (end == knob->edit_text) {
                update_edit_text(knob);
                return;
            }
        }
    } else {
        char* end = NULL;
        value = strtof(knob->edit_text, &end);
        if (end == knob->edit_text) {
            update_edit_text(knob);
            return;
        }
    }
    knob->value = clamp_value(knob, value);
    update_edit_text(knob);
    send_value(ui, knob);
}

static void handle_key(ConvulseUI* ui, XKeyEvent* ev) {
    pthread_mutex_lock(&ui->mutex);
    if (ui->active_edit < 0) {
        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    Knob* knob = &ui->knobs[ui->active_edit];
    char buf[8] = {0};
    KeySym sym = 0;
    const int len = XLookupString(ev, buf, sizeof(buf) - 1, &sym, NULL);

    if (sym == XK_Return || sym == XK_KP_Enter) {
        commit_edit(ui, knob);
        ui->active_edit = -1;
        ui->needs_redraw = true;
        pthread_mutex_unlock(&ui->mutex);
        return;
    }
    if (sym == XK_Escape) {
        update_edit_text(knob);
        ui->active_edit = -1;
        ui->needs_redraw = true;
        pthread_mutex_unlock(&ui->mutex);
        return;
    }
    if (sym == XK_BackSpace) {
        size_t lenText = strlen(knob->edit_text);
        if (lenText > 0) {
            knob->edit_text[lenText - 1] = '\0';
            ui->needs_redraw = true;
        }
        pthread_mutex_unlock(&ui->mutex);
        return;
    }
    if (len <= 0) {
        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    if ((buf[0] >= '0' && buf[0] <= '9') || buf[0] == '.' || buf[0] == '-') {
        const size_t lenText = strlen(knob->edit_text);
        if (lenText + 1 < sizeof(knob->edit_text)) {
            knob->edit_text[lenText] = buf[0];
            knob->edit_text[lenText + 1] = '\0';
            ui->needs_redraw = true;
        }
    }
    pthread_mutex_unlock(&ui->mutex);
}

static void* event_thread_main(void* arg) {
    ConvulseUI* ui = (ConvulseUI*)arg;

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
                case KeyPress:
                    handle_key(ui, &ev.xkey);
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

static LV2UI_Handle ui_instantiate(const LV2UI_Descriptor*,
                                   const char*,
                                   const char*,
                                   LV2UI_Write_Function write_function,
                                   LV2UI_Controller controller,
                                   LV2UI_Widget* widget,
                                   const LV2_Feature* const* features) {
    ensure_xlib_threads();

    ConvulseUI* ui = (ConvulseUI*)calloc(1, sizeof(ConvulseUI));
    if (!ui) {
        return NULL;
    }

    ui->write = write_function;
    ui->controller = controller;
    ui->active_knob = -1;
    ui->active_edit = -1;
    ui->knob_count = (int)(sizeof(kControls) / sizeof(kControls[0]));

    ui->width = MARGIN * 2 + ui->knob_count * KNOB_DIAMETER + (ui->knob_count - 1) * KNOB_SPACING;
    ui->height = MARGIN * 2 + TITLE_HEIGHT + KNOB_HEIGHT + EDIT_HEIGHT + LABEL_HEIGHT + 10;

    void* parent = NULL;
    for (const LV2_Feature* const* f = features; f && *f; ++f) {
        if (!strcmp((*f)->URI, LV2_UI__parent)) {
            parent = (*f)->data;
        }
    }

    if (!parent) {
        fprintf(stderr, "convulse-ui: no parent window provided\n");
        free(ui);
        return NULL;
    }

    ui->display = XOpenDisplay(NULL);
    if (!ui->display) {
        free(ui);
        return NULL;
    }
    ui->screen = DefaultScreen(ui->display);

    XSetWindowAttributes attrs;
    attrs.background_pixel = BlackPixel(ui->display, ui->screen);
    attrs.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask |
                       ButtonReleaseMask | PointerMotionMask | KeyPressMask;

    ui->window = XCreateWindow(ui->display,
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
        XCloseDisplay(ui->display);
        free(ui);
        return NULL;
    }

    XStoreName(ui->display, ui->window, "Convulse");
    XMapWindow(ui->display, ui->window);

    ui->surface = cairo_xlib_surface_create(ui->display,
                                            ui->window,
                                            DefaultVisual(ui->display, ui->screen),
                                            ui->width,
                                            ui->height);
    cairo_xlib_surface_set_size(ui->surface, ui->width, ui->height);

    const int knob_y = MARGIN + TITLE_HEIGHT;
    for (int i = 0; i < ui->knob_count; ++i) {
        Knob* knob = &ui->knobs[i];
        knob->label = kControls[i].label;
        knob->port = kControls[i].port;
        knob->min = kControls[i].min;
        knob->max = kControls[i].max;
        knob->def = kControls[i].def;
        knob->value = kControls[i].def;
        knob->is_int = kControls[i].is_int;
        knob->width = KNOB_DIAMETER;
        knob->height = KNOB_HEIGHT;
        knob->x = MARGIN + i * (KNOB_DIAMETER + KNOB_SPACING);
        knob->y = knob_y;
        knob->edit_w = KNOB_DIAMETER;
        knob->edit_h = EDIT_HEIGHT;
        knob->edit_x = knob->x;
        knob->edit_y = knob->y + KNOB_HEIGHT + LABEL_HEIGHT;
        update_edit_text(knob);
    }

    XSetInputFocus(ui->display, ui->window, RevertToPointerRoot, CurrentTime);

    pthread_mutex_init(&ui->mutex, NULL);
    ui->running = true;
    ui->needs_redraw = true;

    if (pthread_create(&ui->thread, NULL, event_thread_main, ui) != 0) {
        cairo_surface_destroy(ui->surface);
        XDestroyWindow(ui->display, ui->window);
        XCloseDisplay(ui->display);
        pthread_mutex_destroy(&ui->mutex);
        free(ui);
        return NULL;
    }

    *widget = (LV2UI_Widget)(uintptr_t)ui->window;
    return ui;
}

static void ui_cleanup(LV2UI_Handle handle) {
    ConvulseUI* ui = (ConvulseUI*)handle;
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

static void ui_port_event(LV2UI_Handle handle,
                          uint32_t port_index,
                          uint32_t buffer_size,
                          uint32_t format,
                          const void* buffer) {
    ConvulseUI* ui = (ConvulseUI*)handle;
    if (!ui || !buffer || format != 0 || buffer_size < sizeof(float)) {
        return;
    }

    pthread_mutex_lock(&ui->mutex);
    for (int i = 0; i < ui->knob_count; ++i) {
        if (ui->knobs[i].port != port_index) {
            continue;
        }
        if (ui->active_edit == i) {
            pthread_mutex_unlock(&ui->mutex);
            return;
        }

        float value = *(const float*)buffer;
        value = clamp_value(&ui->knobs[i], value);
        if (fabsf(value - ui->knobs[i].value) > 0.0001f) {
            ui->knobs[i].value = value;
            update_edit_text(&ui->knobs[i]);
            ui->needs_redraw = true;
        }
        pthread_mutex_unlock(&ui->mutex);
        return;
    }
    pthread_mutex_unlock(&ui->mutex);
}

static const void* ui_extension_data(const char* uri) {
    (void)uri;
    return NULL;
}

static const LV2UI_Descriptor ui_descriptor = {
    CONVULSE_UI_URI,
    ui_instantiate,
    ui_cleanup,
    ui_port_event,
    ui_extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return (index == 0) ? &ui_descriptor : NULL;
}
