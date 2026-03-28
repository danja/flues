#include <lv2/core/lv2.h>
#include <lv2/ui/ui.h>

#include <X11/Xlib.h>
#include <X11/cursorfont.h>

#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PLUGIN_URI "https://danja.github.io/flues/plugins/bassgen"
#define UI_URI PLUGIN_URI "#ui"

typedef enum {
    PORT_CONTROL = 0,
    PORT_MIDI_OUT,
    PORT_ROOT_NOTE,
    PORT_SCALE,
    PORT_GENRE,
    PORT_CHANNEL,
    PORT_LENGTH_BEATS,
    PORT_SUBDIVISION,
    PORT_DENSITY,
    PORT_REGISTER,
    PORT_HOLD,
    PORT_ACCENT,
    PORT_SEED,
    PORT_ACTION_NEW,
    PORT_ACTION_NOTES,
    PORT_ACTION_RHYTHM,
    PORT_TOTAL_COUNT
} PortIndex;

typedef enum {
    GROUP_GLOBAL = 0,
    GROUP_PATTERN,
    GROUP_ACTIONS,
    GROUP_COUNT
} GroupIndex;

typedef struct {
    GroupIndex group;
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
    int width;
    int height;
} Slider;

typedef struct {
    uint32_t port;
    const char* label;
    const char** items;
    int count;
    int value;
    bool open;
    int item_height;
    int x;
    int y;
    int width;
    int height;
} Selector;

typedef struct {
    uint32_t port;
    const char* label;
    int counter;
    int x;
    int y;
    int width;
    int height;
} ActionButton;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} GroupRect;

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

    Slider sliders[16];
    int slider_count;
    int active_slider;

    Selector selectors[4];
    int selector_count;

    ActionButton actions[3];
    GroupRect groups[GROUP_COUNT];
    int open_selector;
} BassGenUI;

static const char* kGroupNames[GROUP_COUNT] = {
    "GLOBAL",
    "PATTERN",
    "ACTIONS"
};

static const char* kScaleNames[] = {
    "Minor", "Major", "Dorian", "Phrygian", "Pent Minor", "Blues",
    "Mixolydian", "Harm Minor", "Pent Major"
};

static const char* kGenreNames[] = {
    "Techno", "Acid", "House", "Electro", "Dub", "Ambient"
};

static const char* kSubdivisionNames[] = {
    "1/8", "1/16", "1/16T"
};

static const char* kChannelNames[] = {
    "1", "2", "3", "4", "5", "6", "7", "8",
    "9", "10", "11", "12", "13", "14", "15", "16"
};

static const char* kNoteNames[] = {
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B"
};

static const ControlDesc kControlDescs[] = {
    { GROUP_GLOBAL, "ROOT", PORT_ROOT_NOTE, 0.0f, 127.0f, 36.0f, true },
    { GROUP_PATTERN, "LENGTH", PORT_LENGTH_BEATS, 8.0f, 32.0f, 16.0f, true },
    { GROUP_PATTERN, "DENS", PORT_DENSITY, 0.0f, 1.0f, 0.45f, false },
    { GROUP_PATTERN, "REG", PORT_REGISTER, 0.0f, 3.0f, 1.0f, true },
    { GROUP_PATTERN, "HOLD", PORT_HOLD, 0.0f, 1.0f, 0.35f, false },
    { GROUP_PATTERN, "ACC", PORT_ACCENT, 0.0f, 1.0f, 0.45f, false },
    { GROUP_PATTERN, "SEED", PORT_SEED, 0.0f, 65535.0f, 1.0f, true }
};

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

static void notify_host(BassGenUI* ui, uint32_t port, float value) {
    if (ui->write) {
        ui->write(ui->controller, port, sizeof(float), 0, &value);
    }
}

static float clampf_local(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static bool point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}

static int slider_at(BassGenUI* ui, int x, int y) {
    for (int i = 0; i < ui->slider_count; ++i) {
        const Slider* s = &ui->sliders[i];
        if (point_in_rect(x, y, s->x, s->y, s->width, s->height + 28)) {
            return i;
        }
    }
    return -1;
}

static float slider_value_from_y(const Slider* slider, int y) {
    const int top = slider->y;
    const int bottom = slider->y + slider->height;
    float t = 1.0f - (float)(y - top) / (float)(bottom - top);
    t = clampf_local(t, 0.0f, 1.0f);
    return slider->min + t * (slider->max - slider->min);
}

