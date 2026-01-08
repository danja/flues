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

#define DISYN_URI "https://danja.github.io/flues/plugins/disyn"
#define DISYN_UI_URI DISYN_URI "#ui"
#define LOG_PREFIX "[Disyn UI] "

#define DEFAULT_WINDOW_WIDTH 760
#define DEFAULT_WINDOW_HEIGHT 420

#define GROUP_PADDING 16
#define GROUP_GAP_X 18
#define GROUP_GAP_Y 26
#define TITLE_HEIGHT 20
#define KNOB_SIZE 120
#define KNOB_HEIGHT 108
#define KNOB_SPACING_X 16
#define KNOB_SPACING_Y 18

typedef enum {
    PORT_AUDIO_OUT_L = 0,
    PORT_AUDIO_OUT_R,
    PORT_MIDI_IN,
    PORT_ALGORITHM_TYPE,
    PORT_PARAM_1,
    PORT_PARAM_2,
    PORT_PARAM_3,
    PORT_ENVELOPE_ATTACK,
    PORT_ENVELOPE_RELEASE,
    PORT_REVERB_SIZE,
    PORT_REVERB_LEVEL,
    PORT_MASTER_GAIN,
    PORT_TOTAL_COUNT
} PortIndex;

typedef enum {
    GROUP_ALGO = 0,
    GROUP_ENVELOPE,
    GROUP_SPACE,
    GROUP_OUTPUT,
    GROUP_COUNT
} GroupIndex;

static const char* const kAlgorithmLabels[] = {
    "Dirichlet",
    "DSF 1",
    "DSF 2",
    "Tanh Sq",
    "Tanh Saw",
    "PAF",
    "Mod FM",
    "Hybrid",
    "Cascade",
    "Parallel",
    "Feedback",
    "Morph",
    "Inharm",
    "Adaptive",
    "Multi",
    "Asym",
    "Cross",
    "Taylor",
    "Trajectory"
};

static const char* const kParam1Labels[] = {
    "HARMONICS",   // 0 Dirichlet
    "DECAY",       // 1 DSF 1
    "DECAY",       // 2 DSF 2
    "DRIVE",       // 3 Tanh Sq
    "DRIVE",       // 4 Tanh Saw
    "FORMANT",     // 5 PAF
    "INDEX",       // 6 Mod FM
    "INDEX",       // 7 Hybrid
    "DECAY",       // 8 Cascade
    "INDEX",       // 9 Parallel
    "INDEX",       // 10 Feedback
    "MORPH",       // 11 Morph
    "DECAY",       // 12 Inharm
    "CUTOFF",      // 13 Adaptive
    "DRIVE",       // 14 Multi
    "LOW ASYM",    // 15 Asym
    "DSF->FM",     // 16 Cross
    "TERMS 1",     // 17 Taylor
    "SIDES"        // 18 Trajectory
};

static const char* const kParam2Labels[] = {
    "TILT",        // 0 Dirichlet
    "RATIO",       // 1 DSF 1
    "RATIO",       // 2 DSF 2
    "TRIM",        // 3 Tanh Sq
    "BLEND",       // 4 Tanh Saw
    "BANDWIDTH",   // 5 PAF
    "RATIO",       // 6 Mod FM
    "RESERVED",    // 7 Hybrid
    "ASYM R",      // 8 Cascade
    "RESERVED",    // 9 Parallel
    "FEEDBACK",    // 10 Feedback
    "CHARACTER",   // 11 Morph
    "SHIFT",       // 12 Inharm
    "RESON",       // 13 Adaptive
    "EXP DEPTH",   // 14 Multi
    "HIGH ASYM",   // 15 Asym
    "FM->DSF",     // 16 Cross
    "TERMS 2",     // 17 Taylor
    "LAUNCH"       // 18 Trajectory
};

static const char* const kParam3Labels[] = {
    "SHAPE",       // 0 Dirichlet
    "SINE MIX",    // 1 DSF 1
    "BALANCE",     // 2 DSF 2
    "BIAS",        // 3 Tanh Sq
    "EDGE",        // 4 Tanh Saw
    "DEPTH",       // 5 PAF
    "FEEDBACK",    // 6 Mod FM
    "SPACING",     // 7 Hybrid
    "DRIVE",       // 8 Cascade
    "MIX",         // 9 Parallel
    "DRIVE",       // 10 Feedback
    "CURVE",       // 11 Morph
    "MIX",         // 12 Inharm
    "MIX",         // 13 Adaptive
    "RING",        // 14 Multi
    "INDEX",       // 15 Asym
    "MIX",         // 16 Cross
    "BLEND",       // 17 Taylor
    "JITTER"       // 18 Trajectory
};

