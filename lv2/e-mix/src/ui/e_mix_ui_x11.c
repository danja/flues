#include <lv2/core/lv2.h>
#include <lv2/ui/ui.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include <pthread.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define EMIX_URI "https://danja.github.io/flues/plugins/e-mix"
#define EMIX_UI_URI EMIX_URI "#ui"

#define MARGIN 18
#define TITLE_HEIGHT 24
#define FIELD_HEIGHT 40
#define FIELD_GAP 10
#define FIELD_LABEL_HEIGHT 14
#define VALUE_PADDING 8

typedef enum {
    PORT_CONTROL = 0,
    PORT_AUDIO_IN_1,
    PORT_AUDIO_IN_2,
    PORT_AUDIO_IN_3,
    PORT_AUDIO_IN_4,
    PORT_AUDIO_IN_5,
    PORT_AUDIO_IN_6,
    PORT_AUDIO_IN_7,
    PORT_AUDIO_IN_8,
    PORT_AUDIO_OUT_1,
    PORT_AUDIO_OUT_2,
    PORT_AUDIO_OUT_3,
    PORT_AUDIO_OUT_4,
    PORT_AUDIO_OUT_5,
    PORT_AUDIO_OUT_6,
    PORT_AUDIO_OUT_7,
    PORT_AUDIO_OUT_8,
    PORT_TOTAL_BARS,
    PORT_DIVISION,
    PORT_STEPS,
    PORT_OFFSET,
    PORT_FADE,
    PORT_TOTAL_COUNT
} PortIndex;

typedef struct {
    const char* label;
    uint32_t port;
    float min;
    float max;
    float def;
    bool is_int;
} FieldDesc;

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
    char edit_text[32];
} Field;

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

    Field fields[5];
    int field_count;
    int active_edit;
    bool clear_on_type;

    volatile bool needs_redraw;
} EMixUI;

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

static const FieldDesc kFields[] = {
    { "TOTAL BARS", PORT_TOTAL_BARS, 1.0f, 4096.0f, 128.0f, true },
    { "DIVISION", PORT_DIVISION, 1.0f, 512.0f, 16.0f, true },
    { "STEPS", PORT_STEPS, 0.0f, 512.0f, 8.0f, true },
    { "OFFSET", PORT_OFFSET, 0.0f, 511.0f, 0.0f, true },
    { "FADE (BARS)", PORT_FADE, 0.0f, 4096.0f, 0.0f, true }
};

static float clamp_value(const Field* field, float value) {
    if (value < field->min) return field->min;
    if (value > field->max) return field->max;
    return value;
}

static void format_value(const Field* field, char* out, size_t out_size) {
    if (field->is_int) {
        snprintf(out, out_size, "%.0f", field->value);
    } else {
        snprintf(out, out_size, "%.3f", field->value);
    }
}

static void draw_ui(EMixUI* ui) {
    cairo_t* cr = cairo_create(ui->surface);

    cairo_set_source_rgb(cr, 0.08, 0.09, 0.11);
    cairo_paint(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14.0);
    cairo_set_source_rgb(cr, 0.95, 0.80, 0.32);
    cairo_move_to(cr, MARGIN, MARGIN + 12);
    cairo_show_text(cr, "E-MIX EUCLIDEAN MIXER");

    for (int i = 0; i < ui->field_count; ++i) {
        Field* field = &ui->fields[i];
        const bool editing = (ui->active_edit == i);

        cairo_set_source_rgb(cr, 0.18, 0.19, 0.23);
        cairo_rectangle(cr, field->x, field->y, field->w, field->h);
        cairo_fill(cr);

        cairo_set_line_width(cr, editing ? 2.0 : 1.0);
        if (editing) {
            cairo_set_source_rgb(cr, 0.95, 0.80, 0.32);
        } else {
            cairo_set_source_rgb(cr, 0.35, 0.35, 0.40);
        }
        cairo_rectangle(cr, field->x, field->y, field->w, field->h);
        cairo_stroke(cr);

        cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 11.0);
        cairo_set_source_rgb(cr, 0.80, 0.82, 0.86);
        cairo_move_to(cr, field->x, field->y - 4);
        cairo_show_text(cr, field->label);

        cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 15.0);
        cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
        cairo_move_to(cr, field->x + VALUE_PADDING, field->y + 26);
        cairo_show_text(cr, field->edit_text);
    }

    cairo_destroy(cr);
    cairo_surface_flush(ui->surface);
    XFlush(ui->display);
}

static void send_value(EMixUI* ui, const Field* field) {
    if (!ui->write) {
        return;
    }
    const float value = field->value;
    ui->write(ui->controller, field->port, sizeof(float), 0, &value);
}

static bool field_hit_test(const Field* field, int x, int y) {
    return x >= field->x && x <= (field->x + field->w) &&
           y >= field->y && y <= (field->y + field->h);
}

static void commit_edit(EMixUI* ui, Field* field) {
    if (!field) {
        return;
    }
    char* end = NULL;
    float value = strtof(field->edit_text, &end);
    if (end == field->edit_text) {
        format_value(field, field->edit_text, sizeof(field->edit_text));
        return;
    }
    value = clamp_value(field, value);
    field->value = field->is_int ? roundf(value) : value;
    format_value(field, field->edit_text, sizeof(field->edit_text));
    send_value(ui, field);
}

