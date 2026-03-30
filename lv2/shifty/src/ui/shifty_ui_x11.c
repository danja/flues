#include <lv2/core/lv2.h>
#include <lv2/ui/ui.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/cursorfont.h>
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

#define SHIFTY_URI "https://danja.github.io/flues/plugins/shifty"
#define SHIFTY_UI_URI SHIFTY_URI "#ui"

enum PortIndex {
    PORT_CONTROL = 0,
    PORT_IN_L,
    PORT_IN_R,
    PORT_OUT_L,
    PORT_OUT_R,
    PORT_BLOCK_BARS,
    PORT_DIVISION_COUNT,
    PORT_MIX,
    PORT_SMOOTH_MS,
    PORT_SHIFT_1,
    PORT_SHIFT_16 = PORT_SHIFT_1 + 15,
    PORT_ACTIVE_DIVISION,
    PORT_ACTIVE_SHIFT
};

typedef struct {
    const char* label;
    uint32_t port;
    float min;
    float max;
    float value;
    bool is_int;
    int x;
    int y;
    int w;
    int h;
    char text[24];
} EditField;

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
    volatile bool needs_redraw;

    int width;
    int height;

    EditField globals[4];
    EditField shifts[16];
    int active_field;
    size_t cursor;
    int active_division;
    int active_shift;
} ShiftyUI;

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

static float clampf_local(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int clampi_local(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void notify_host(ShiftyUI* ui, uint32_t port, float value) {
    if (ui->write) {
        ui->write(ui->controller, port, sizeof(float), 0, &value);
    }
}

static void field_set_value(EditField* field, float value) {
    field->value = clampf_local(value, field->min, field->max);
    if (field->is_int) {
        snprintf(field->text, sizeof(field->text), "%d", (int)lroundf(field->value));
    } else {
        snprintf(field->text, sizeof(field->text), "%.2f", field->value);
    }
}

static bool point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}

static EditField* field_at(ShiftyUI* ui, int x, int y, int* out_index) {
    for (int i = 0; i < 4; ++i) {
        if (point_in_rect(x, y, ui->globals[i].x, ui->globals[i].y, ui->globals[i].w, ui->globals[i].h)) {
            if (out_index) *out_index = i;
            return &ui->globals[i];
        }
    }
    const int visible = clampi_local((int)lroundf(ui->globals[1].value), 1, 16);
    for (int i = 0; i < visible; ++i) {
        if (point_in_rect(x, y, ui->shifts[i].x, ui->shifts[i].y, ui->shifts[i].w, ui->shifts[i].h)) {
            if (out_index) *out_index = 4 + i;
            return &ui->shifts[i];
        }
    }
    return NULL;
}

static void draw_field(cairo_t* cr, const EditField* field, bool active, bool playing_active) {
    if (playing_active) {
        cairo_set_source_rgb(cr, 0.20, 0.30, 0.18);
    } else {
        cairo_set_source_rgb(cr, 0.14, 0.15, 0.18);
    }
    cairo_rectangle(cr, field->x, field->y, field->w, field->h);
    cairo_fill(cr);

    cairo_set_line_width(cr, active ? 2.0 : 1.0);
    if (active) {
        cairo_set_source_rgb(cr, 0.95, 0.76, 0.36);
    } else if (playing_active) {
        cairo_set_source_rgb(cr, 0.58, 0.80, 0.44);
    } else {
        cairo_set_source_rgb(cr, 0.34, 0.36, 0.40);
    }
    cairo_rectangle(cr, field->x, field->y, field->w, field->h);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.85, 0.85, 0.90);
    cairo_move_to(cr, field->x, field->y - 4);
    cairo_show_text(cr, field->label);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.0);
    cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
    cairo_move_to(cr, field->x + 8, field->y + 17);
    cairo_show_text(cr, field->text);
}