typedef struct {
    GroupIndex group;
    const char* label;
    uint32_t port;
    float min;
    float max;
    float def;
    uint32_t steps;
    const char* const* scale_labels;
    uint32_t scale_count;
} ControlDesc;

static const ControlDesc kControlInfo[] = {
    { GROUP_ALGO, "ALGORITHM", PORT_ALGORITHM_TYPE, 0.0f, 18.0f, 3.0f, 19, kAlgorithmLabels, 19 },
    { GROUP_ALGO, "SIDES", PORT_PARAM_1, 0.0f, 1.0f, 0.55f, 0, NULL, 0 },
    { GROUP_ALGO, "LAUNCH", PORT_PARAM_2, 0.0f, 1.0f, 0.50f, 0, NULL, 0 },
    { GROUP_ALGO, "JITTER", PORT_PARAM_3, 0.0f, 1.0f, 0.00f, 0, NULL, 0 },

    { GROUP_ENVELOPE, "ATTACK", PORT_ENVELOPE_ATTACK, 0.0f, 1.0f, 0.50f, 0, NULL, 0 },
    { GROUP_ENVELOPE, "RELEASE", PORT_ENVELOPE_RELEASE, 0.0f, 1.0f, 0.50f, 0, NULL, 0 },

    { GROUP_SPACE, "REVERB SIZE", PORT_REVERB_SIZE, 0.0f, 1.0f, 0.50f, 0, NULL, 0 },
    { GROUP_SPACE, "REVERB LEVEL", PORT_REVERB_LEVEL, 0.0f, 1.0f, 0.30f, 0, NULL, 0 },

    { GROUP_OUTPUT, "MASTER", PORT_MASTER_GAIN, 0.0f, 1.0f, 0.80f, 0, NULL, 0 }
};

typedef struct {
    int row;
    int columns;
} GroupLayout;

static const GroupLayout kGroupLayout[GROUP_COUNT] = {
    [GROUP_ALGO] = { 0, 4 },
    [GROUP_ENVELOPE] = { 1, 2 },
    [GROUP_SPACE] = { 1, 2 },
    [GROUP_OUTPUT] = { 2, 1 }
};

static const GroupIndex kRowGroups[][3] = {
    { GROUP_ALGO, GROUP_COUNT, GROUP_COUNT },
    { GROUP_ENVELOPE, GROUP_SPACE, GROUP_COUNT },
    { GROUP_OUTPUT, GROUP_COUNT, GROUP_COUNT }
};

typedef struct {
    uint32_t port;
    const char* label;
    float min;
    float max;
    float def;
    float value;
    uint32_t steps;
    const char* const* scale_labels;
    uint32_t scale_count;
    int x;
    int y;
    int width;
    int height;
    bool is_dropdown;
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

    int width;
    int height;
    int content_width;
    int content_height;

    Knob knobs[PORT_TOTAL_COUNT];
    bool knob_used[PORT_TOTAL_COUNT];

    GroupState groups[GROUP_COUNT];

    volatile bool needs_redraw;
    int active_knob;
    int dropdown_port;
    bool dropdown_open;
    double drag_start_y;
    float drag_start_value;
} DisynUI;

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

static void setup_layout(DisynUI* ui, int available_width);
static void update_param_labels(DisynUI* ui, uint32_t algorithm_index);

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

typedef struct {
    double x;
    double y;
    double w;
    double h;
} Rect;

static Rect dropdown_box_rect(const Knob* knob) {
    const double inset = 8.0;
    const double box_height = 26.0;
    Rect rect = {
        .x = knob->x + inset,
        .y = knob->y + 12.0,
        .w = knob->width - inset * 2.0,
        .h = box_height
    };
    return rect;
}

static Rect dropdown_list_rect(const Knob* knob) {
    const double item_height = 18.0;
    Rect box = dropdown_box_rect(knob);
    Rect list = {
        .x = box.x,
        .y = box.y + box.h + 4.0,
        .w = box.w,
        .h = item_height * knob->scale_count
    };
    return list;
}

static bool point_in_rect(int x, int y, Rect rect) {
    return x >= rect.x && x <= rect.x + rect.w &&
           y >= rect.y && y <= rect.y + rect.h;
}

