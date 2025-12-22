// euclid_ui_x11.c
// X11/Cairo UI for Flues Euclid rhythm generator

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

#define PLUGIN_URI "https://danja.github.io/flues/plugins/euclid"
#define UI_URI PLUGIN_URI "#ui"

// Port indices (must match plugin)
typedef enum {
    PORT_CONTROL = 0,
    PORT_MIDI_OUT,
    PORT_STEPS_PER_BAR,
    PORT_SWING,
    PORT_SEED,
    PORT_KICK_BEATS,
    PORT_KICK_OFFSET,
    PORT_KICK_LENGTH,
    PORT_KICK_RANDOM,
    PORT_SNARE_BEATS,
    PORT_SNARE_OFFSET,
    PORT_SNARE_LENGTH,
    PORT_SNARE_RANDOM,
    PORT_CLAP_BEATS,
    PORT_CLAP_OFFSET,
    PORT_CLAP_LENGTH,
    PORT_CLAP_RANDOM,
    PORT_HH_CLOSED_BEATS,
    PORT_HH_CLOSED_OFFSET,
    PORT_HH_CLOSED_LENGTH,
    PORT_HH_CLOSED_RANDOM,
    PORT_HH_OPEN_BEATS,
    PORT_HH_OPEN_OFFSET,
    PORT_HH_OPEN_LENGTH,
    PORT_HH_OPEN_RANDOM,
    PORT_TOM_LO_BEATS,
    PORT_TOM_LO_OFFSET,
    PORT_TOM_LO_LENGTH,
    PORT_TOM_LO_RANDOM,
    PORT_TOM_HI_BEATS,
    PORT_TOM_HI_OFFSET,
    PORT_TOM_HI_LENGTH,
    PORT_TOM_HI_RANDOM,
    PORT_CRASH_BEATS,
    PORT_CRASH_OFFSET,
    PORT_CRASH_LENGTH,
    PORT_CRASH_RANDOM,
    PORT_BASH_BEATS,
    PORT_BASH_OFFSET,
    PORT_BASH_LENGTH,
    PORT_BASH_RANDOM,
    PORT_TOTAL_COUNT
} PortIndex;

// Group indices
typedef enum {
    GROUP_GLOBAL = 0,
    GROUP_KICK,
    GROUP_SNARE,
    GROUP_CLAP,
    GROUP_HH_CLOSED,
    GROUP_HH_OPEN,
    GROUP_TOM_LO,
    GROUP_TOM_HI,
    GROUP_CRASH,
    GROUP_BASH,
    GROUP_COUNT
} GroupIndex;

// Layout constants
#define GROUP_PADDING 8
#define GROUP_GAP_X 10
#define GROUP_GAP_Y 12
#define TITLE_HEIGHT 12
#define SLIDER_WIDTH 24
#define SLIDER_HEIGHT 90
#define SLIDER_LABEL_HEIGHT 14
#define SLIDER_GAP_X 12

// Control descriptor
typedef struct {
    GroupIndex group;
    const char* label;
    uint32_t port;
    float min;
    float max;
    float def;
    bool is_int;
} ControlDesc;

// Slider runtime state
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
} Slider;

// Group layout state
typedef struct {
    int row;
    int col;
    int columns;
    int count;
    int assigned;
    int x;
    int y;
    int width;
    int height;
} GroupState;

// UI instance
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

    Slider sliders[PORT_TOTAL_COUNT];
    bool slider_used[PORT_TOTAL_COUNT];
    int slider_by_port[PORT_TOTAL_COUNT];

    GroupState groups[GROUP_COUNT];

    volatile bool needs_redraw;
    int active_slider;
} EuclidUI;