static void set_slider_value(BassGenUI* ui, Slider* slider, float value, bool notify) {
    value = clampf_local(value, slider->min, slider->max);
    if (slider->is_int) {
        value = floorf(value + 0.5f);
    }
    if (fabsf(value - slider->value) > 0.0001f) {
        slider->value = value;
        if (notify) {
            notify_host(ui, slider->port, value);
        }
        ui->needs_redraw = true;
    }
}

static Selector* selector_at(BassGenUI* ui, int x, int y) {
    for (int i = 0; i < ui->selector_count; ++i) {
        Selector* s = &ui->selectors[i];
        if (point_in_rect(x, y, s->x, s->y, s->width, s->height)) {
            return s;
        }
    }
    return NULL;
}

static int selector_menu_y(BassGenUI* ui, const Selector* s) {
    const int menu_h = s->count * s->item_height;
    const int down_y = s->y + s->height;
    if (down_y + menu_h <= ui->height - 8) {
        return down_y;
    }
    return s->y - menu_h;
}

static int selector_index_at(BassGenUI* ui, int x, int y) {
    for (int i = 0; i < ui->selector_count; ++i) {
        Selector* s = &ui->selectors[i];
        if (point_in_rect(x, y, s->x, s->y, s->width, s->height)) {
            return i;
        }
    }
    return -1;
}

static int selector_menu_item_at_ui(BassGenUI* ui, const Selector* s, int x, int y) {
    if (!s->open) {
        return -1;
    }
    const int menu_y = selector_menu_y(ui, s);
    if (!point_in_rect(x, y, s->x, menu_y, s->width, s->count * s->item_height)) {
        return -1;
    }
    const int item = (y - menu_y) / s->item_height;
    return (item >= 0 && item < s->count) ? item : -1;
}

static ActionButton* action_at(BassGenUI* ui, int x, int y) {
    for (int i = 0; i < 3; ++i) {
        ActionButton* a = &ui->actions[i];
        if (point_in_rect(x, y, a->x, a->y, a->width, a->height)) {
            return a;
        }
    }
    return NULL;
}

static void draw_group(cairo_t* cr, const GroupRect* rect, const char* title) {
    cairo_rectangle(cr, rect->x, rect->y, rect->width, rect->height);
    cairo_set_source_rgb(cr, 0.13, 0.14, 0.18);
    cairo_fill(cr);

    cairo_rectangle(cr, rect->x, rect->y, rect->width, rect->height);
    cairo_set_source_rgb(cr, 0.32, 0.33, 0.38);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.94, 0.80, 0.42);
    cairo_move_to(cr, rect->x + 10, rect->y + 16);
    cairo_show_text(cr, title);
}

static void draw_slider(cairo_t* cr, const Slider* s) {
    const int track_x = s->x + (s->width - 6) / 2;
    cairo_rectangle(cr, track_x, s->y, 6, s->height);
    cairo_set_source_rgb(cr, 0.21, 0.22, 0.25);
    cairo_fill(cr);

    const float norm = (s->value - s->min) / (s->max - s->min);
    const int fill_h = (int)lrintf(norm * s->height);
    const int fill_y = s->y + (s->height - fill_h);
    cairo_rectangle(cr, track_x, fill_y, 6, fill_h);
    cairo_set_source_rgb(cr, 0.94, 0.57, 0.22);
    cairo_fill(cr);

    cairo_rectangle(cr, s->x + 2, fill_y - 3, s->width - 4, 6);
    cairo_set_source_rgb(cr, 0.95, 0.90, 0.82);
    cairo_fill(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 9.0);
    cairo_set_source_rgb(cr, 0.92, 0.92, 0.92);

    char value[24];
    if (s->is_int) {
        snprintf(value, sizeof(value), "%d", (int)lroundf(s->value));
    } else {
        snprintf(value, sizeof(value), "%.2f", s->value);
    }

    cairo_text_extents_t ext;
    cairo_text_extents(cr, value, &ext);
    cairo_move_to(cr, s->x + (s->width - ext.width) / 2.0, s->y + s->height + 10.0);
    cairo_show_text(cr, value);

    cairo_text_extents(cr, s->label, &ext);
    cairo_move_to(cr, s->x + (s->width - ext.width) / 2.0, s->y + s->height + 22.0);
    cairo_show_text(cr, s->label);

    if (s->port == PORT_ROOT_NOTE) {
        const int midi_note = (int)lroundf(clampf_local(s->value, s->min, s->max));
        const int note_index = midi_note % 12;
        const int octave = (midi_note / 12) - 1;
        char note_label[16];
        snprintf(note_label, sizeof(note_label), "%s%d", kNoteNames[note_index], octave);
        cairo_text_extents(cr, note_label, &ext);
        cairo_move_to(cr, s->x + (s->width - ext.width) / 2.0, s->y + s->height + 34.0);
        cairo_show_text(cr, note_label);
    }
}

