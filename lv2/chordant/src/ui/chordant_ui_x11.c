#include <lv2/core/lv2.h>
#include <lv2/ui/ui.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHORDANT_URI "https://danja.github.io/flues/plugins/chordant"
#define CHORDANT_UI_URI CHORDANT_URI "#ui"

#define MARGIN 18
#define TITLE_HEIGHT 20
#define KNOB_DIAMETER 54
#define KNOB_HEIGHT 80
#define EDIT_HEIGHT 22
#define LABEL_HEIGHT 14
#define KNOB_SPACING 14
#define ROW_GAP 20
#define EDIT_PADDING 6

typedef enum {
    PORT_CONTROL = 0,
    PORT_AUDIO_IN_L,
    PORT_AUDIO_IN_R,
    PORT_AUDIO_OUT_L,
    PORT_AUDIO_OUT_R,
    PORT_TOTAL_BARS,
    PORT_DIVISION,
    PORT_STEPS,
    PORT_OFFSET,
    PORT_FADE,
    PORT_MAX_SEGMENTS,
    PORT_NOCAP_PASSTHROUGH,
    PORT_CLEAR_TRIGGER,
    PORT_CAPTURE_MODE,
    PORT_DRY_WET,
    PORT_ATTACK_MS,
    PORT_DECAY_MS,
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
    float value;
    bool is_int;
    int x;
    int y;
    int w;
    int h;
    int edit_x;
    int edit_y;
    int edit_w;
    int edit_h;
    char edit_text[32];
} Knob;

typedef struct {
    uint32_t port;
    const char* label;
    float value;
    int x;
    int y;
    int size;
} Toggle;

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

    Knob knobs[10];
    int knob_count;
    Toggle toggles[2];
    int toggle_count;

    volatile bool needs_redraw;
    int active_knob;
    double drag_start_y;
    float drag_start_value;
    int active_edit;
} ChordantUI;

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
    { "TBAR", PORT_TOTAL_BARS, 0.125f, 8.0f, 2.0f, false },
    { "DIV", PORT_DIVISION, 1.0f, 16.0f, 8.0f, true },
    { "STEP", PORT_STEPS, 0.0f, 16.0f, 2.0f, true },
    { "OFFS", PORT_OFFSET, 0.0f, 15.0f, 0.0f, true },
    { "FADE", PORT_FADE, 0.0f, 1.0f, 0.0f, false },
    { "SEGS", PORT_MAX_SEGMENTS, 1.0f, 12.0f, 8.0f, true },
    { "CAP", PORT_CAPTURE_MODE, 0.0f, 3.0f, 0.0f, true },
    { "D/W", PORT_DRY_WET, 0.0f, 100.0f, 100.0f, true },
    { "ATT", PORT_ATTACK_MS, 1.0f, 200.0f, 8.0f, true },
    { "DEC", PORT_DECAY_MS, 10.0f, 800.0f, 180.0f, true }
};

static float clampf_knob(const Knob* knob, float value) {
    if (value < knob->min) return knob->min;
    if (value > knob->max) return knob->max;
    return value;
}

static void format_value(const Knob* knob, char* out, size_t out_size) {
    if (knob->is_int) {
        snprintf(out, out_size, "%.0f", roundf(knob->value));
    } else {
        snprintf(out, out_size, "%.3f", knob->value);
    }
}

static void update_edit_text(Knob* knob) {
    format_value(knob, knob->edit_text, sizeof(knob->edit_text));
}

static void send_knob_value(ChordantUI* ui, const Knob* knob) {
    if (!ui->write) return;
    const float value = knob->value;
    ui->write(ui->controller, knob->port, sizeof(float), 0, &value);
}

static void send_toggle_value(ChordantUI* ui, const Toggle* toggle) {
    if (!ui->write) return;
    const float value = (toggle->value >= 0.5f) ? 1.0f : 0.0f;
    ui->write(ui->controller, toggle->port, sizeof(float), 0, &value);
}