static uint32_t knob_value_index(const Knob* knob) {
    if (knob->steps <= 1) {
        return 0;
    }
    float step = (knob->max - knob->min) / (float)(knob->steps - 1);
    uint32_t idx = (uint32_t)roundf((knob->value - knob->min) / step);
    if (idx >= knob->scale_count) {
        idx = knob->scale_count - 1;
    }
    return idx;
}

static void draw_group_background(cairo_t* cr, const GroupState* group, const char* title) {
    const double x = group->x;
    const double y = group->y;
    const double w = group->width;
    const double h = group->height;

    cairo_save(cr);
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source_rgb(cr, 0.14, 0.15, 0.19);
    cairo_fill(cr);

    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source_rgb(cr, 0.30, 0.31, 0.37);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12.0);
    cairo_set_source_rgb(cr, 0.95, 0.82, 0.46);
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

    cairo_set_source_rgb(cr, 0.10, 0.11, 0.13);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);

    cairo_arc(cr, cx, cy, radius, 0, 2 * M_PI);
    cairo_set_source_rgb(cr, 0.16, 0.18, 0.22);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, 2.0);
    cairo_set_source_rgb(cr, 0.82, 0.48, 0.18);
    cairo_stroke(cr);

    cairo_arc(cr, cx, cy, radius * 0.72, 0, 2 * M_PI);
    cairo_set_source_rgb(cr, 0.21, 0.23, 0.28);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 0.84, 0.64, 0.36, 0.55);
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

    double norm = (knob->value - knob->min) / (knob->max - knob->min);
    double angle = (norm * 1.5 * M_PI) + (0.75 * M_PI);
    double indicator_outer = radius * 0.88;
    double indicator_inner = radius * 0.22;

    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_width(cr, 4.0);
    cairo_set_source_rgb(cr, 0.97, 0.63, 0.26);
    cairo_move_to(cr,
                  cx + cos(angle) * indicator_inner,
                  cy + sin(angle) * indicator_inner);
    cairo_line_to(cr,
                  cx + cos(angle) * indicator_outer,
                  cy + sin(angle) * indicator_outer);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 0.90, 0.86, 0.74);
    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);

    char value_str[48];
    if (knob->scale_labels && knob->scale_count > 0) {
        uint32_t idx = 0;
        if (knob->steps > 1) {
            float step = (knob->max - knob->min) / (float)(knob->steps - 1);
            idx = (uint32_t)roundf((knob->value - knob->min) / step);
            if (idx >= knob->scale_count) {
                idx = knob->scale_count - 1;
            }
        }
        snprintf(value_str, sizeof value_str, "%s", knob->scale_labels[idx]);
    } else if (knob->steps > 1 && (knob->max - knob->min) <= 12.0f) {
        snprintf(value_str, sizeof value_str, "%.0f", knob->value);
    } else {
        snprintf(value_str, sizeof value_str, "%.2f", knob->value);
    }

    cairo_text_extents_t extents;
    cairo_text_extents(cr, value_str, &extents);
    cairo_move_to(cr, cx - extents.width / 2.0, cy + radius * 0.46);
    cairo_show_text(cr, value_str);

    cairo_set_source_rgb(cr, 0.74, 0.69, 0.60);
    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);
    cairo_text_extents(cr, knob->label, &extents);
    cairo_move_to(cr, cx - extents.width / 2.0, y + h - 7.0);
    cairo_show_text(cr, knob->label);

    cairo_restore(cr);
}