static void draw_selector(cairo_t* cr, const Selector* s) {
    cairo_rectangle(cr, s->x, s->y, s->width, s->height);
    cairo_set_source_rgb(cr, 0.18, 0.19, 0.22);
    cairo_fill(cr);

    cairo_rectangle(cr, s->x, s->y, s->width, s->height);
    cairo_set_source_rgb(cr, 0.89, 0.72, 0.35);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.93, 0.93, 0.93);

    char text[64];
    snprintf(text, sizeof(text), "%s: %s", s->label, s->items[s->value]);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, text, &ext);
    cairo_move_to(cr, s->x + 8, s->y + (s->height - ext.height) / 2.0 - ext.y_bearing);
    cairo_show_text(cr, text);

    cairo_move_to(cr, s->x + s->width - 16, s->y + s->height / 2 - 3);
    cairo_line_to(cr, s->x + s->width - 8, s->y + s->height / 2 - 3);
    cairo_line_to(cr, s->x + s->width - 12, s->y + s->height / 2 + 3);
    cairo_close_path(cr);
    cairo_fill(cr);

}

static void draw_selector_with_ui(cairo_t* cr, BassGenUI* ui, const Selector* s) {
    draw_selector(cr, s);
    if (s->open) {
        const int menu_y = selector_menu_y(ui, s);
        cairo_rectangle(cr, s->x, menu_y, s->width, s->count * s->item_height);
        cairo_set_source_rgb(cr, 0.15, 0.16, 0.19);
        cairo_fill(cr);

        cairo_rectangle(cr, s->x, menu_y, s->width, s->count * s->item_height);
        cairo_set_source_rgb(cr, 0.89, 0.72, 0.35);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 10.0);
        for (int i = 0; i < s->count; ++i) {
            const int iy = menu_y + i * s->item_height;
            if (i == s->value) {
                cairo_rectangle(cr, s->x + 1, iy + 1, s->width - 2, s->item_height - 2);
                cairo_set_source_rgb(cr, 0.26, 0.31, 0.21);
                cairo_fill(cr);
            }
            cairo_set_source_rgb(cr, 0.93, 0.93, 0.93);
            cairo_text_extents_t ext;
            cairo_text_extents(cr, s->items[i], &ext);
            cairo_move_to(cr, s->x + 8, iy + (s->item_height - ext.height) / 2.0 - ext.y_bearing);
            cairo_show_text(cr, s->items[i]);
        }
    }
}

static void draw_action(cairo_t* cr, const ActionButton* a) {
    cairo_rectangle(cr, a->x, a->y, a->width, a->height);
    cairo_set_source_rgb(cr, 0.22, 0.24, 0.28);
    cairo_fill(cr);

    cairo_rectangle(cr, a->x, a->y, a->width, a->height);
    cairo_set_source_rgb(cr, 0.48, 0.78, 0.49);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12.0);
    cairo_set_source_rgb(cr, 0.97, 0.97, 0.97);

    cairo_text_extents_t ext;
    cairo_text_extents(cr, a->label, &ext);
    cairo_move_to(cr,
                  a->x + (a->width - ext.width) / 2.0 - ext.x_bearing,
                  a->y + (a->height - ext.height) / 2.0 - ext.y_bearing);
    cairo_show_text(cr, a->label);
}