static void draw_ui(ShiftyUI* ui) {
    cairo_t* cr = cairo_create(ui->surface);
    cairo_set_source_rgb(cr, 0.08, 0.09, 0.11);
    cairo_paint(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14.0);
    cairo_set_source_rgb(cr, 0.95, 0.76, 0.36);
    cairo_move_to(cr, 18, 24);
    cairo_show_text(cr, "SHIFTY");

    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.80, 0.82, 0.86);
    cairo_move_to(cr, 18, 40);
    cairo_show_text(cr, "Transport-synchronised pitch-shift pattern scaffold");

    for (int i = 0; i < 4; ++i) {
        draw_field(cr, &ui->globals[i], ui->active_field == i, false);
    }

    const int visible = clampi_local((int)lroundf(ui->globals[1].value), 1, 16);
    for (int i = 0; i < visible; ++i) {
        draw_field(cr, &ui->shifts[i], ui->active_field == (4 + i), ui->active_division == i);
    }

    char status[128];
    snprintf(status, sizeof(status), "Active division: %d   Active shift: %d st",
             ui->active_division >= 0 ? ui->active_division + 1 : 0, ui->active_shift);
    cairo_set_source_rgb(cr, 0.86, 0.86, 0.86);
    cairo_move_to(cr, 18, ui->height - 18);
    cairo_show_text(cr, status);

    cairo_destroy(cr);
}

static void commit_field(ShiftyUI* ui, EditField* field) {
    char* end = NULL;
    const float parsed = strtof(field->text, &end);
    float value = (end == field->text) ? 0.0f : parsed;
    value = clampf_local(value, field->min, field->max);
    if (field->is_int) {
        value = floorf(value + 0.5f);
    }
    field_set_value(field, value);
    notify_host(ui, field->port, value);
    ui->needs_redraw = true;
}

static void setup_layout(ShiftyUI* ui) {
    ui->width = 760;
    ui->height = 300;
    ui->active_field = -1;
    ui->active_division = -1;
    ui->active_shift = 0;

    ui->globals[0] = (EditField){ "BLOCK BARS", PORT_BLOCK_BARS, 1.0f, 8.0f, 2.0f, true, 18, 70, 110, 26, "" };
    ui->globals[1] = (EditField){ "DIVISIONS", PORT_DIVISION_COUNT, 1.0f, 16.0f, 8.0f, true, 146, 70, 110, 26, "" };
    ui->globals[2] = (EditField){ "MIX", PORT_MIX, 0.0f, 1.0f, 1.0f, false, 274, 70, 110, 26, "" };
    ui->globals[3] = (EditField){ "SMOOTH MS", PORT_SMOOTH_MS, 0.0f, 250.0f, 30.0f, true, 402, 70, 110, 26, "" };

    for (int i = 0; i < 4; ++i) {
        field_set_value(&ui->globals[i], ui->globals[i].value);
    }
    for (int i = 0; i < 16; ++i) {
        const int row = i / 8;
        const int col = i % 8;
        char* label = (char*)calloc(12, 1);
        snprintf(label, 12, "SHIFT %d", i + 1);
        ui->shifts[i] = (EditField){ label, (uint32_t)(PORT_SHIFT_1 + i), -24.0f, 24.0f, 0.0f, true,
                                     18 + col * 90, 140 + row * 58, 74, 26, "" };
        field_set_value(&ui->shifts[i], 0.0f);
    }
}

static void handle_key(ShiftyUI* ui, XKeyEvent* ev) {
    if (ui->active_field < 0) {
        return;
    }
    EditField* field = (ui->active_field < 4) ? &ui->globals[ui->active_field] : &ui->shifts[ui->active_field - 4];

    KeySym keysym = NoSymbol;
    char buf[8] = {0};
    const int len = XLookupString(ev, buf, sizeof(buf), &keysym, NULL);

    if (keysym == XK_Return || keysym == XK_KP_Enter) {
        commit_field(ui, field);
        return;
    }
    if (keysym == XK_Escape) {
        field_set_value(field, field->value);
        ui->needs_redraw = true;
        return;
    }
    if (keysym == XK_BackSpace) {
        const size_t l = strlen(field->text);
        if (l > 0) {
            field->text[l - 1] = '\0';
            ui->needs_redraw = true;
        }
        return;
    }
    if (len == 1) {
        const char c = buf[0];
        const bool ok = (c >= '0' && c <= '9') || c == '-' || c == '.';
        if (ok && strlen(field->text) + 1 < sizeof(field->text)) {
            const size_t l = strlen(field->text);
            field->text[l] = c;
            field->text[l + 1] = '\0';
            ui->needs_redraw = true;
        }
    }
}