// Control definitions
static const ControlDesc kControls[] = {
    { GROUP_GLOBAL, "STEPS", PORT_STEPS_PER_BAR, 8.0f, 24.0f, 16.0f, true },
    { GROUP_GLOBAL, "SWING", PORT_SWING, 0.0f, 1.0f, 0.0f, false },
    { GROUP_GLOBAL, "SEED", PORT_SEED, 0.0f, 65535.0f, 1.0f, true },

    { GROUP_KICK, "BEATS", PORT_KICK_BEATS, 0.0f, 24.0f, 4.0f, true },
    { GROUP_KICK, "OFFSET", PORT_KICK_OFFSET, 0.0f, 23.0f, 0.0f, true },
    { GROUP_KICK, "LENGTH", PORT_KICK_LENGTH, 1.0f, 24.0f, 16.0f, true },
    { GROUP_KICK, "RAND", PORT_KICK_RANDOM, 0.0f, 1.0f, 0.0f, false },

    { GROUP_SNARE, "BEATS", PORT_SNARE_BEATS, 0.0f, 24.0f, 2.0f, true },
    { GROUP_SNARE, "OFFSET", PORT_SNARE_OFFSET, 0.0f, 23.0f, 8.0f, true },
    { GROUP_SNARE, "LENGTH", PORT_SNARE_LENGTH, 1.0f, 24.0f, 16.0f, true },
    { GROUP_SNARE, "RAND", PORT_SNARE_RANDOM, 0.0f, 1.0f, 0.0f, false },

    { GROUP_CLAP, "BEATS", PORT_CLAP_BEATS, 0.0f, 24.0f, 0.0f, true },
    { GROUP_CLAP, "OFFSET", PORT_CLAP_OFFSET, 0.0f, 23.0f, 0.0f, true },
    { GROUP_CLAP, "LENGTH", PORT_CLAP_LENGTH, 1.0f, 24.0f, 16.0f, true },
    { GROUP_CLAP, "RAND", PORT_CLAP_RANDOM, 0.0f, 1.0f, 0.0f, false },

    { GROUP_HH_CLOSED, "BEATS", PORT_HH_CLOSED_BEATS, 0.0f, 24.0f, 8.0f, true },
    { GROUP_HH_CLOSED, "OFFSET", PORT_HH_CLOSED_OFFSET, 0.0f, 23.0f, 0.0f, true },
    { GROUP_HH_CLOSED, "LENGTH", PORT_HH_CLOSED_LENGTH, 1.0f, 24.0f, 16.0f, true },
    { GROUP_HH_CLOSED, "RAND", PORT_HH_CLOSED_RANDOM, 0.0f, 1.0f, 0.0f, false },

    { GROUP_HH_OPEN, "BEATS", PORT_HH_OPEN_BEATS, 0.0f, 24.0f, 0.0f, true },
    { GROUP_HH_OPEN, "OFFSET", PORT_HH_OPEN_OFFSET, 0.0f, 23.0f, 0.0f, true },
    { GROUP_HH_OPEN, "LENGTH", PORT_HH_OPEN_LENGTH, 1.0f, 24.0f, 16.0f, true },
    { GROUP_HH_OPEN, "RAND", PORT_HH_OPEN_RANDOM, 0.0f, 1.0f, 0.0f, false },

    { GROUP_TOM_LO, "BEATS", PORT_TOM_LO_BEATS, 0.0f, 24.0f, 0.0f, true },
    { GROUP_TOM_LO, "OFFSET", PORT_TOM_LO_OFFSET, 0.0f, 23.0f, 0.0f, true },
    { GROUP_TOM_LO, "LENGTH", PORT_TOM_LO_LENGTH, 1.0f, 24.0f, 16.0f, true },
    { GROUP_TOM_LO, "RAND", PORT_TOM_LO_RANDOM, 0.0f, 1.0f, 0.0f, false },

    { GROUP_TOM_HI, "BEATS", PORT_TOM_HI_BEATS, 0.0f, 24.0f, 0.0f, true },
    { GROUP_TOM_HI, "OFFSET", PORT_TOM_HI_OFFSET, 0.0f, 23.0f, 0.0f, true },
    { GROUP_TOM_HI, "LENGTH", PORT_TOM_HI_LENGTH, 1.0f, 24.0f, 16.0f, true },
    { GROUP_TOM_HI, "RAND", PORT_TOM_HI_RANDOM, 0.0f, 1.0f, 0.0f, false },

    { GROUP_CRASH, "BEATS", PORT_CRASH_BEATS, 0.0f, 24.0f, 1.0f, true },
    { GROUP_CRASH, "OFFSET", PORT_CRASH_OFFSET, 0.0f, 23.0f, 0.0f, true },
    { GROUP_CRASH, "LENGTH", PORT_CRASH_LENGTH, 1.0f, 24.0f, 16.0f, true },
    { GROUP_CRASH, "RAND", PORT_CRASH_RANDOM, 0.0f, 1.0f, 0.0f, false },

    { GROUP_BASH, "BEATS", PORT_BASH_BEATS, 0.0f, 24.0f, 0.0f, true },
    { GROUP_BASH, "OFFSET", PORT_BASH_OFFSET, 0.0f, 23.0f, 0.0f, true },
    { GROUP_BASH, "LENGTH", PORT_BASH_LENGTH, 1.0f, 24.0f, 16.0f, true },
    { GROUP_BASH, "RAND", PORT_BASH_RANDOM, 0.0f, 1.0f, 0.0f, false }
};

