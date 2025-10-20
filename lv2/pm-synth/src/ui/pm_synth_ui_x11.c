#include <lv2/ui/ui.h>

#include <X11/Xlib.h>
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

#define PMSYNTH_URI "https://danja.github.io/flues/plugins/pm-synth"
#define PMSYNTH_UI_URI PMSYNTH_URI "#ui"
#define LOG_PREFIX "[PM-Synth UI] "

#define WINDOW_WIDTH 900
#define WINDOW_HEIGHT 460

#define GROUP_PADDING 16
#define GROUP_GAP_X 16
#define GROUP_GAP_Y 28
#define TITLE_HEIGHT 22
#define KNOB_SIZE 96
#define KNOB_HEIGHT 110
#define KNOB_SPACING_X 18
#define KNOB_SPACING_Y 18

typedef enum {
    PORT_AUDIO_OUT = 0,
    PORT_MIDI_IN,
    PORT_DC_LEVEL,
    PORT_NOISE_LEVEL,
    PORT_TONE_LEVEL,
    PORT_ATTACK,
    PORT_RELEASE,
    PORT_INTERFACE_TYPE,
    PORT_INTERFACE_INTENSITY,
    PORT_TUNING,
    PORT_RATIO,
    PORT_DELAY1_FEEDBACK,
    PORT_DELAY2_FEEDBACK,
    PORT_FILTER_FEEDBACK,
    PORT_FILTER_FREQUENCY,
    PORT_FILTER_Q,
    PORT_FILTER_SHAPE,
    PORT_LFO_FREQUENCY,
    PORT_MOD_TYPE_LEVEL,
    PORT_REVERB_SIZE,
    PORT_REVERB_LEVEL,
    PORT_TOTAL_COUNT
} PortIndex;

typedef enum {
    GROUP_STEAM = 0,
    GROUP_INTERFACE,
    GROUP_ENVELOPE,
    GROUP_PIPE,
    GROUP_FILTER,
    GROUP_MODULATION,
    GROUP_REVERB,
    GROUP_COUNT
} GroupIndex;

typedef struct {
    GroupIndex group;
    const char* label;
    uint32_t port;
    float min;
    float max;
    float def;
    uint32_t steps;
} ControlDesc;

static const ControlDesc kControlInfo[] = {
    { GROUP_STEAM, "DC LEVEL", PORT_DC_LEVEL, 0.0f, 1.0f, 0.5f, 0 },
    { GROUP_STEAM, "NOISE", PORT_NOISE_LEVEL, 0.0f, 1.0f, 0.15f, 0 },
    { GROUP_STEAM, "TONE", PORT_TONE_LEVEL, 0.0f, 1.0f, 0.0f, 0 },

    { GROUP_ENVELOPE, "ATTACK", PORT_ATTACK, 0.0f, 1.0f, 0.33333334f, 0 },
    { GROUP_ENVELOPE, "RELEASE", PORT_RELEASE, 0.0f, 1.0f, 0.2821703f, 0 },

    { GROUP_INTERFACE, "TYPE", PORT_INTERFACE_TYPE, 0.0f, 11.0f, 2.0f, 12 },
    { GROUP_INTERFACE, "INTENSITY", PORT_INTERFACE_INTENSITY, 0.0f, 1.0f, 0.5f, 0 },

    { GROUP_PIPE, "TUNING", PORT_TUNING, 0.0f, 1.0f, 0.5f, 0 },
    { GROUP_PIPE, "RATIO", PORT_RATIO, 0.0f, 1.0f, 0.5f, 0 },
    { GROUP_PIPE, "DELAY 1 FB", PORT_DELAY1_FEEDBACK, 0.0f, 1.0f, 0.95959598f, 0 },
    { GROUP_PIPE, "DELAY 2 FB", PORT_DELAY2_FEEDBACK, 0.0f, 1.0f, 0.95959598f, 0 },

    { GROUP_FILTER, "FILTER FB", PORT_FILTER_FEEDBACK, 0.0f, 1.0f, 0.0f, 0 },
    { GROUP_FILTER, "FILTER FREQ", PORT_FILTER_FREQUENCY, 0.0f, 1.0f, 0.56632334f, 0 },
    { GROUP_FILTER, "FILTER Q", PORT_FILTER_Q, 0.0f, 1.0f, 0.18790182f, 0 },
    { GROUP_FILTER, "FILTER SHAPE", PORT_FILTER_SHAPE, 0.0f, 1.0f, 0.0f, 0 },

    { GROUP_MODULATION, "LFO RATE", PORT_LFO_FREQUENCY, 0.0f, 1.0f, 0.73835194f, 0 },
    { GROUP_MODULATION, "AM ↔ FM", PORT_MOD_TYPE_LEVEL, 0.0f, 1.0f, 0.5f, 0 },

    { GROUP_REVERB, "SIZE", PORT_REVERB_SIZE, 0.0f, 1.0f, 0.5f, 0 },
    { GROUP_REVERB, "LEVEL", PORT_REVERB_LEVEL, 0.0f, 1.0f, 0.3f, 0 },
};