static void handle_press(ShiftyUI* ui, XButtonEvent* ev) {
    if (ev->button != Button1) {
        return;
    }
    int index = -1;
    EditField* field = field_at(ui, ev->x, ev->y, &index);
    if (field) {
        ui->active_field = index;
        ui->needs_redraw = true;
    } else {
        ui->active_field = -1;
        ui->needs_redraw = true;
    }
}

static void* event_thread_main(void* arg) {
    ShiftyUI* ui = (ShiftyUI*)arg;
    while (ui->running) {
        while (XPending(ui->display)) {
            XEvent ev;
            XNextEvent(ui->display, &ev);
            pthread_mutex_lock(&ui->mutex);
            switch (ev.type) {
                case Expose:
                case ConfigureNotify:
                    ui->needs_redraw = true;
                    break;
                case ButtonPress:
                    handle_press(ui, &ev.xbutton);
                    break;
                case KeyPress:
                    handle_key(ui, &ev.xkey);
                    break;
                default:
                    break;
            }
            pthread_mutex_unlock(&ui->mutex);
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

static LV2UI_Handle ui_instantiate(const LV2UI_Descriptor*, const char*, const char*,
                                   LV2UI_Write_Function write_function, LV2UI_Controller controller,
                                   LV2UI_Widget* widget, const LV2_Feature* const* features) {
    ensure_xlib_threads();
    ShiftyUI* ui = (ShiftyUI*)calloc(1, sizeof(ShiftyUI));
    if (!ui) {
        return NULL;
    }

    pthread_mutex_init(&ui->mutex, NULL);
    ui->write = write_function;
    ui->controller = controller;
    setup_layout(ui);

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

    XSetWindowAttributes attrs;
    attrs.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask | KeyPressMask;
    attrs.background_pixel = BlackPixel(ui->display, ui->screen);

    ui->window = XCreateWindow(ui->display, parent, 0, 0, ui->width, ui->height, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWBackPixel | CWEventMask, &attrs);
    Cursor hand = XCreateFontCursor(ui->display, XC_hand2);
    XDefineCursor(ui->display, ui->window, hand);
    XMapWindow(ui->display, ui->window);
    XSetInputFocus(ui->display, ui->window, RevertToParent, CurrentTime);

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
    ShiftyUI* ui = (ShiftyUI*)handle;
    if (!ui) {
        return;
    }
    ui->running = false;
    pthread_join(ui->thread, NULL);
    for (int i = 0; i < 16; ++i) {
        free((void*)ui->shifts[i].label);
    }
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

static void ui_port_event(LV2UI_Handle handle, uint32_t port_index, uint32_t buffer_size, uint32_t format, const void* buffer) {
    ShiftyUI* ui = (ShiftyUI*)handle;
    if (!ui || !buffer || format != 0) {
        return;
    }
    (void)buffer_size;
    const float value = *(const float*)buffer;

    pthread_mutex_lock(&ui->mutex);
    if (port_index >= PORT_BLOCK_BARS && port_index <= PORT_SMOOTH_MS) {
        field_set_value(&ui->globals[port_index - PORT_BLOCK_BARS], value);
    } else if (port_index >= PORT_SHIFT_1 && port_index <= PORT_SHIFT_16) {
        field_set_value(&ui->shifts[port_index - PORT_SHIFT_1], value);
    } else if (port_index == PORT_ACTIVE_DIVISION) {
        ui->active_division = (int)lroundf(value);
    } else if (port_index == PORT_ACTIVE_SHIFT) {
        ui->active_shift = (int)lroundf(value);
    }
    ui->needs_redraw = true;
    pthread_mutex_unlock(&ui->mutex);
}

static const LV2UI_Descriptor descriptor = {
    SHIFTY_UI_URI,
    ui_instantiate,
    ui_cleanup,
    ui_port_event,
    NULL
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : NULL;
}