static const int kControlCount = sizeof(kControls) / sizeof(kControls[0]);

static const char* kGroupNames[GROUP_COUNT] = {
    "GLOBAL", "KICK", "SNARE", "CLAP", "CLOSED HH", "OPEN HH", "LO TOM", "HI TOM", "CRASH", "BASH"
};

static const GroupIndex kRowGroups[][4] = {
    { GROUP_GLOBAL, GROUP_KICK, GROUP_SNARE, GROUP_CLAP },
    { GROUP_HH_CLOSED, GROUP_HH_OPEN, GROUP_TOM_LO, GROUP_TOM_HI },
    { GROUP_CRASH, GROUP_BASH, GROUP_COUNT, GROUP_COUNT }
};

static const int kRowCount = sizeof(kRowGroups) / sizeof(kRowGroups[0]);

static const int kGroupColumns[GROUP_COUNT] = {
    3, // Global
    4, // Kick
    4, // Snare
    4, // Clap
    4, // Closed HH
    4, // Open HH
    4, // Lo Tom
    4, // Hi Tom
    4, // Crash
    4  // Bash
};

// ===== XLib Threading Setup =====

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

// ===== Helper Functions =====

static float clamp_value(const Slider* slider, float value) {
    if (value < slider->min) return slider->min;
    if (value > slider->max) return slider->max;
    return value;
}

static void notify_host(EuclidUI* ui, uint32_t port, float value) {
    if (ui->write) {
        ui->write(ui->controller, port, sizeof(float), 0, &value);
    }
}