static void draw_dropdown(cairo_t* cr, const Knob* knob, const DisynUI* ui) {
    const Rect box = dropdown_box_rect(knob);
    const double label_y = knob->y + knob->height - 7.0;

    cairo_save(cr);
    cairo_rectangle(cr, knob->x, knob->y, knob->width, knob->height);
    cairo_clip(cr);

    cairo_set_source_rgb(cr, 0.10, 0.11, 0.13);
    cairo_rectangle(cr, knob->x, knob->y, knob->width, knob->height);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.16, 0.18, 0.22);
    cairo_rectangle(cr, box.x, box.y, box.w, box.h);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.82, 0.48, 0.18);
    cairo_set_line_width(cr, 1.4);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 0.90, 0.86, 0.74);
    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);

    uint32_t idx = knob_value_index(knob);
    const char* text = (knob->scale_labels && idx < knob->scale_count)
        ? knob->scale_labels[idx]
        : "";

    cairo_text_extents_t extents;
    cairo_text_extents(cr, text, &extents);
    cairo_move_to(cr, box.x + 8.0, box.y + box.h - 8.0);
    cairo_show_text(cr, text);

    cairo_set_source_rgb(cr, 0.85, 0.72, 0.36);
    cairo_move_to(cr, box.x + box.w - 16.0, box.y + box.h / 2.0 - 2.0);
    cairo_line_to(cr, box.x + box.w - 8.0, box.y + box.h / 2.0 - 2.0);
    cairo_line_to(cr, box.x + box.w - 12.0, box.y + box.h / 2.0 + 4.0);
    cairo_close_path(cr);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.74, 0.69, 0.60);
    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);
    cairo_text_extents(cr, knob->label, &extents);
    cairo_move_to(cr, knob->x + knob->width / 2.0 - extents.width / 2.0, label_y);
    cairo_show_text(cr, knob->label);

    if (ui->dropdown_open && ui->dropdown_port == (int)knob->port && knob->scale_labels) {
        const Rect list = dropdown_list_rect(knob);
        const double item_height = 18.0;

        cairo_set_source_rgb(cr, 0.14, 0.15, 0.19);
        cairo_rectangle(cr, list.x, list.y, list.w, list.h);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0.30, 0.31, 0.37);
        cairo_set_line_width(cr, 1.1);
        cairo_stroke(cr);

        for (uint32_t i = 0; i < knob->scale_count; ++i) {
            double item_y = list.y + i * item_height;
            if (i == idx) {
                cairo_set_source_rgb(cr, 0.28, 0.26, 0.20);
                cairo_rectangle(cr, list.x, item_y, list.w, item_height);
                cairo_fill(cr);
            }

            cairo_set_source_rgb(cr, 0.92, 0.88, 0.74);
            cairo_move_to(cr, list.x + 8.0, item_y + item_height - 5.0);
            cairo_show_text(cr, knob->scale_labels[i]);
        }
    }

    cairo_restore(cr);
}