typedef struct {
    int row;
    int columns;
} GroupLayout;

static const GroupLayout kGroupLayout[GROUP_COUNT] = {
    [GROUP_STEAM] = { 0, 3 },
    [GROUP_INTERFACE] = { 0, 2 },
    [GROUP_ENVELOPE] = { 0, 2 },
    [GROUP_PIPE] = { 1, 2 },
    [GROUP_FILTER] = { 1, 2 },
    [GROUP_MODULATION] = { 1, 2 },
    [GROUP_REVERB] = { 2, 2 },
};

static const GroupIndex kRowGroups[][4] = {
    { GROUP_STEAM, GROUP_INTERFACE, GROUP_ENVELOPE, GROUP_COUNT },
    { GROUP_PIPE, GROUP_FILTER, GROUP_MODULATION, GROUP_COUNT },
    { GROUP_REVERB, GROUP_COUNT, GROUP_COUNT, GROUP_COUNT }
};

typedef struct {
    uint32_t port;
    const char* label;
    float min;
    float max;
    float def;
    float value;
    uint32_t steps;
    int x;
    int y;
    int width;
    int height;
} Knob;

typedef struct {
    int row;
    int columns;
    int count;
    int assigned;
    int rows;
    int x;
    int y;
    int width;
    int height;
} GroupState;

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

    Knob knobs[PORT_TOTAL_COUNT];
    bool knob_used[PORT_TOTAL_COUNT];

    GroupState groups[GROUP_COUNT];

    volatile bool needs_redraw;
    int active_knob;
    double drag_start_y;
    float drag_start_value;
} PMSynthUI;

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

static float clamp_value(const Knob* knob, float value) {
    if (value < knob->min) {
        value = knob->min;
    } else if (value > knob->max) {
        value = knob->max;
    }
    if (knob->steps > 1) {
        float step = (knob->max - knob->min) / (float)(knob->steps - 1);
        value = knob->min + roundf((value - knob->min) / step) * step;
    }
    return value;
}

static void draw_group_background(cairo_t* cr, const GroupState* group, const char* title) {
    const double x = group->x;
    const double y = group->y;
    const double w = group->width;
    const double h = group->height;

    cairo_save(cr);
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source_rgb(cr, 0.14, 0.15, 0.18);
    cairo_fill(cr);

    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source_rgb(cr, 0.24, 0.25, 0.30);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);

    // Title
    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12.0);
    cairo_set_source_rgb(cr, 0.94, 0.80, 0.48);
    cairo_move_to(cr, x + GROUP_PADDING, y + GROUP_PADDING + 10);
    cairo_show_text(cr, title);
    cairo_restore(cr);
}