static int slider_at(EuclidUI* ui, int x, int y) {
    for (int i = 0; i < PORT_TOTAL_COUNT; ++i) {
        if (!ui->slider_used[i]) {
            continue;
        }
        const Slider* slider = &ui->sliders[i];
        const int left = slider->x;
        const int top = slider->y;
        const int right = left + slider->width;
        const int bottom = top + slider->height + SLIDER_LABEL_HEIGHT;
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

// ===== Drawing =====

static void draw_group_background(cairo_t* cr, const GroupState* group, const char* title) {
    const int x = group->x;
    const int y = group->y;
    const int w = group->width;
    const int h = group->height;

    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source_rgb(cr, 0.14, 0.15, 0.19);
    cairo_fill(cr);

    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source_rgb(cr, 0.30, 0.31, 0.37);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12.0);
    cairo_set_source_rgb(cr, 0.95, 0.82, 0.46);
    cairo_move_to(cr, x + GROUP_PADDING, y + GROUP_PADDING + 10);
    cairo_show_text(cr, title);

    cairo_new_path(cr);
}

static void draw_slider(cairo_t* cr, const Slider* slider) {
    const int x = slider->x;
    const int y = slider->y;
    const int w = slider->width;
    const int h = slider->height;

    const int track_w = 6;
    const int track_x = x + (w - track_w) / 2;

    cairo_rectangle(cr, track_x, y, track_w, h);
    cairo_set_source_rgb(cr, 0.20, 0.21, 0.24);
    cairo_fill(cr);

    const float norm = (slider->value - slider->min) / (slider->max - slider->min);
    const int fill_h = (int)lrintf(norm * h);
    const int fill_y = y + (h - fill_h);

    cairo_rectangle(cr, track_x, fill_y, track_w, fill_h);
    cairo_set_source_rgb(cr, 0.96, 0.62, 0.26);
    cairo_fill(cr);

    const int handle_h = 6;
    cairo_rectangle(cr, x + 2, fill_y - handle_h / 2, w - 4, handle_h);
    cairo_set_source_rgb(cr, 0.95, 0.90, 0.80);
    cairo_fill(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);

    char value_str[16];
    if (slider->is_int) {
        snprintf(value_str, sizeof(value_str), "%d", (int)lroundf(slider->value));
    } else {
        snprintf(value_str, sizeof(value_str), "%.2f", slider->value);
    }
    cairo_text_extents_t extents;
    cairo_text_extents(cr, value_str, &extents);
    cairo_move_to(cr, x + (w - extents.width) / 2.0, y + h + 10.0);
    cairo_show_text(cr, value_str);

    cairo_text_extents(cr, slider->label, &extents);
    cairo_move_to(cr, x + (w - extents.width) / 2.0, y + h + SLIDER_LABEL_HEIGHT + 2.0);
    cairo_show_text(cr, slider->label);

    cairo_new_path(cr);
}

static void draw_ui(EuclidUI* ui) {
    if (!ui->surface) return;

    cairo_t* cr = cairo_create(ui->surface);

    cairo_set_source_rgb(cr, 0.1, 0.1, 0.12);
    cairo_paint(cr);

    for (int g = 0; g < GROUP_COUNT; ++g) {
        if (ui->groups[g].count > 0) {
            draw_group_background(cr, &ui->groups[g], kGroupNames[g]);
        }
    }

    for (int i = 0; i < PORT_TOTAL_COUNT; ++i) {
        if (ui->slider_used[i]) {
            draw_slider(cr, &ui->sliders[i]);
        }
    }

    cairo_destroy(cr);
}

// ===== Layout =====

static void setup_layout(EuclidUI* ui) {
    const int group_width = GROUP_PADDING * 2 + (SLIDER_WIDTH * 4) + (SLIDER_GAP_X * 3);
    const int group_height = GROUP_PADDING * 2 + TITLE_HEIGHT + SLIDER_HEIGHT + SLIDER_LABEL_HEIGHT + 8;

    for (int g = 0; g < GROUP_COUNT; ++g) {
        ui->groups[g].count = 0;
        ui->groups[g].assigned = 0;
        ui->groups[g].columns = kGroupColumns[g];
        ui->groups[g].row = 0;
        ui->groups[g].col = 0;
    }

    for (int r = 0; r < kRowCount; ++r) {
        for (int c = 0; c < 4; ++c) {
            GroupIndex group = kRowGroups[r][c];
            if (group == GROUP_COUNT) {
                continue;
            }
            ui->groups[group].row = r;
            ui->groups[group].col = c;
        }
    }

    for (int i = 0; i < kControlCount; ++i) {
        ui->groups[kControls[i].group].count++;
    }

    const int margin = 10;
    int total_rows = kRowCount;
    ui->width = margin * 2 + (group_width * 4) + (GROUP_GAP_X * 3);
    ui->height = margin * 2 + (group_height * total_rows) + (GROUP_GAP_Y * (total_rows - 1));

    for (int g = 0; g < GROUP_COUNT; ++g) {
        GroupState* group = &ui->groups[g];
        if (group->count == 0) continue;
        group->width = group_width;
        group->height = group_height;
        group->x = margin + group->col * (group_width + GROUP_GAP_X);
        group->y = margin + group->row * (group_height + GROUP_GAP_Y);
    }

    memset(ui->slider_used, 0, sizeof(ui->slider_used));
    for (int i = 0; i < PORT_TOTAL_COUNT; ++i) {
        ui->slider_by_port[i] = -1;
    }

    for (int i = 0; i < kControlCount; ++i) {
        const ControlDesc* desc = &kControls[i];
        GroupState* group = &ui->groups[desc->group];
        if (group->assigned >= group->columns) {
            continue;
        }
        int col = group->assigned++;
        Slider* slider = &ui->sliders[desc->port];
        slider->port = desc->port;
        slider->label = desc->label;
        slider->min = desc->min;
        slider->max = desc->max;
        slider->def = desc->def;
        slider->value = desc->def;
        slider->is_int = desc->is_int;
        slider->width = SLIDER_WIDTH;
        slider->height = SLIDER_HEIGHT;
        slider->x = group->x + GROUP_PADDING + col * (SLIDER_WIDTH + SLIDER_GAP_X);
        slider->y = group->y + GROUP_PADDING + TITLE_HEIGHT + 6;
        ui->slider_used[desc->port] = true;
        ui->slider_by_port[desc->port] = desc->port;
    }
}

// ===== Event Thread =====

static void handle_motion(EuclidUI* ui, XMotionEvent* motion) {
    if (ui->active_slider < 0) return;
    Slider* slider = &ui->sliders[ui->active_slider];
    float value = slider_value_from_y(slider, motion->y);
    if (slider->is_int) {
        value = floorf(value + 0.5f);
    }
    value = clamp_value(slider, value);
    if (fabsf(value - slider->value) > 0.0001f) {
        slider->value = value;
        notify_host(ui, slider->port, value);
        ui->needs_redraw = true;
    }
}

static void handle_button_press(EuclidUI* ui, XButtonEvent* button) {
    if (button->button != Button1) return;
    int index = slider_at(ui, button->x, button->y);
    if (index >= 0) {
        ui->active_slider = index;
        Slider* slider = &ui->sliders[index];
        float value = slider_value_from_y(slider, button->y);
        if (slider->is_int) {
            value = floorf(value + 0.5f);
        }
        value = clamp_value(slider, value);
        slider->value = value;
        notify_host(ui, slider->port, value);
        ui->needs_redraw = true;
    }
}

static void handle_button_release(EuclidUI* ui, XButtonEvent* button) {
    if (button->button != Button1) return;
    ui->active_slider = -1;
}

static void* event_thread_main(void* arg) {
    EuclidUI* ui = (EuclidUI*)arg;
    while (ui->running) {
        while (XPending(ui->display)) {
            XEvent ev;
            XNextEvent(ui->display, &ev);

            pthread_mutex_lock(&ui->mutex);
            switch (ev.type) {
                case Expose:
                    ui->needs_redraw = true;
                    break;
                case ConfigureNotify:
                    ui->needs_redraw = true;
                    break;
                case MotionNotify:
                    handle_motion(ui, &ev.xmotion);
                    break;
                case ButtonPress:
                    handle_button_press(ui, &ev.xbutton);
                    break;
                case ButtonRelease:
                    handle_button_release(ui, &ev.xbutton);
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

// ===== LV2 UI Callbacks =====

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

    EuclidUI* ui = (EuclidUI*)calloc(1, sizeof(EuclidUI));
    pthread_mutex_init(&ui->mutex, NULL);
    ui->write = write_function;
    ui->controller = controller;
    ui->active_slider = -1;

    ui->display = XOpenDisplay(NULL);
    if (!ui->display) {
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

    XMapWindow(ui->display, ui->window);

    ui->surface = cairo_xlib_surface_create(ui->display, ui->window,
                                            DefaultVisual(ui->display, ui->screen),
                                            ui->width, ui->height);

    ui->running = true;
    pthread_create(&ui->thread, NULL, event_thread_main, ui);

    *widget = (LV2UI_Widget)(uintptr_t)ui->window;
    return ui;
}

static void ui_cleanup(LV2UI_Handle handle) {
    EuclidUI* ui = (EuclidUI*)handle;

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
    EuclidUI* ui = (EuclidUI*)handle;
    if (!ui || !buffer || format != 0) return;
    (void)buffer_size;

    float value = *((const float*)buffer);

    pthread_mutex_lock(&ui->mutex);
    if (port_index < PORT_TOTAL_COUNT && ui->slider_used[port_index]) {
        Slider* slider = &ui->sliders[port_index];
        value = clamp_value(slider, value);
        if (slider->is_int) {
            value = floorf(value + 0.5f);
        }
        if (fabsf(value - slider->value) > 0.0001f) {
            slider->value = value;
            ui->needs_redraw = true;
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