static void draw_ui(BassGenUI* ui) {
    cairo_t* cr = cairo_create(ui->surface);
    cairo_set_source_rgb(cr, 0.09, 0.09, 0.11);
    cairo_paint(cr);

    for (int i = 0; i < GROUP_COUNT; ++i) {
        draw_group(cr, &ui->groups[i], kGroupNames[i]);
    }

    for (int i = 0; i < ui->slider_count; ++i) {
        draw_slider(cr, &ui->sliders[i]);
    }
    for (int i = 0; i < ui->selector_count; ++i) {
        draw_selector(cr, &ui->selectors[i]);
    }
    for (int i = 0; i < 3; ++i) {
        draw_action(cr, &ui->actions[i]);
    }
    if (ui->open_selector >= 0) {
        draw_selector_with_ui(cr, ui, &ui->selectors[ui->open_selector]);
    }

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.86, 0.86, 0.86);
    char status[192];
    snprintf(status, sizeof(status),
             "Root %d  Scale %s  Genre %s  Ch %s  Len %d  Sub %s  Density %.2f  Hold %.2f",
             (int)lroundf(ui->sliders[0].value),
             ui->selectors[0].items[ui->selectors[0].value],
             ui->selectors[1].items[ui->selectors[1].value],
             ui->selectors[2].items[ui->selectors[2].value],
             (int)lroundf(ui->sliders[1].value),
             ui->selectors[3].items[ui->selectors[3].value],
             ui->sliders[2].value,
             ui->sliders[4].value);
    cairo_move_to(cr, 16, ui->height - 14);
    cairo_show_text(cr, status);

    cairo_destroy(cr);
}

static void setup_layout(BassGenUI* ui) {
    ui->width = 760;
    ui->height = 396;

    ui->groups[GROUP_GLOBAL] = (GroupRect){10, 10, 360, 240};
    ui->groups[GROUP_PATTERN] = (GroupRect){380, 10, 370, 240};
    ui->groups[GROUP_ACTIONS] = (GroupRect){10, 260, 740, 92};

    ui->slider_count = 0;
    ui->selector_count = 0;

    for (int i = 0; i < (int)(sizeof(kControlDescs) / sizeof(kControlDescs[0])); ++i) {
        const ControlDesc* d = &kControlDescs[i];
        Slider* s = &ui->sliders[ui->slider_count++];
        s->port = d->port;
        s->label = d->label;
        s->min = d->min;
        s->max = d->max;
        s->value = d->def;
        s->is_int = d->is_int;
        s->width = 26;
        s->height = 120;

        GroupRect* g = &ui->groups[d->group];
        const int local_index = (d->group == GROUP_GLOBAL) ? i : (i - 1);
        s->x = g->x + 16 + local_index * 48;
        s->y = g->y + 34;
    }

    Selector* s = &ui->selectors[ui->selector_count++];
    s->port = PORT_SCALE; s->label = "Scale"; s->items = kScaleNames; s->count = 9; s->value = 0; s->open = false; s->item_height = 20;
    s->x = ui->groups[GROUP_GLOBAL].x + 70; s->y = ui->groups[GROUP_GLOBAL].y + 38; s->width = 260; s->height = 24;

    s = &ui->selectors[ui->selector_count++];
    s->port = PORT_GENRE; s->label = "Genre"; s->items = kGenreNames; s->count = 6; s->value = 0; s->open = false; s->item_height = 20;
    s->x = ui->groups[GROUP_GLOBAL].x + 70; s->y = ui->groups[GROUP_GLOBAL].y + 72; s->width = 260; s->height = 24;

    s = &ui->selectors[ui->selector_count++];
    s->port = PORT_CHANNEL; s->label = "Channel"; s->items = kChannelNames; s->count = 16; s->value = 0; s->open = false; s->item_height = 18;
    s->x = ui->groups[GROUP_GLOBAL].x + 70; s->y = ui->groups[GROUP_GLOBAL].y + 106; s->width = 260; s->height = 24;

    s = &ui->selectors[ui->selector_count++];
    s->port = PORT_SUBDIVISION; s->label = "Subdivision"; s->items = kSubdivisionNames; s->count = 3; s->value = 1; s->open = false; s->item_height = 20;
    s->x = ui->groups[GROUP_PATTERN].x + 196; s->y = ui->groups[GROUP_PATTERN].y + 198; s->width = 158; s->height = 24;

    ui->actions[0] = (ActionButton){PORT_ACTION_NEW, "NEW", 0, 34, 282, 200, 48};
    ui->actions[1] = (ActionButton){PORT_ACTION_NOTES, "NOTES", 0, 280, 282, 200, 48};
    ui->actions[2] = (ActionButton){PORT_ACTION_RHYTHM, "RHYTHM", 0, 526, 282, 200, 48};
    ui->open_selector = -1;
}

static void handle_motion(BassGenUI* ui, XMotionEvent* ev) {
    if (ui->active_slider < 0) {
        return;
    }
    Slider* s = &ui->sliders[ui->active_slider];
    set_slider_value(ui, s, slider_value_from_y(s, ev->y), true);
}