static void draw_knob(cairo_t* cr, const Knob* knob, bool active, bool editing) {
    const double x = knob->x;
    const double y = knob->y;
    const double radius = KNOB_DIAMETER * 0.5;
    const double cx = x + radius;
    const double cy = y + radius;

    cairo_set_source_rgb(cr, 0.12, 0.13, 0.16);
    cairo_rectangle(cr, x - 6, y - 6, KNOB_DIAMETER + 12, KNOB_HEIGHT + EDIT_HEIGHT + LABEL_HEIGHT + 12);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.22, 0.23, 0.26);
    cairo_arc(cr, cx, cy, radius, 0, 2 * M_PI);
    cairo_fill(cr);

    const double t = (knob->value - knob->min) / (knob->max - knob->min);
    const double start = -0.75 * M_PI;
    const double end = start + (1.5 * M_PI * t);
    cairo_set_line_width(cr, 4.0);
    cairo_set_source_rgb(cr, 0.95, 0.75, 0.35);
    cairo_arc(cr, cx, cy, radius - 5.0, start, end);
    cairo_stroke(cr);

    cairo_set_line_width(cr, 3.0);
    cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
    cairo_move_to(cr, cx, cy);
    cairo_line_to(cr, cx + cos(end) * (radius - 8.0), cy + sin(end) * (radius - 8.0));
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.84, 0.84, 0.90);
    cairo_move_to(cr, x, y + KNOB_DIAMETER + LABEL_HEIGHT - 2);
    cairo_show_text(cr, knob->label);

    cairo_rectangle(cr, knob->edit_x, knob->edit_y, knob->edit_w, knob->edit_h);
    cairo_set_source_rgb(cr, editing ? 0.18 : 0.12, editing ? 0.22 : 0.14, editing ? 0.30 : 0.18);
    cairo_fill_preserve(cr);

    cairo_set_line_width(cr, active ? 2.0 : 1.0);
    cairo_set_source_rgb(cr, editing ? 0.95 : 0.35, editing ? 0.75 : 0.35, editing ? 0.35 : 0.40);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
    cairo_move_to(cr, knob->edit_x + EDIT_PADDING, knob->edit_y + 15);
    cairo_show_text(cr, knob->edit_text);
}

static void draw_toggle(cairo_t* cr, const Toggle* toggle) {
    cairo_set_source_rgb(cr, 0.12, 0.13, 0.16);
    cairo_rectangle(cr, toggle->x - 10, toggle->y - 8, 122, 44);
    cairo_fill(cr);

    cairo_rectangle(cr, toggle->x, toggle->y, toggle->size, toggle->size);
    cairo_set_source_rgb(cr, 0.10, 0.11, 0.14);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.45, 0.45, 0.5);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    if (toggle->value >= 0.5f) {
        cairo_set_source_rgb(cr, 0.25, 0.88, 0.42);
        cairo_rectangle(cr, toggle->x + 3, toggle->y + 3, toggle->size - 6, toggle->size - 6);
        cairo_fill(cr);
    }

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.84, 0.84, 0.90);
    cairo_move_to(cr, toggle->x + toggle->size + 8, toggle->y + 14);
    cairo_show_text(cr, toggle->label);
}

static void draw_ui(ChordantUI* ui) {
    cairo_t* cr = cairo_create(ui->surface);
    cairo_set_source_rgb(cr, 0.08, 0.09, 0.11);
    cairo_paint(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 13.0);
    cairo_set_source_rgb(cr, 0.95, 0.75, 0.35);
    cairo_move_to(cr, MARGIN, MARGIN + 10);
    cairo_show_text(cr, "CHORDANT CAPTURE MIXER");

    for (int i = 0; i < ui->knob_count; ++i) {
        draw_knob(cr, &ui->knobs[i], ui->active_knob == i, ui->active_edit == i);
    }
    for (int i = 0; i < ui->toggle_count; ++i) {
        draw_toggle(cr, &ui->toggles[i]);
    }

    cairo_destroy(cr);
    cairo_surface_flush(ui->surface);
    XFlush(ui->display);
}

static bool point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x <= (rx + rw) && y >= ry && y <= (ry + rh);
}

static void commit_edit(ChordantUI* ui, Knob* knob) {
    if (!knob) return;
    char* end = NULL;
    float value = strtof(knob->edit_text, &end);
    if (end == knob->edit_text) {
        update_edit_text(knob);
        return;
    }
    value = clampf_knob(knob, value);
    knob->value = knob->is_int ? roundf(value) : value;
    update_edit_text(knob);
    send_knob_value(ui, knob);
}