static void draw_ui(DisynUI* ui) {
    pthread_mutex_lock(&ui->mutex);
    if (!ui->surface) {
        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    cairo_t* cr = cairo_create(ui->surface);

    cairo_rectangle(cr, 0, 0, ui->width, ui->height);
    cairo_set_source_rgb(cr, 0.06, 0.07, 0.10);
    cairo_fill(cr);

    static const char* kGroupTitles[GROUP_COUNT] = {
        [GROUP_ALGO] = "Algorithm & Timbre",
        [GROUP_ENVELOPE] = "Envelope",
        [GROUP_SPACE] = "Reverb",
        [GROUP_OUTPUT] = "Output"
    };

    for (int g = 0; g < GROUP_COUNT; ++g) {
        draw_group_background(cr, &ui->groups[g], kGroupTitles[g]);
    }

    for (int port = 0; port < PORT_TOTAL_COUNT; ++port) {
        if (!ui->knob_used[port]) {
            continue;
        }
        if (ui->knobs[port].is_dropdown) {
            draw_dropdown(cr, &ui->knobs[port], ui);
        } else {
            draw_knob(cr, &ui->knobs[port]);
        }
    }

    cairo_destroy(cr);
    cairo_surface_flush(ui->surface);
    XFlush(ui->display);
    ui->needs_redraw = false;
    pthread_mutex_unlock(&ui->mutex);
}

static int find_knob_at(DisynUI* ui, int x, int y) {
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

static void notify_host(DisynUI* ui, uint32_t port, float value) {
    if (ui->write) {
        ui->write(ui->controller, port, sizeof(float), 0, &value);
    }
}

static void handle_button_press(DisynUI* ui, const XButtonEvent* event) {
    if (event->button != Button1) {
        return;
    }
    pthread_mutex_lock(&ui->mutex);
    if (ui->dropdown_open && ui->dropdown_port >= 0 &&
        ui->dropdown_port < (int)PORT_TOTAL_COUNT &&
        ui->knob_used[ui->dropdown_port]) {
        Knob* dropdown = &ui->knobs[ui->dropdown_port];
        Rect list = dropdown_list_rect(dropdown);
        if (point_in_rect(event->x, event->y, list)) {
            const double item_height = 18.0;
            int index = (int)((event->y - list.y) / item_height);
            if (index >= 0 && index < (int)dropdown->scale_count) {
                float step = (dropdown->max - dropdown->min) / (float)(dropdown->steps - 1);
                float value = dropdown->min + step * (float)index;
                value = clamp_value(dropdown, value);
                if (fabsf(value - dropdown->value) > 0.0001f) {
                    dropdown->value = value;
                    ui->needs_redraw = true;
                    update_param_labels(ui, (uint32_t)index);
                    notify_host(ui, dropdown->port, dropdown->value);
                }
            }
            ui->dropdown_open = false;
            pthread_mutex_unlock(&ui->mutex);
            return;
        }
    }

    int knob_index = find_knob_at(ui, event->x, event->y);
    if (knob_index >= 0) {
        Knob* knob = &ui->knobs[knob_index];
        if (knob->is_dropdown) {
            if (!ui->dropdown_open || ui->dropdown_port != knob_index) {
                ui->dropdown_open = true;
                ui->dropdown_port = knob_index;
            } else {
                ui->dropdown_open = false;
            }
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
            return;
        }
        ui->dropdown_open = false;
        ui->active_knob = knob_index;
        ui->drag_start_y = event->y;
        ui->drag_start_value = ui->knobs[knob_index].value;
    } else if (ui->dropdown_open) {
        ui->dropdown_open = false;
        ui->needs_redraw = true;
    }
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_button_release(DisynUI* ui, const XButtonEvent* event) {
    (void)event;
    pthread_mutex_lock(&ui->mutex);
    ui->active_knob = -1;
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_scroll(DisynUI* ui, const XButtonEvent* event) {
    int knob_index = find_knob_at(ui, event->x, event->y);
    if (knob_index < 0) {
        return;
    }

    pthread_mutex_lock(&ui->mutex);
    Knob* knob = &ui->knobs[knob_index];
    if (knob->is_dropdown) {
        float step = (knob->max - knob->min) / (float)(knob->steps - 1);
        float value = knob->value + ((event->button == Button4) ? step : -step);
        value = clamp_value(knob, value);
        if (fabsf(value - knob->value) > 0.0001f) {
            knob->value = value;
            ui->needs_redraw = true;
            update_param_labels(ui, knob_value_index(knob));
            notify_host(ui, knob->port, knob->value);
        }
        pthread_mutex_unlock(&ui->mutex);
        return;
    }
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

static void handle_motion(DisynUI* ui, const XMotionEvent* event) {
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

static void process_x_event(DisynUI* ui, XEvent* event) {
    switch (event->type) {
        case Expose:
            pthread_mutex_lock(&ui->mutex);
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
            break;
        case ConfigureNotify: {
            pthread_mutex_lock(&ui->mutex);
            if (event->xconfigure.width != ui->width ||
                event->xconfigure.height != ui->height) {
                ui->width = event->xconfigure.width;
                ui->height = event->xconfigure.height;
                cairo_xlib_surface_set_size(ui->surface, ui->width, ui->height);
                setup_layout(ui, ui->width - 40);
                ui->needs_redraw = true;
            }
            pthread_mutex_unlock(&ui->mutex);
            break;
        }
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
    DisynUI* ui = (DisynUI*)data;
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

static void setup_layout(DisynUI* ui, int available_width) {
    memset(ui->groups, 0, sizeof(ui->groups));
    memset(ui->knob_used, 0, sizeof(ui->knob_used));

    const int row_count = 3;
    int row_heights[3] = {0};
    int row_widths[3] = {0};
    int row_counts[3] = {0};

    for (int g = 0; g < GROUP_COUNT; ++g) {
        ui->groups[g].row = kGroupLayout[g].row;
        ui->groups[g].columns = kGroupLayout[g].columns;
    }

    for (size_t i = 0; i < sizeof(kControlInfo) / sizeof(kControlInfo[0]); ++i) {
        const ControlDesc* desc = &kControlInfo[i];
        ui->groups[desc->group].count++;
    }

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

    int max_row_width = 0;
    for (int row = 0; row < row_count; ++row) {
        int width = 0;
        int count = 0;
        for (int idx = 0; kRowGroups[row][idx] != GROUP_COUNT; ++idx) {
            GroupIndex group_index = kRowGroups[row][idx];
            width += ui->groups[group_index].width;
            count++;
        }
        if (count > 0) {
            width += (count - 1) * GROUP_GAP_X;
        }
        row_widths[row] = width;
        row_counts[row] = count;
        if (width > max_row_width) {
            max_row_width = width;
        }
    }

    int current_y = 20;
    for (int row = 0; row < row_count; ++row) {
        if (row_counts[row] == 0) {
            continue;
        }
        int row_width = row_widths[row];
        int start_x = (available_width - row_width) / 2;
        if (start_x < 20) {
            start_x = 20;
        }
        int current_x = start_x;
        for (int idx = 0; kRowGroups[row][idx] != GROUP_COUNT; ++idx) {
            GroupIndex group_index = kRowGroups[row][idx];
            GroupState* group = &ui->groups[group_index];
            group->x = current_x;
            group->y = current_y;
            current_x += group->width + GROUP_GAP_X;
        }
        current_y += row_heights[row];
        if (row < row_count - 1 && row_counts[row + 1] > 0) {
            current_y += GROUP_GAP_Y;
        }
    }

    ui->content_width = max_row_width + 40;
    ui->content_height = current_y + 20;

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
        knob->scale_labels = desc->scale_labels;
        knob->scale_count = desc->scale_count;
        knob->width = KNOB_SIZE;
        knob->height = KNOB_HEIGHT;
        knob->x = group->x + GROUP_PADDING + col * (KNOB_SIZE + KNOB_SPACING_X);
        knob->y = group->y + GROUP_PADDING + TITLE_HEIGHT + row * (KNOB_HEIGHT + KNOB_SPACING_Y);
        knob->is_dropdown = (desc->port == PORT_ALGORITHM_TYPE);
        ui->knob_used[desc->port] = true;
    }

    update_param_labels(ui, knob_value_index(&ui->knobs[PORT_ALGORITHM_TYPE]));
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

    if (strcmp(plugin_uri, DISYN_URI) != 0) {
        fprintf(stderr, LOG_PREFIX "Plugin URI mismatch (%s)\n", plugin_uri);
        return NULL;
    }

    ensure_xlib_threads();

    DisynUI* ui = calloc(1, sizeof(DisynUI));
    if (!ui) {
        return NULL;
    }

    pthread_mutex_init(&ui->mutex, NULL);
    ui->write = write_function;
    ui->controller = controller;
    ui->active_knob = -1;
    ui->dropdown_port = -1;
    ui->dropdown_open = false;
    ui->needs_redraw = true;
    ui->content_width = 0;
    ui->content_height = 0;

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

    int target_width = DEFAULT_WINDOW_WIDTH;
    int target_height = DEFAULT_WINDOW_HEIGHT;

    setup_layout(ui, target_width);
    if (ui->content_width > target_width) {
        target_width = ui->content_width;
        setup_layout(ui, target_width);
    }
    if (ui->content_height > target_height) {
        target_height = ui->content_height;
    }

    ui->width = target_width;
    ui->height = target_height;

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

    XStoreName(display, ui->window, "Disyn");
    XMapWindow(display, ui->window);
    XFlush(display);

    ui->surface = cairo_xlib_surface_create(
        display,
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

    cairo_xlib_surface_set_size(ui->surface, ui->width, ui->height);

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
    DisynUI* ui = (DisynUI*)handle;
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
    DisynUI* ui = (DisynUI*)handle;
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
        if (port_index == PORT_ALGORITHM_TYPE) {
            update_param_labels(ui, knob_value_index(knob));
        }
    }
    pthread_mutex_unlock(&ui->mutex);
}

static void update_param_labels(DisynUI* ui, uint32_t algorithm_index) {
    if (algorithm_index >= (sizeof(kParam1Labels) / sizeof(kParam1Labels[0]))) {
        return;
    }
    if (ui->knob_used[PORT_PARAM_1]) {
        ui->knobs[PORT_PARAM_1].label = kParam1Labels[algorithm_index];
    }
    if (ui->knob_used[PORT_PARAM_2]) {
        ui->knobs[PORT_PARAM_2].label = kParam2Labels[algorithm_index];
    }
    if (ui->knob_used[PORT_PARAM_3]) {
        ui->knobs[PORT_PARAM_3].label = kParam3Labels[algorithm_index];
    }
}

static const void* ui_extension_data(const char* uri) {
    (void)uri;
    return NULL;
}

static const LV2UI_Descriptor ui_descriptor = {
    DISYN_UI_URI,
    ui_instantiate,
    ui_cleanup,
    ui_port_event,
    ui_extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return (index == 0) ? &ui_descriptor : NULL;
}