static void draw_knob(cairo_t* cr, const Knob* knob) {
    const double x = knob->x;
    const double y = knob->y;
    const double w = knob->width;
    const double h = knob->height;
    const double padding = 8.0;
    const double diameter = w - (padding * 2.0);
    const double radius = diameter / 2.0;
    const double cx = x + w / 2.0;
    const double cy = y + h / 2.0 - 8.0;

    cairo_save(cr);
    cairo_rectangle(cr, x, y, w, h);
    cairo_clip(cr);

    // Background
    cairo_set_source_rgb(cr, 0.10, 0.11, 0.13);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);

    // Outer ring
    cairo_arc(cr, cx, cy, radius, 0, 2 * M_PI);
    cairo_set_source_rgb(cr, 0.13, 0.15, 0.18);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, 2.0);
    cairo_set_source_rgb(cr, 0.80, 0.48, 0.16);
    cairo_stroke(cr);

    // Inner circle
    cairo_arc(cr, cx, cy, radius * 0.72, 0, 2 * M_PI);
    cairo_set_source_rgb(cr, 0.18, 0.20, 0.24);
    cairo_fill(cr);

    // Ticks
    cairo_set_source_rgba(cr, 0.84, 0.64, 0.36, 0.5);
    cairo_set_line_width(cr, 1.5);
    uint32_t ticks = knob->steps > 1 ? knob->steps : 11;
    for (uint32_t i = 0; i < ticks; ++i) {
        double t = (double)i / (double)(ticks - 1);
        double angle = (1.5 * M_PI * t) + (0.75 * M_PI);
        double r_inner = radius * 0.82;
        double r_outer = radius * 0.92;
        double x1 = cx + cos(angle) * r_inner;
        double y1 = cy + sin(angle) * r_inner;
        double x2 = cx + cos(angle) * r_outer;
        double y2 = cy + sin(angle) * r_outer;
        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
    }
    cairo_stroke(cr);

    // Indicator
    double norm = (knob->value - knob->min) / (knob->max - knob->min);
    double angle = (norm * 1.5 * M_PI) + (0.75 * M_PI);
    double indicator_outer = radius * 0.88;
    double indicator_inner = radius * 0.20;

    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_width(cr, 4.0);
    cairo_set_source_rgb(cr, 0.96, 0.63, 0.24);
    cairo_move_to(cr,
                  cx + cos(angle) * indicator_inner,
                  cy + sin(angle) * indicator_inner);
    cairo_line_to(cr,
                  cx + cos(angle) * indicator_outer,
                  cy + sin(angle) * indicator_outer);
    cairo_stroke(cr);

    // Value text
    cairo_set_source_rgb(cr, 0.89, 0.85, 0.72);
    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    char value_str[32];
    if (knob->steps > 1 && (knob->max - knob->min) <= 12.0f) {
        snprintf(value_str, sizeof value_str, "%.0f", knob->value);
    } else {
        snprintf(value_str, sizeof value_str, "%.2f", knob->value);
    }
    cairo_text_extents_t extents;
    cairo_text_extents(cr, value_str, &extents);
    cairo_move_to(cr, cx - extents.width / 2.0, cy + radius * 0.45);
    cairo_show_text(cr, value_str);

    // Label
    cairo_set_source_rgb(cr, 0.72, 0.68, 0.58);
    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);
    cairo_text_extents(cr, knob->label, &extents);
    cairo_move_to(cr, cx - extents.width / 2.0, y + h - 8.0);
    cairo_show_text(cr, knob->label);

    cairo_restore(cr);
}