static void handle_button_press(ChordantUI* ui, XButtonEvent* ev) {
    pthread_mutex_lock(&ui->mutex);

    for (int i = 0; i < ui->toggle_count; ++i) {
        Toggle* t = &ui->toggles[i];
        if (point_in_rect(ev->x, ev->y, t->x, t->y, t->size, t->size)) {
            t->value = (t->value >= 0.5f) ? 0.0f : 1.0f;
            send_toggle_value(ui, t);
            ui->active_edit = -1;
            ui->active_knob = -1;
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
            return;
        }
    }

    for (int i = 0; i < ui->knob_count; ++i) {
        Knob* k = &ui->knobs[i];
        if (point_in_rect(ev->x, ev->y, k->edit_x, k->edit_y, k->edit_w, k->edit_h)) {
            ui->active_edit = i;
            ui->active_knob = -1;
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
            return;
        }
        if (point_in_rect(ev->x, ev->y, k->x, k->y, k->w, k->h)) {
            ui->active_knob = i;
            ui->active_edit = -1;
            ui->drag_start_y = ev->y;
            ui->drag_start_value = k->value;
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
            return;
        }
    }

    if (ui->active_edit >= 0 && ui->active_edit < ui->knob_count) {
        commit_edit(ui, &ui->knobs[ui->active_edit]);
    }
    ui->active_edit = -1;
    ui->active_knob = -1;
    ui->needs_redraw = true;
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_button_release(ChordantUI* ui) {
    pthread_mutex_lock(&ui->mutex);
    ui->active_knob = -1;
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_motion(ChordantUI* ui, XMotionEvent* ev) {
    pthread_mutex_lock(&ui->mutex);
    if (ui->active_knob < 0) {
        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    Knob* k = &ui->knobs[ui->active_knob];
    const float range = k->max - k->min;
    float value = ui->drag_start_value + (float)(ui->drag_start_y - ev->y) * (range / 140.0f);
    value = clampf_knob(k, value);
    if (k->is_int) {
        value = roundf(value);
    }
    if (fabsf(value - k->value) > 0.0001f) {
        k->value = value;
        update_edit_text(k);
        send_knob_value(ui, k);
        ui->needs_redraw = true;
    }

    pthread_mutex_unlock(&ui->mutex);
}

static void handle_key(ChordantUI* ui, XKeyEvent* ev) {
    pthread_mutex_lock(&ui->mutex);
    if (ui->active_edit < 0 || ui->active_edit >= ui->knob_count) {
        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    Knob* k = &ui->knobs[ui->active_edit];

    char buf[8] = {0};
    KeySym sym = 0;
    int len = XLookupString(ev, buf, sizeof(buf) - 1, &sym, NULL);

    if (sym == XK_Return || sym == XK_KP_Enter) {
        commit_edit(ui, k);
        ui->active_edit = -1;
        ui->needs_redraw = true;
        pthread_mutex_unlock(&ui->mutex);
        return;
    }
    if (sym == XK_Escape) {
        update_edit_text(k);
        ui->active_edit = -1;
        ui->needs_redraw = true;
        pthread_mutex_unlock(&ui->mutex);
        return;
    }
    if (sym == XK_BackSpace) {
        size_t n = strlen(k->edit_text);
        if (n > 0) {
            k->edit_text[n - 1] = '\0';
            ui->needs_redraw = true;
        }
        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    if (len > 0) {
        const char c = buf[0];
        if ((c >= '0' && c <= '9') || c == '.' || c == '-') {
            size_t n = strlen(k->edit_text);
            if (n + 1 < sizeof(k->edit_text)) {
                k->edit_text[n] = c;
                k->edit_text[n + 1] = '\0';
                ui->needs_redraw = true;
            }
        }
    }

    pthread_mutex_unlock(&ui->mutex);
}

static void* event_thread_main(void* arg) {
    ChordantUI* ui = (ChordantUI*)arg;

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
                case DestroyNotify:
                    ui->running = false;
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

    ChordantUI* ui = (ChordantUI*)calloc(1, sizeof(ChordantUI));
    if (!ui) {
        return NULL;
    }

    ui->write = write_function;
    ui->controller = controller;
    ui->active_knob = -1;
    ui->active_edit = -1;
    ui->knob_count = (int)(sizeof(kControls) / sizeof(kControls[0]));
    ui->toggle_count = 2;

    const int row_block_h = KNOB_HEIGHT + EDIT_HEIGHT + LABEL_HEIGHT;
    const int knob_row_w = 5 * KNOB_DIAMETER + 4 * KNOB_SPACING;
    ui->width = MARGIN * 2 + knob_row_w + 140;
    ui->height = MARGIN * 2 + TITLE_HEIGHT + row_block_h * 2 + ROW_GAP + 10;

    void* parent = NULL;
    for (const LV2_Feature* const* f = features; f && *f; ++f) {
        if (!strcmp((*f)->URI, LV2_UI__parent)) {
            parent = (*f)->data;
        }
    }

    if (!parent) {
        fprintf(stderr, "chordant-ui: no parent window provided\n");
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

    ui->window = XCreateWindow(
        ui->display,
        (Window)(uintptr_t)parent,
        0,
        0,
        (unsigned int)ui->width,
        (unsigned int)ui->height,
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

    XStoreName(ui->display, ui->window, "Chordant");
    XMapWindow(ui->display, ui->window);
    XSetInputFocus(ui->display, ui->window, RevertToPointerRoot, CurrentTime);

    ui->surface = cairo_xlib_surface_create(
        ui->display,
        ui->window,
        DefaultVisual(ui->display, ui->screen),
        ui->width,
        ui->height);
    cairo_xlib_surface_set_size(ui->surface, ui->width, ui->height);

    const int row1_y = MARGIN + TITLE_HEIGHT;
    const int row2_y = row1_y + row_block_h + ROW_GAP;

    for (int i = 0; i < ui->knob_count; ++i) {
        Knob* k = &ui->knobs[i];
        const int col = i % 5;
        const int row = (i < 5) ? 0 : 1;
        const int y = (row == 0) ? row1_y : row2_y;

        k->label = kControls[i].label;
        k->port = kControls[i].port;
        k->min = kControls[i].min;
        k->max = kControls[i].max;
        k->value = kControls[i].def;
        k->is_int = kControls[i].is_int;

        k->x = MARGIN + col * (KNOB_DIAMETER + KNOB_SPACING);
        k->y = y;
        k->w = KNOB_DIAMETER;
        k->h = KNOB_HEIGHT;
        k->edit_x = k->x;
        k->edit_y = y + KNOB_HEIGHT + LABEL_HEIGHT;
        k->edit_w = KNOB_DIAMETER;
        k->edit_h = EDIT_HEIGHT;
        update_edit_text(k);
    }

    ui->toggles[0].port = PORT_NOCAP_PASSTHROUGH;
    ui->toggles[0].label = "NOCAP";
    ui->toggles[0].value = 0.0f;
    ui->toggles[0].size = 18;
    ui->toggles[0].x = MARGIN + knob_row_w + 28;
    ui->toggles[0].y = row1_y + 24;

    ui->toggles[1].port = PORT_CLEAR_TRIGGER;
    ui->toggles[1].label = "CLEAR";
    ui->toggles[1].value = 1.0f;
    ui->toggles[1].size = 18;
    ui->toggles[1].x = MARGIN + knob_row_w + 28;
    ui->toggles[1].y = row2_y + 24;

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
    ChordantUI* ui = (ChordantUI*)handle;
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
    ChordantUI* ui = (ChordantUI*)handle;
    if (!ui || !buffer || format != 0 || buffer_size < sizeof(float)) {
        return;
    }

    const float incoming = *(const float*)buffer;
    pthread_mutex_lock(&ui->mutex);

    for (int i = 0; i < ui->knob_count; ++i) {
        Knob* k = &ui->knobs[i];
        if (k->port != port_index) {
            continue;
        }

        if (ui->active_edit == i) {
            pthread_mutex_unlock(&ui->mutex);
            return;
        }

        float value = clampf_knob(k, incoming);
        if (k->is_int) {
            value = roundf(value);
        }

        if (fabsf(value - k->value) > 0.0001f) {
            k->value = value;
            update_edit_text(k);
            ui->needs_redraw = true;
        }

        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    for (int i = 0; i < ui->toggle_count; ++i) {
        Toggle* t = &ui->toggles[i];
        if (t->port != port_index) {
            continue;
        }
        const float value = (incoming >= 0.5f) ? 1.0f : 0.0f;
        if (fabsf(value - t->value) > 0.0001f) {
            t->value = value;
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
    CHORDANT_UI_URI,
    ui_instantiate,
    ui_cleanup,
    ui_port_event,
    ui_extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return (index == 0) ? &ui_descriptor : NULL;
}