static void handle_press(BassGenUI* ui, XButtonEvent* ev) {
    if (ev->button != Button1) {
        return;
    }

    if (ui->open_selector >= 0) {
        Selector* open = &ui->selectors[ui->open_selector];
        const int item = selector_menu_item_at_ui(ui, open, ev->x, ev->y);
        if (item >= 0) {
            open->value = item;
            notify_host(ui, open->port, (float)((open->port == PORT_CHANNEL) ? (open->value + 1) : open->value));
            open->open = false;
            ui->open_selector = -1;
            ui->needs_redraw = true;
            return;
        }

        if (!point_in_rect(ev->x, ev->y, open->x, open->y, open->width, open->height)) {
            open->open = false;
            ui->open_selector = -1;
            ui->needs_redraw = true;
        }
    }

    const int sel_index = selector_index_at(ui, ev->x, ev->y);
    if (sel_index >= 0) {
        if (ui->open_selector >= 0 && ui->open_selector != sel_index) {
            ui->selectors[ui->open_selector].open = false;
        }
        ui->selectors[sel_index].open = !ui->selectors[sel_index].open;
        ui->open_selector = ui->selectors[sel_index].open ? sel_index : -1;
        ui->needs_redraw = true;
        return;
    }

    ActionButton* action = action_at(ui, ev->x, ev->y);
    if (action) {
        action->counter += 1;
        notify_host(ui, action->port, (float)action->counter);
        ui->needs_redraw = true;
        return;
    }

    const int index = slider_at(ui, ev->x, ev->y);
    if (index >= 0) {
        ui->active_slider = index;
        Slider* s = &ui->sliders[index];
        set_slider_value(ui, s, slider_value_from_y(s, ev->y), true);
    }
}

static void handle_release(BassGenUI* ui, XButtonEvent* ev) {
    if (ev->button == Button1) {
        ui->active_slider = -1;
    }
}

static void* event_thread_main(void* arg) {
    BassGenUI* ui = (BassGenUI*)arg;
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
                case MotionNotify:
                    handle_motion(ui, &ev.xmotion);
                    break;
                case ButtonPress:
                    handle_press(ui, &ev.xbutton);
                    break;
                case ButtonRelease:
                    handle_release(ui, &ev.xbutton);
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

static LV2UI_Handle ui_instantiate(const LV2UI_Descriptor*,
                                   const char*,
                                   const char*,
                                   LV2UI_Write_Function write_function,
                                   LV2UI_Controller controller,
                                   LV2UI_Widget* widget,
                                   const LV2_Feature* const* features) {
    ensure_xlib_threads();

    BassGenUI* ui = (BassGenUI*)calloc(1, sizeof(BassGenUI));
    if (!ui) {
        return NULL;
    }

    pthread_mutex_init(&ui->mutex, NULL);
    ui->write = write_function;
    ui->controller = controller;
    ui->active_slider = -1;
    ui->open_selector = -1;

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
    attrs.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
    attrs.background_pixel = BlackPixel(ui->display, ui->screen);

    ui->window = XCreateWindow(ui->display, parent, 0, 0, ui->width, ui->height, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWBackPixel | CWEventMask, &attrs);

    Cursor hand_cursor = XCreateFontCursor(ui->display, XC_hand2);
    XDefineCursor(ui->display, ui->window, hand_cursor);
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
    BassGenUI* ui = (BassGenUI*)handle;
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
    BassGenUI* ui = (BassGenUI*)handle;
    if (!ui || !buffer || format != 0) {
        return;
    }
    (void)buffer_size;

    const float value = *(const float*)buffer;
    pthread_mutex_lock(&ui->mutex);

    for (int i = 0; i < ui->slider_count; ++i) {
        if (ui->sliders[i].port == port_index) {
            set_slider_value(ui, &ui->sliders[i], value, false);
            pthread_mutex_unlock(&ui->mutex);
            return;
        }
    }

    for (int i = 0; i < ui->selector_count; ++i) {
        if (ui->selectors[i].port == port_index) {
            int v = (int)lroundf(value);
            if (port_index == PORT_CHANNEL) {
                v -= 1;
            }
            if (v < 0) v = 0;
            if (v >= ui->selectors[i].count) v = ui->selectors[i].count - 1;
            ui->selectors[i].value = v;
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
            return;
        }
    }

    pthread_mutex_unlock(&ui->mutex);
}

static const LV2UI_Descriptor descriptor = {
    UI_URI,
    ui_instantiate,
    ui_cleanup,
    ui_port_event,
    NULL
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : NULL;
}
