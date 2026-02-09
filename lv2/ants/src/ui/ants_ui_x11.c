#include <lv2/ui/ui.h>
#include <lv2/core/lv2.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
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

#define ANTS_URI "https://danja.github.io/flues/plugins/ants"
#define ANTS_UI_URI ANTS_URI "#ui"

#define MARGIN 18
#define TITLE_HEIGHT 20
#define KNOB_DIAMETER 54
#define KNOB_HEIGHT 80
#define EDIT_HEIGHT 22
#define KNOB_SPACING 14
#define EDIT_PADDING 6
#define LABEL_HEIGHT 14

typedef enum {
    PORT_ANTS = 2,
    PORT_VOICES = 3,
    PORT_SCALE = 4,
    PORT_ROOT = 5,
    PORT_STEPS = 6,
    PORT_SPEED = 7,
    PORT_RANDOM = 8,
    PORT_TRAIL = 9,
    PORT_DECAY = 10,
    PORT_NOTE_LEN = 11,
    PORT_GAP = 12,
    PORT_VELOCITY = 13,
    PORT_DENSITY = 14
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

    Knob knobs[13];
    int knob_count;

    volatile bool needs_redraw;
    int active_knob;
    double drag_start_y;
    float drag_start_value;

    int active_edit;
    size_t edit_cursor;
} AntsUI;

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
    { "ANTS", PORT_ANTS, 2.0f, 8.0f, 4.0f, true },
    { "VOICES", PORT_VOICES, 1.0f, 4.0f, 2.0f, true },
    { "SCALE", PORT_SCALE, 0.0f, 9.0f, 1.0f, true },
    { "ROOT", PORT_ROOT, 0.0f, 11.0f, 0.0f, true },
    { "STEPS", PORT_STEPS, 1.0f, 4.0f, 1.0f, true },
    { "SPEED", PORT_SPEED, 0.0f, 4.0f, 2.0f, true },
    { "RAND", PORT_RANDOM, 0.0f, 1.0f, 0.35f, false },
    { "TRAIL", PORT_TRAIL, 0.0f, 1.0f, 0.7f, false },
    { "DECAY", PORT_DECAY, 0.0f, 1.0f, 0.9f, false },
    { "LEN", PORT_NOTE_LEN, 0.1f, 1.0f, 0.5f, false },
    { "GAP", PORT_GAP, 0.0f, 4.0f, 0.0f, false },
    { "VEL", PORT_VELOCITY, 0.0f, 1.0f, 1.0f, false },
    { "DENS", PORT_DENSITY, 0.0f, 1.0f, 0.75f, false }
};

static float clamp_value(const Knob* knob, float value) {
    if (value < knob->min) return knob->min;
    if (value > knob->max) return knob->max;
    return value;
}