static void handle_button_press(EMixUI* ui, XButtonEvent* ev) {
    pthread_mutex_lock(&ui->mutex);
    if (ui->active_edit >= 0 && ui->active_edit < ui->field_count) {
        commit_edit(ui, &ui->fields[ui->active_edit]);
    }
    ui->active_edit = -1;
    ui->clear_on_type = false;
    for (int i = 0; i < ui->field_count; ++i) {
        if (field_hit_test(&ui->fields[i], ev->x, ev->y)) {
            ui->active_edit = i;
            ui->clear_on_type = true;
            XSetInputFocus(ui->display, ui->window, RevertToPointerRoot, CurrentTime);
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
            return;
        }
    }
    ui->needs_redraw = true;
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_key(EMixUI* ui, XKeyEvent* ev) {
    pthread_mutex_lock(&ui->mutex);
    if (ui->active_edit < 0) {
        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    Field* field = &ui->fields[ui->active_edit];

    char buf[8] = {0};
    KeySym sym = 0;
    int len = XLookupString(ev, buf, sizeof(buf) - 1, &sym, NULL);

    if (sym == XK_Return || sym == XK_KP_Enter) {
        commit_edit(ui, field);
        ui->active_edit = -1;
        ui->clear_on_type = false;
        ui->needs_redraw = true;
        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    if (sym == XK_Escape) {
        format_value(field, field->edit_text, sizeof(field->edit_text));
        ui->active_edit = -1;
        ui->clear_on_type = false;
        ui->needs_redraw = true;
        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    if (sym == XK_BackSpace) {
        ui->clear_on_type = false;
        size_t len_text = strlen(field->edit_text);
        if (len_text > 0) {
            field->edit_text[len_text - 1] = '\0';
            ui->needs_redraw = true;
        }
        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    if (len > 0) {
        const char c = buf[0];
        if ((c >= '0' && c <= '9') || c == '.') {
            if (ui->clear_on_type) {
                field->edit_text[0] = '\0';
                ui->clear_on_type = false;
            }
            size_t len_text = strlen(field->edit_text);
            if (len_text + 1 < sizeof(field->edit_text)) {
                field->edit_text[len_text] = c;
                field->edit_text[len_text + 1] = '\0';
                ui->needs_redraw = true;
            }
        }
    }

    pthread_mutex_unlock(&ui->mutex);
}

static void* event_thread_main(void* arg) {
    EMixUI* ui = (EMixUI*)arg;

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

    EMixUI* ui = (EMixUI*)calloc(1, sizeof(EMixUI));
    if (!ui) {
        return NULL;
    }

    ui->write = write_function;
    ui->controller = controller;
    ui->active_edit = -1;
    ui->clear_on_type = false;
    ui->field_count = (int)(sizeof(kFields) / sizeof(kFields[0]));

    ui->width = 340;
    ui->height = MARGIN * 2 + TITLE_HEIGHT + (ui->field_count * (FIELD_HEIGHT + FIELD_GAP));

    void* parent = NULL;
    for (const LV2_Feature* const* f = features; f && *f; ++f) {
        if (!strcmp((*f)->URI, LV2_UI__parent)) {
            parent = (*f)->data;
        }
    }

    if (!parent) {
        fprintf(stderr, "e-mix-ui: no parent window provided\n");
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
    attrs.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask | KeyPressMask;

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

    XStoreName(ui->display, ui->window, "E-Mix");
    XMapWindow(ui->display, ui->window);
    XSetInputFocus(ui->display, ui->window, RevertToPointerRoot, CurrentTime);

    ui->surface = cairo_xlib_surface_create(
        ui->display,
        ui->window,
        DefaultVisual(ui->display, ui->screen),
        ui->width,
        ui->height);
    cairo_xlib_surface_set_size(ui->surface, ui->width, ui->height);

    int y = MARGIN + TITLE_HEIGHT;
    for (int i = 0; i < ui->field_count; ++i) {
        ui->fields[i].label = kFields[i].label;
        ui->fields[i].port = kFields[i].port;
        ui->fields[i].min = kFields[i].min;
        ui->fields[i].max = kFields[i].max;
        ui->fields[i].value = kFields[i].def;
        ui->fields[i].is_int = kFields[i].is_int;
        ui->fields[i].x = MARGIN;
        ui->fields[i].y = y;
        ui->fields[i].w = ui->width - (MARGIN * 2);
        ui->fields[i].h = FIELD_HEIGHT;
        format_value(&ui->fields[i], ui->fields[i].edit_text, sizeof(ui->fields[i].edit_text));
        y += FIELD_HEIGHT + FIELD_GAP;
    }

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
    EMixUI* ui = (EMixUI*)handle;
    if (!ui) {
        return;
    }

    pthread_mutex_lock(&ui->mutex);
    if (ui->active_edit >= 0 && ui->active_edit < ui->field_count) {
        commit_edit(ui, &ui->fields[ui->active_edit]);
        ui->active_edit = -1;
    }
    pthread_mutex_unlock(&ui->mutex);

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
    EMixUI* ui = (EMixUI*)handle;
    if (!ui || !buffer || format != 0 || buffer_size < sizeof(float)) {
        return;
    }

    pthread_mutex_lock(&ui->mutex);
    for (int i = 0; i < ui->field_count; ++i) {
        if (ui->fields[i].port != port_index) {
            continue;
        }

        if (ui->active_edit == i) {
            pthread_mutex_unlock(&ui->mutex);
            return;
        }

        float value = *(const float*)buffer;
        value = clamp_value(&ui->fields[i], value);
        if (ui->fields[i].is_int) {
            value = roundf(value);
        }

        if (fabsf(value - ui->fields[i].value) > 0.0001f) {
            ui->fields[i].value = value;
            format_value(&ui->fields[i], ui->fields[i].edit_text, sizeof(ui->fields[i].edit_text));
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
    EMIX_UI_URI,
    ui_instantiate,
    ui_cleanup,
    ui_port_event,
    ui_extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return (index == 0) ? &ui_descriptor : NULL;
}