static void draw_ui(PMSynthUI* ui) {
    pthread_mutex_lock(&ui->mutex);
    if (!ui->surface) {
        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    cairo_t* cr = cairo_create(ui->surface);

    // Background
    cairo_rectangle(cr, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    cairo_set_source_rgb(cr, 0.07, 0.08, 0.11);
    cairo_fill(cr);

    static const char* kGroupTitles[GROUP_COUNT] = {
        [GROUP_STEAM] = "Steam",
        [GROUP_INTERFACE] = "Interface",
        [GROUP_ENVELOPE] = "Envelope",
        [GROUP_PIPE] = "Pipe & Delay",
        [GROUP_FILTER] = "Feedback & Filter",
        [GROUP_MODULATION] = "Modulation",
        [GROUP_REVERB] = "Reverb"
    };

    for (int g = 0; g < GROUP_COUNT; ++g) {
        draw_group_background(cr, &ui->groups[g], kGroupTitles[g]);
    }

    for (int port = 0; port < PORT_TOTAL_COUNT; ++port) {
        if (!ui->knob_used[port]) {
            continue;
        }
        draw_knob(cr, &ui->knobs[port]);
    }

    cairo_destroy(cr);
    cairo_surface_flush(ui->surface);
    XFlush(ui->display);
    ui->needs_redraw = false;
    pthread_mutex_unlock(&ui->mutex);
}

static int find_knob_at(PMSynthUI* ui, int x, int y) {
    for (int port = 0; port < PORT_TOTAL_COUNT; ++port) {
        if (!ui->knob_used[port]) {
            continue;
        }
        const Knob* knob = &ui->knobs[port];
        if (x >= knob->x && x <= knob->x + knob->width &&
            y >= knob->y && y <= knob->y + knob->height) {
            return port;
        }
    }
    return -1;
}

static void notify_host(PMSynthUI* ui, uint32_t port, float value) {
    if (ui->write) {
        ui->write(ui->controller, port, sizeof(float), 0, &value);
    }
}

static void handle_button_press(PMSynthUI* ui, const XButtonEvent* event) {
    if (event->button != Button1) {
        return;
    }
    pthread_mutex_lock(&ui->mutex);
    int knob_index = find_knob_at(ui, event->x, event->y);
    if (knob_index >= 0) {
        ui->active_knob = knob_index;
        ui->drag_start_y = event->y;
        ui->drag_start_value = ui->knobs[knob_index].value;
    }
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_button_release(PMSynthUI* ui, const XButtonEvent* event) {
    (void)event;
    pthread_mutex_lock(&ui->mutex);
    ui->active_knob = -1;
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_scroll(PMSynthUI* ui, const XButtonEvent* event) {
    int knob_index = find_knob_at(ui, event->x, event->y);
    if (knob_index < 0) {
        return;
    }

    pthread_mutex_lock(&ui->mutex);
    Knob* knob = &ui->knobs[knob_index];
    float step = (knob->max - knob->min) / 100.0f;
    float value = knob->value;
    if (event->button == Button4) {
        value += step * 4.0f;
    } else if (event->button == Button5) {
        value -= step * 4.0f;
    }
    value = clamp_value(knob, value);
    if (fabsf(value - knob->value) > 0.0001f) {
        knob->value = value;
        ui->needs_redraw = true;
        notify_host(ui, knob->port, knob->value);
    }
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_motion(PMSynthUI* ui, const XMotionEvent* event) {
    pthread_mutex_lock(&ui->mutex);
    int knob_index = ui->active_knob;
    if (knob_index >= 0 && ui->knob_used[knob_index]) {
        Knob* knob = &ui->knobs[knob_index];
        double delta = ui->drag_start_y - event->y;
        double sensitivity = (knob->max - knob->min) / 200.0;
        float value = clamp_value(knob, ui->drag_start_value + (float)(delta * sensitivity));
        if (fabsf(value - knob->value) > 0.0001f) {
            knob->value = value;
            ui->needs_redraw = true;
            notify_host(ui, knob->port, knob->value);
        }
    }
    pthread_mutex_unlock(&ui->mutex);
}

static void process_x_event(PMSynthUI* ui, XEvent* event) {
    switch (event->type) {
        case Expose:
            pthread_mutex_lock(&ui->mutex);
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
            break;
        case ButtonPress:
            if (event->xbutton.button == Button1) {
                handle_button_press(ui, &event->xbutton);
            } else if (event->xbutton.button == Button4 || event->xbutton.button == Button5) {
                handle_scroll(ui, &event->xbutton);
            }
            break;
        case ButtonRelease:
            if (event->xbutton.button == Button1) {
                handle_button_release(ui, &event->xbutton);
            }
            break;
        case MotionNotify:
            handle_motion(ui, &event->xmotion);
            break;
        default:
            break;
    }
}

static void* event_thread_main(void* data) {
    PMSynthUI* ui = (PMSynthUI*)data;
    while (ui->running) {
        while (XPending(ui->display) > 0) {
            XEvent event;
            XNextEvent(ui->display, &event);
            process_x_event(ui, &event);
        }

        if (ui->needs_redraw) {
            draw_ui(ui);
        }

        usleep(16000);
    }
    return NULL;
}

static void setup_layout(PMSynthUI* ui) {
    memset(ui->groups, 0, sizeof(ui->groups));

    for (int g = 0; g < GROUP_COUNT; ++g) {
        ui->groups[g].row = kGroupLayout[g].row;
        ui->groups[g].columns = kGroupLayout[g].columns;
    }

    for (size_t i = 0; i < sizeof(kControlInfo) / sizeof(kControlInfo[0]); ++i) {
        const ControlDesc* desc = &kControlInfo[i];
        ui->groups[desc->group].count++;
    }

    // Pre-compute group dimensions
    int row_heights[4] = {0};
    const int row_count = 3;
    for (int g = 0; g < GROUP_COUNT; ++g) {
        GroupState* group = &ui->groups[g];
        group->rows = (group->count + group->columns - 1) / group->columns;
        if (group->rows < 1) {
            group->rows = 1;
        }

        group->width = (GROUP_PADDING * 2) +
                       group->columns * KNOB_SIZE +
                       (group->columns - 1) * KNOB_SPACING_X;
        group->height = GROUP_PADDING + TITLE_HEIGHT +
                        group->rows * KNOB_HEIGHT +
                        (group->rows - 1) * KNOB_SPACING_Y +
                        GROUP_PADDING;

        if (group->height > row_heights[group->row]) {
            row_heights[group->row] = group->height;
        }
    }

    int current_y = 20;
    for (int row = 0; row < row_count; ++row) {
        int current_x = 20;
        for (int idx = 0; kRowGroups[row][idx] != GROUP_COUNT; ++idx) {
            GroupIndex group_index = kRowGroups[row][idx];
            GroupState* group = &ui->groups[group_index];
            group->x = current_x;
            group->y = current_y;
            current_x += group->width + GROUP_GAP_X;
        }
        current_y += row_heights[row] + GROUP_GAP_Y;
    }

    // Assign knob positions
    for (int g = 0; g < GROUP_COUNT; ++g) {
        ui->groups[g].assigned = 0;
    }

    for (size_t i = 0; i < sizeof(kControlInfo) / sizeof(kControlInfo[0]); ++i) {
        const ControlDesc* desc = &kControlInfo[i];
        GroupState* group = &ui->groups[desc->group];
        int index = group->assigned++;
        int col = index % group->columns;
        int row = index / group->columns;

        Knob* knob = &ui->knobs[desc->port];
        knob->port = desc->port;
        knob->label = desc->label;
        knob->min = desc->min;
        knob->max = desc->max;
        knob->def = desc->def;
        knob->value = desc->def;
        knob->steps = desc->steps;
        knob->width = KNOB_SIZE;
        knob->height = KNOB_HEIGHT;
        knob->x = group->x + GROUP_PADDING + col * (KNOB_SIZE + KNOB_SPACING_X);
        knob->y = group->y + GROUP_PADDING + TITLE_HEIGHT + row * (KNOB_HEIGHT + KNOB_SPACING_Y);
        ui->knob_used[desc->port] = true;
    }
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

    if (strcmp(plugin_uri, PMSYNTH_URI) != 0) {
        fprintf(stderr, LOG_PREFIX "Plugin URI mismatch (%s)\n", plugin_uri);
        return NULL;
    }

    ensure_xlib_threads();

    PMSynthUI* ui = calloc(1, sizeof(PMSynthUI));
    if (!ui) {
        return NULL;
    }

    pthread_mutex_init(&ui->mutex, NULL);
    ui->write = write_function;
    ui->controller = controller;
    ui->active_knob = -1;
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
    for (int i = 0; features && features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_UI__parent)) {
            parent = (Window)(uintptr_t)features[i]->data;
        }
    }

    XSetWindowAttributes attrs;
    attrs.background_pixel = BlackPixel(display, ui->screen);
    attrs.event_mask = ExposureMask |
                       StructureNotifyMask |
                       ButtonPressMask |
                       ButtonReleaseMask |
                       PointerMotionMask;

    ui->window = XCreateWindow(
        display,
        parent,
        0,
        0,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
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

    XStoreName(display, ui->window, "PM Synth");
    XMapWindow(display, ui->window);
    XFlush(display);

    ui->surface = cairo_xlib_surface_create(
        display,
        ui->window,
        DefaultVisual(display, ui->screen),
        WINDOW_WIDTH,
        WINDOW_HEIGHT);

    if (!ui->surface) {
        fprintf(stderr, LOG_PREFIX "Failed to create Cairo surface\n");
        XDestroyWindow(display, ui->window);
        XCloseDisplay(display);
        pthread_mutex_destroy(&ui->mutex);
        free(ui);
        return NULL;
    }

    setup_layout(ui);

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
    PMSynthUI* ui = (PMSynthUI*)handle;
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
    PMSynthUI* ui = (PMSynthUI*)handle;
    if (!ui || !buffer || format != 0 || buffer_size < sizeof(float)) {
        return;
    }
    if (port_index >= PORT_TOTAL_COUNT) {
        return;
    }
    if (!ui->knob_used[port_index]) {
        return;
    }

    float value = *((const float*)buffer);

    pthread_mutex_lock(&ui->mutex);
    Knob* knob = &ui->knobs[port_index];
    value = clamp_value(knob, value);
    if (fabsf(value - knob->value) > 0.0001f) {
        knob->value = value;
        ui->needs_redraw = true;
    }
    pthread_mutex_unlock(&ui->mutex);
}

static const void* ui_extension_data(const char* uri) {
    (void)uri;
    return NULL;
}

static const LV2UI_Descriptor ui_descriptor = {
    PMSYNTH_UI_URI,
    ui_instantiate,
    ui_cleanup,
    ui_port_event,
    ui_extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return (index == 0) ? &ui_descriptor : NULL;
}