static void format_value(const Knob* knob, float value, char* out, size_t out_size) {
    if (knob->is_int) {
        snprintf(out, out_size, "%.0f", value);
    } else {
        snprintf(out, out_size, "%.2f", value);
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
    const double indicator_angle = end_angle;
    const double ix = center_x + cos(indicator_angle) * (radius - 8);
    const double iy = center_y + sin(indicator_angle) * (radius - 8);
    cairo_move_to(cr, center_x, center_y);
    cairo_line_to(cr, ix, iy);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.85, 0.85, 0.9);
    cairo_move_to(cr, x, y + diameter + LABEL_HEIGHT - 2);
    cairo_show_text(cr, knob->label);

    cairo_rectangle(cr, knob->edit_x, knob->edit_y, knob->edit_w, knob->edit_h);
    if (editing) {
        cairo_set_source_rgb(cr, 0.18, 0.22, 0.30);
    } else {
        cairo_set_source_rgb(cr, 0.12, 0.14, 0.18);
    }
    cairo_fill(cr);

    cairo_rectangle(cr, knob->edit_x, knob->edit_y, knob->edit_w, knob->edit_h);
    if (editing) {
        cairo_set_source_rgb(cr, 0.95, 0.75, 0.35);
    } else {
        cairo_set_source_rgb(cr, 0.35, 0.35, 0.40);
    }
    cairo_set_line_width(cr, active ? 2.0 : 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
    cairo_move_to(cr, knob->edit_x + EDIT_PADDING, knob->edit_y + 15);
    cairo_show_text(cr, knob->edit_text);

    cairo_restore(cr);
}

static void draw_ui(AntsUI* ui) {
    cairo_t* cr = cairo_create(ui->surface);

    cairo_set_source_rgb(cr, 0.08, 0.09, 0.11);
    cairo_paint(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 13.0);
    cairo_set_source_rgb(cr, 0.95, 0.75, 0.35);
    cairo_move_to(cr, MARGIN, MARGIN + 10);
    cairo_show_text(cr, "ANTS GENERATOR");

    for (int i = 0; i < ui->knob_count; ++i) {
        const bool active = (ui->active_knob == i);
        const bool editing = (ui->active_edit == i);
        draw_knob(cr, &ui->knobs[i], active, editing);
    }

    cairo_destroy(cr);
    cairo_surface_flush(ui->surface);
    XFlush(ui->display);
}

static void send_value(AntsUI* ui, const Knob* knob) {
    if (!ui->write) return;
    const float value = knob->value;
    ui->write(ui->controller, knob->port, sizeof(float), 0, &value);
}

static int knob_hit_test(const Knob* knob, int x, int y) {
    if (x >= knob->x && x <= knob->x + knob->width &&
        y >= knob->y && y <= knob->y + knob->height) {
        return 1;
    }
    return 0;
}

static int edit_hit_test(const Knob* knob, int x, int y) {
    if (x >= knob->edit_x && x <= knob->edit_x + knob->edit_w &&
        y >= knob->edit_y && y <= knob->edit_y + knob->edit_h) {
        return 1;
    }
    return 0;
}

static void handle_button_press(AntsUI* ui, XButtonEvent* ev) {
    const int x = ev->x;
    const int y = ev->y;

    pthread_mutex_lock(&ui->mutex);
    for (int i = 0; i < ui->knob_count; ++i) {
        Knob* knob = &ui->knobs[i];
        if (edit_hit_test(knob, x, y)) {
            ui->active_edit = i;
            update_edit_text(knob);
            ui->edit_cursor = strlen(knob->edit_text);
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
            return;
        }
        if (knob_hit_test(knob, x, y)) {
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

static void handle_button_release(AntsUI* ui) {
    pthread_mutex_lock(&ui->mutex);
    ui->active_knob = -1;
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_motion(AntsUI* ui, XMotionEvent* ev) {
    pthread_mutex_lock(&ui->mutex);
    if (ui->active_knob < 0) {
        pthread_mutex_unlock(&ui->mutex);
        return;
    }
    Knob* knob = &ui->knobs[ui->active_knob];
    const float range = knob->max - knob->min;
    const float delta = (float)(ui->drag_start_y - ev->y) / 140.0f;
    float value = ui->drag_start_value + delta * range;
    value = clamp_value(knob, value);
    if (fabsf(value - knob->value) > 0.0001f) {
        knob->value = value;
        update_edit_text(knob);
        send_value(ui, knob);
        ui->needs_redraw = true;
    }
    pthread_mutex_unlock(&ui->mutex);
}

static void commit_edit(AntsUI* ui, Knob* knob) {
    if (!knob) return;
    char* end = NULL;
    float value = strtof(knob->edit_text, &end);
    if (end == knob->edit_text) {
        update_edit_text(knob);
        return;
    }
    value = clamp_value(knob, value);
    knob->value = value;
    update_edit_text(knob);
    send_value(ui, knob);
}

static void handle_key(AntsUI* ui, XKeyEvent* ev) {
    pthread_mutex_lock(&ui->mutex);
    if (ui->active_edit < 0) {
        pthread_mutex_unlock(&ui->mutex);
        return;
    }
    Knob* knob = &ui->knobs[ui->active_edit];

    char buf[8] = {0};
    KeySym sym = 0;
    int len = XLookupString(ev, buf, sizeof(buf) - 1, &sym, NULL);

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
        size_t len_text = strlen(knob->edit_text);
        if (len_text > 0) {
            knob->edit_text[len_text - 1] = '\0';
            ui->needs_redraw = true;
        }
        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    if (len <= 0) {
        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    const char c = buf[0];
    if ((c >= '0' && c <= '9') || c == '.' || c == '-') {
        size_t len_text = strlen(knob->edit_text);
        if (len_text + 1 < sizeof(knob->edit_text)) {
            knob->edit_text[len_text] = c;
            knob->edit_text[len_text + 1] = '\0';
            ui->needs_redraw = true;
        }
    }
    pthread_mutex_unlock(&ui->mutex);
}

static void* event_thread_main(void* arg) {
    AntsUI* ui = (AntsUI*)arg;

    while (ui->running) {
        while (XPending(ui->display)) {
            XEvent ev;
            XNextEvent(ui->display, &ev);
            switch (ev.type) {
                case Expose:
                    ui->needs_redraw = true;
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

static LV2UI_Handle ui_instantiate(const LV2UI_Descriptor* /*descriptor*/,
                                   const char* /*plugin_uri*/,
                                   const char* /*bundle_path*/,
                                   LV2UI_Write_Function write_function,
                                   LV2UI_Controller controller,
                                   LV2UI_Widget* widget,
                                   const LV2_Feature* const* features) {
    ensure_xlib_threads();

    AntsUI* ui = (AntsUI*)calloc(1, sizeof(AntsUI));
    if (!ui) return NULL;

    ui->write = write_function;
    ui->controller = controller;
    ui->active_knob = -1;
    ui->active_edit = -1;

    const int control_count = (int)(sizeof(kControls) / sizeof(kControls[0]));
    ui->knob_count = control_count;

    int width = MARGIN * 2 + control_count * KNOB_DIAMETER + (control_count - 1) * KNOB_SPACING;
    int height = MARGIN * 2 + TITLE_HEIGHT + KNOB_HEIGHT + EDIT_HEIGHT + LABEL_HEIGHT + 10;

    ui->width = width;
    ui->height = height;

    void* parent = NULL;
    for (const LV2_Feature* const* f = features; f && *f; ++f) {
        if (!strcmp((*f)->URI, LV2_UI__parent)) {
            parent = (*f)->data;
        }
    }

    if (!parent) {
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
    attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                       PointerMotionMask | KeyPressMask;

    ui->window = XCreateWindow(
        display,
        (Window)(uintptr_t)parent,
        0,
        0,
        width,
        height,
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

    XStoreName(display, ui->window, "Ants");
    XMapWindow(display, ui->window);

    ui->surface = cairo_xlib_surface_create(display, ui->window,
                                            DefaultVisual(display, ui->screen),
                                            width, height);

    const int knob_y = MARGIN + TITLE_HEIGHT;
    for (int i = 0; i < control_count; ++i) {
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
    AntsUI* ui = (AntsUI*)handle;
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
    AntsUI* ui = (AntsUI*)handle;
    if (!ui || !buffer || format != 0 || buffer_size < sizeof(float)) return;

    pthread_mutex_lock(&ui->mutex);
    for (int i = 0; i < ui->knob_count; ++i) {
        if (ui->knobs[i].port != port_index) continue;
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
    ANTS_UI_URI,
    ui_instantiate,
    ui_cleanup,
    ui_port_event,
    ui_extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return (index == 0) ? &ui_descriptor : NULL;
}
