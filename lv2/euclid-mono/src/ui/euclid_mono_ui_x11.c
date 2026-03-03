#include <lv2/ui/ui.h>
#include <lv2/core/lv2.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#define PLUGIN_URI "https://danja.github.io/flues/plugins/euclid-mono"
#define UI_URI PLUGIN_URI "#ui"

typedef enum {
    PORT_CONTROL = 0,
    PORT_MIDI_OUT,
    PORT_STEPS_PER_BAR,
    PORT_SWING,
    PORT_SEED,
    PORT_MIDI_NOTE,
    PORT_A_BEATS,
    PORT_A_OFFSET,
    PORT_A_LENGTH,
    PORT_A_RANDOM,
    PORT_B_BEATS,
    PORT_B_OFFSET,
    PORT_B_LENGTH,
    PORT_B_RANDOM,
    PORT_INVERT_A,
    PORT_INVERT_B,
    PORT_LOGIC_OP,
    PORT_TOTAL_COUNT
} PortIndex;

typedef enum {
    GROUP_GLOBAL = 0,
    GROUP_LOGIC,
    GROUP_PATTERN_A,
    GROUP_PATTERN_B,
    GROUP_COUNT
} GroupIndex;

typedef enum {
    LOGIC_AND = 0,
    LOGIC_OR,
    LOGIC_XOR,
    LOGIC_NAND,
    LOGIC_NOR,
    LOGIC_XNOR,
    LOGIC_COUNT
} LogicOp;

#define GROUP_PADDING 8
#define GROUP_GAP_X 12
#define GROUP_GAP_Y 12
#define TITLE_HEIGHT 14
#define SLIDER_WIDTH 24
#define SLIDER_HEIGHT 120
#define SLIDER_VALUE_Y 12
#define SLIDER_LABEL_Y 26
#define SLIDER_TEXT_AREA_HEIGHT 30
#define SLIDER_GAP_X 12
#define RANDOM_BUTTON_WIDTH 18
#define RANDOM_BUTTON_HEIGHT 16
#define NOTE_BOX_WIDTH 66
#define NOTE_BOX_HEIGHT 20
#define NOTE_KNOB_RADIUS 26
#define TOGGLE_WIDTH 58
#define TOGGLE_HEIGHT 20
#define DROPDOWN_HEIGHT 22

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
    float def;
    float value;
    bool is_int;
    int x;
    int y;
    int width;
    int height;
} Slider;

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

typedef struct {
    GroupIndex group;
    bool used;
    int x;
    int y;
    int width;
    int height;
} RandomButton;

typedef struct {
    uint32_t port;
    const char* label;
    bool value;
    int x;
    int y;
    int width;
    int height;
} ToggleButton;

typedef struct {
    uint32_t port;
    int value;
    bool open;
    int item_height;
    int x;
    int y;
    int width;
    int height;
} Dropdown;

typedef struct {
    uint32_t port;
    int value;
    bool editing;
    char text[8];
    int x;
    int y;
    int width;
    int height;
} NoteBox;

typedef struct {
    uint32_t port;
    int min;
    int max;
    int value;
    int x;
    int y;
    int radius;
    bool active;
    int last_y;
} NoteKnob;

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

    GroupState groups[GROUP_COUNT];
    RandomButton random_buttons[GROUP_COUNT];

    ToggleButton invert_a;
    ToggleButton invert_b;
    Dropdown logic_dropdown;
    NoteBox note_box;
    NoteKnob note_knob;

    volatile bool needs_redraw;
    int active_slider;
    bool active_note_knob;
    uint32_t rand_state;
} EuclidMonoUI;

static const ControlDesc kControls[] = {
    { GROUP_GLOBAL, "STEPS", PORT_STEPS_PER_BAR, 8.0f, 24.0f, 16.0f, true },
    { GROUP_GLOBAL, "SWING", PORT_SWING, 0.0f, 1.0f, 0.0f, false },
    { GROUP_GLOBAL, "SEED", PORT_SEED, 0.0f, 65535.0f, 1.0f, true },

    { GROUP_PATTERN_A, "BEATS", PORT_A_BEATS, 0.0f, 24.0f, 4.0f, true },
    { GROUP_PATTERN_A, "OFFSET", PORT_A_OFFSET, 0.0f, 23.0f, 0.0f, true },
    { GROUP_PATTERN_A, "LENGTH", PORT_A_LENGTH, 1.0f, 24.0f, 16.0f, true },
    { GROUP_PATTERN_A, "RAND", PORT_A_RANDOM, 0.0f, 1.0f, 0.0f, false },

    { GROUP_PATTERN_B, "BEATS", PORT_B_BEATS, 0.0f, 24.0f, 3.0f, true },
    { GROUP_PATTERN_B, "OFFSET", PORT_B_OFFSET, 0.0f, 23.0f, 0.0f, true },
    { GROUP_PATTERN_B, "LENGTH", PORT_B_LENGTH, 1.0f, 24.0f, 16.0f, true },
    { GROUP_PATTERN_B, "RAND", PORT_B_RANDOM, 0.0f, 1.0f, 0.0f, false }
};

static const int kControlCount = sizeof(kControls) / sizeof(kControls[0]);

static const char* kGroupNames[GROUP_COUNT] = {
    "GLOBAL", "LOGIC", "PATTERN A", "PATTERN B"
};

static const char* kLogicNames[LOGIC_COUNT] = {
    "AND", "OR", "XOR", "NAND", "NOR", "XNOR"
};

static const GroupIndex kRowGroups[][2] = {
    { GROUP_GLOBAL, GROUP_LOGIC },
    { GROUP_PATTERN_A, GROUP_PATTERN_B }
};

static const int kRowCount = sizeof(kRowGroups) / sizeof(kRowGroups[0]);

static const int kGroupColumns[GROUP_COUNT] = {
    3,
    0,
    4,
    4
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

static float clamp_value(const Slider* slider, float value) {
    if (value < slider->min) return slider->min;
    if (value > slider->max) return slider->max;
    return value;
}

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static bool point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}

static void notify_host(EuclidMonoUI* ui, uint32_t port, float value) {
    if (ui->write) {
        ui->write(ui->controller, port, sizeof(float), 0, &value);
    }
}

static int slider_at(EuclidMonoUI* ui, int x, int y) {
    for (int i = 0; i < PORT_TOTAL_COUNT; ++i) {
        if (!ui->slider_used[i]) {
            continue;
        }
        const Slider* slider = &ui->sliders[i];
        if (point_in_rect(x, y, slider->x, slider->y, slider->width, slider->height + SLIDER_TEXT_AREA_HEIGHT)) {
            return i;
        }
    }
    return -1;
}

static int random_button_at(EuclidMonoUI* ui, int x, int y) {
    for (int g = 0; g < GROUP_COUNT; ++g) {
        const RandomButton* btn = &ui->random_buttons[g];
        if (!btn->used) {
            continue;
        }
        if (point_in_rect(x, y, btn->x, btn->y, btn->width, btn->height)) {
            return g;
        }
    }
    return -1;
}

static uint32_t next_u32(EuclidMonoUI* ui) {
    ui->rand_state = ui->rand_state * 1664525u + 1013904223u;
    return ui->rand_state;
}

static float random_unit(EuclidMonoUI* ui) {
    return (float)(next_u32(ui) & 0x00FFFFFFu) / 16777215.0f;
}

static int random_int(EuclidMonoUI* ui, int min_value, int max_value) {
    if (max_value <= min_value) {
        return min_value;
    }
    const int span = max_value - min_value + 1;
    return min_value + (int)(next_u32(ui) % (uint32_t)span);
}

static void set_slider_value(EuclidMonoUI* ui, Slider* slider, float value) {
    value = clamp_value(slider, value);
    if (slider->is_int) {
        value = floorf(value + 0.5f);
    }
    if (fabsf(value - slider->value) > 0.0001f) {
        slider->value = value;
        notify_host(ui, slider->port, value);
        ui->needs_redraw = true;
    }
}

static float slider_value_from_y(const Slider* slider, int y) {
    const int top = slider->y;
    const int bottom = slider->y + slider->height;
    float t = 1.0f - (float)(y - top) / (float)(bottom - top);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return slider->min + t * (slider->max - slider->min);
}

static void set_toggle_value(EuclidMonoUI* ui, ToggleButton* toggle, bool value) {
    if (toggle->value != value) {
        toggle->value = value;
        notify_host(ui, toggle->port, value ? 1.0f : 0.0f);
        ui->needs_redraw = true;
    }
}

static void set_logic_value(EuclidMonoUI* ui, int value) {
    value = clamp_int(value, 0, LOGIC_COUNT - 1);
    if (ui->logic_dropdown.value != value) {
        ui->logic_dropdown.value = value;
        notify_host(ui, ui->logic_dropdown.port, (float)value);
        ui->needs_redraw = true;
    }
}

static void set_note_value(EuclidMonoUI* ui, int value, bool notify) {
    value = clamp_int(value, 0, 127);
    if (ui->note_box.value != value) {
        ui->note_box.value = value;
        ui->note_knob.value = value;
        if (notify) {
            notify_host(ui, ui->note_box.port, (float)value);
        }
        ui->needs_redraw = true;
    }
}

static void reset_note_text(EuclidMonoUI* ui) {
    snprintf(ui->note_box.text, sizeof(ui->note_box.text), "%d", ui->note_box.value);
}

static void commit_note_edit(EuclidMonoUI* ui, bool cancel_only) {
    if (!ui->note_box.editing) {
        return;
    }
    if (!cancel_only) {
        if (ui->note_box.text[0] != '\0') {
            char* end = NULL;
            long parsed = strtol(ui->note_box.text, &end, 10);
            if (end && *end == '\0') {
                set_note_value(ui, (int)parsed, true);
            }
        }
    }
    ui->note_box.editing = false;
    reset_note_text(ui);
    ui->needs_redraw = true;
}

static void randomize_slider(EuclidMonoUI* ui, Slider* slider) {
    const float t = random_unit(ui);
    const float value = slider->min + t * (slider->max - slider->min);
    set_slider_value(ui, slider, value);
}

static void randomize_logic(EuclidMonoUI* ui) {
    set_toggle_value(ui, &ui->invert_a, random_int(ui, 0, 1) == 1);
    set_toggle_value(ui, &ui->invert_b, random_int(ui, 0, 1) == 1);
    set_logic_value(ui, random_int(ui, 0, LOGIC_COUNT - 1));
}

static void randomize_group(EuclidMonoUI* ui, GroupIndex group) {
    if (group == GROUP_LOGIC) {
        randomize_logic(ui);
        return;
    }

    for (int i = 0; i < kControlCount; ++i) {
        const ControlDesc* desc = &kControls[i];
        if (desc->group != group || !ui->slider_used[desc->port]) {
            continue;
        }
        randomize_slider(ui, &ui->sliders[desc->port]);
    }
}

static void draw_group_background(cairo_t* cr, const GroupState* group, const char* title) {
    cairo_rectangle(cr, group->x, group->y, group->width, group->height);
    cairo_set_source_rgb(cr, 0.14, 0.15, 0.19);
    cairo_fill(cr);

    cairo_rectangle(cr, group->x, group->y, group->width, group->height);
    cairo_set_source_rgb(cr, 0.30, 0.31, 0.37);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12.0);
    cairo_set_source_rgb(cr, 0.95, 0.82, 0.46);
    cairo_move_to(cr, group->x + GROUP_PADDING, group->y + GROUP_PADDING + 10);
    cairo_show_text(cr, title);
    cairo_new_path(cr);
}

static void draw_random_button(cairo_t* cr, const RandomButton* btn) {
    if (!btn->used) {
        return;
    }

    cairo_rectangle(cr, btn->x, btn->y, btn->width, btn->height);
    cairo_set_source_rgb(cr, 0.22, 0.24, 0.28);
    cairo_fill(cr);

    cairo_rectangle(cr, btn->x, btn->y, btn->width, btn->height);
    cairo_set_source_rgb(cr, 0.90, 0.72, 0.36);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.97, 0.95, 0.90);

    cairo_text_extents_t extents;
    cairo_text_extents(cr, "R", &extents);
    cairo_move_to(cr,
                  btn->x + (btn->width - extents.width) / 2.0 - extents.x_bearing,
                  btn->y + (btn->height - extents.height) / 2.0 - extents.y_bearing);
    cairo_show_text(cr, "R");
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
    cairo_move_to(cr, x + (w - extents.width) / 2.0, y + h + SLIDER_VALUE_Y);
    cairo_show_text(cr, value_str);

    cairo_text_extents(cr, slider->label, &extents);
    cairo_move_to(cr, x + (w - extents.width) / 2.0, y + h + SLIDER_LABEL_Y);
    cairo_show_text(cr, slider->label);

    cairo_new_path(cr);
}

static void draw_toggle(cairo_t* cr, const ToggleButton* toggle) {
    cairo_rectangle(cr, toggle->x, toggle->y, toggle->width, toggle->height);
    cairo_set_source_rgb(cr, 0.19, 0.20, 0.23);
    cairo_fill(cr);

    cairo_rectangle(cr, toggle->x, toggle->y, toggle->width, toggle->height);
    cairo_set_source_rgb(cr, toggle->value ? 0.35 : 0.28, toggle->value ? 0.76 : 0.30, toggle->value ? 0.40 : 0.34);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);

    cairo_text_extents_t ext;
    cairo_text_extents(cr, toggle->label, &ext);
    cairo_move_to(cr,
                  toggle->x + (toggle->width - ext.width) / 2.0 - ext.x_bearing,
                  toggle->y + (toggle->height - ext.height) / 2.0 - ext.y_bearing);
    cairo_show_text(cr, toggle->label);
    cairo_new_path(cr);
}

static void draw_dropdown(cairo_t* cr, const Dropdown* dropdown) {
    cairo_rectangle(cr, dropdown->x, dropdown->y, dropdown->width, dropdown->height);
    cairo_set_source_rgb(cr, 0.17, 0.18, 0.21);
    cairo_fill(cr);

    cairo_rectangle(cr, dropdown->x, dropdown->y, dropdown->width, dropdown->height);
    cairo_set_source_rgb(cr, 0.90, 0.72, 0.36);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);

    char label[32];
    snprintf(label, sizeof(label), "OP: %s", kLogicNames[dropdown->value]);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, label, &ext);
    cairo_move_to(cr,
                  dropdown->x + (dropdown->width - ext.width) / 2.0 - ext.x_bearing,
                  dropdown->y + (dropdown->height - ext.height) / 2.0 - ext.y_bearing);
    cairo_show_text(cr, label);

    if (dropdown->open) {
        const int menu_x = dropdown->x;
        const int menu_y = dropdown->y + dropdown->height;
        const int menu_h = dropdown->item_height * LOGIC_COUNT;

        cairo_rectangle(cr, menu_x, menu_y, dropdown->width, menu_h);
        cairo_set_source_rgb(cr, 0.14, 0.15, 0.18);
        cairo_fill(cr);

        cairo_rectangle(cr, menu_x, menu_y, dropdown->width, menu_h);
        cairo_set_source_rgb(cr, 0.90, 0.72, 0.36);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 10.0);

        for (int i = 0; i < LOGIC_COUNT; ++i) {
            const int iy = menu_y + i * dropdown->item_height;
            if (i == dropdown->value) {
                cairo_rectangle(cr, menu_x + 1, iy + 1, dropdown->width - 2, dropdown->item_height - 2);
                cairo_set_source_rgb(cr, 0.28, 0.33, 0.22);
                cairo_fill(cr);
            }

            cairo_set_source_rgb(cr, 0.93, 0.93, 0.93);
            cairo_text_extents(cr, kLogicNames[i], &ext);
            cairo_move_to(cr,
                          menu_x + 8,
                          iy + (dropdown->item_height - ext.height) / 2.0 - ext.y_bearing);
            cairo_show_text(cr, kLogicNames[i]);
        }
    }
    cairo_new_path(cr);
}

static void draw_note_box(cairo_t* cr, const NoteBox* note_box) {
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 9.0);
    cairo_set_source_rgb(cr, 0.85, 0.85, 0.85);
    cairo_move_to(cr, note_box->x, note_box->y - 2);
    cairo_show_text(cr, "NOTE");

    cairo_rectangle(cr, note_box->x, note_box->y, note_box->width, note_box->height);
    cairo_set_source_rgb(cr, note_box->editing ? 0.20 : 0.16, note_box->editing ? 0.24 : 0.17, 0.20);
    cairo_fill(cr);

    cairo_rectangle(cr, note_box->x, note_box->y, note_box->width, note_box->height);
    cairo_set_source_rgb(cr, note_box->editing ? 0.95 : 0.45, note_box->editing ? 0.82 : 0.48, 0.42);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);

    char text[12];
    if (note_box->editing) {
        snprintf(text, sizeof(text), "%s_", note_box->text);
    } else {
        snprintf(text, sizeof(text), "%d", note_box->value);
    }

    cairo_text_extents_t ext;
    cairo_text_extents(cr, text, &ext);
    cairo_move_to(cr,
                  note_box->x + (note_box->width - ext.width) / 2.0 - ext.x_bearing,
                  note_box->y + (note_box->height - ext.height) / 2.0 - ext.y_bearing);
    cairo_show_text(cr, text);
    cairo_new_path(cr);
}

static void draw_note_knob(cairo_t* cr, const NoteKnob* knob) {
    const double cx = (double)knob->x;
    const double cy = (double)knob->y;
    const double r = (double)knob->radius;
    const double t = (double)(knob->value - knob->min) / (double)(knob->max - knob->min);
    const double angle = (M_PI * 1.25) - (t * M_PI * 1.5);

    cairo_arc(cr, cx, cy, r, 0.0, 2.0 * M_PI);
    cairo_set_source_rgb(cr, 0.17, 0.18, 0.21);
    cairo_fill(cr);

    cairo_arc(cr, cx, cy, r, 0.0, 2.0 * M_PI);
    cairo_set_source_rgb(cr, 0.90, 0.72, 0.36);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);

    cairo_move_to(cr, cx, cy);
    cairo_line_to(cr, cx + cos(angle) * (r - 6.0), cy - sin(angle) * (r - 6.0));
    cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 9.0);
    cairo_set_source_rgb(cr, 0.85, 0.85, 0.85);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, "NOTE KNOB", &ext);
    cairo_move_to(cr, cx - ext.width / 2.0, cy + r + 14.0);
    cairo_show_text(cr, "NOTE KNOB");
    cairo_new_path(cr);
}

static void draw_ui(EuclidMonoUI* ui) {
    if (!ui->surface) return;

    cairo_t* cr = cairo_create(ui->surface);
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.12);
    cairo_paint(cr);

    for (int g = 0; g < GROUP_COUNT; ++g) {
        draw_group_background(cr, &ui->groups[g], kGroupNames[g]);
        draw_random_button(cr, &ui->random_buttons[g]);
    }

    for (int i = 0; i < PORT_TOTAL_COUNT; ++i) {
        if (ui->slider_used[i]) {
            draw_slider(cr, &ui->sliders[i]);
        }
    }

    draw_note_box(cr, &ui->note_box);
    draw_note_knob(cr, &ui->note_knob);
    draw_toggle(cr, &ui->invert_a);
    draw_toggle(cr, &ui->invert_b);
    draw_dropdown(cr, &ui->logic_dropdown);

    cairo_destroy(cr);
}

static void setup_layout(EuclidMonoUI* ui) {
    const int group_width = GROUP_PADDING * 2 + (SLIDER_WIDTH * 4) + (SLIDER_GAP_X * 3);
    const int group_height = GROUP_PADDING * 2 + TITLE_HEIGHT + SLIDER_HEIGHT + SLIDER_TEXT_AREA_HEIGHT + 8;

    for (int g = 0; g < GROUP_COUNT; ++g) {
        ui->groups[g].count = 0;
        ui->groups[g].assigned = 0;
        ui->groups[g].columns = kGroupColumns[g];
        ui->groups[g].row = 0;
        ui->groups[g].col = 0;

        ui->random_buttons[g].group = (GroupIndex)g;
        ui->random_buttons[g].used = true;
        ui->random_buttons[g].width = RANDOM_BUTTON_WIDTH;
        ui->random_buttons[g].height = RANDOM_BUTTON_HEIGHT;
    }

    for (int r = 0; r < kRowCount; ++r) {
        for (int c = 0; c < 2; ++c) {
            GroupIndex group = kRowGroups[r][c];
            ui->groups[group].row = r;
            ui->groups[group].col = c;
        }
    }

    for (int i = 0; i < kControlCount; ++i) {
        ui->groups[kControls[i].group].count++;
    }

    const int margin = 10;
    ui->width = margin * 2 + (group_width * 2) + GROUP_GAP_X;
    ui->height = margin * 2 + (group_height * 2) + GROUP_GAP_Y;

    for (int g = 0; g < GROUP_COUNT; ++g) {
        GroupState* group = &ui->groups[g];
        group->width = group_width;
        group->height = group_height;
        group->x = margin + group->col * (group_width + GROUP_GAP_X);
        group->y = margin + group->row * (group_height + GROUP_GAP_Y);

        ui->random_buttons[g].x = group->x + group->width - GROUP_PADDING - RANDOM_BUTTON_WIDTH;
        ui->random_buttons[g].y = group->y + GROUP_PADDING;
    }

    memset(ui->slider_used, 0, sizeof(ui->slider_used));

    for (int i = 0; i < kControlCount; ++i) {
        const ControlDesc* desc = &kControls[i];
        GroupState* group = &ui->groups[desc->group];
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
    }

    GroupState* logic = &ui->groups[GROUP_LOGIC];
    const int knob_diameter = NOTE_KNOB_RADIUS * 2;
    const int knob_column_w = knob_diameter + 12;
    const int logic_inner_x = logic->x + GROUP_PADDING;
    const int logic_controls_w = logic->width - GROUP_PADDING * 2 - knob_column_w - 6;

    ui->note_box.port = PORT_MIDI_NOTE;
    ui->note_box.value = 60;
    ui->note_box.editing = false;
    ui->note_box.width = logic_controls_w;
    ui->note_box.height = NOTE_BOX_HEIGHT;
    ui->note_box.x = logic_inner_x;
    ui->note_box.y = logic->y + GROUP_PADDING + TITLE_HEIGHT + 6;
    reset_note_text(ui);

    ui->note_knob.port = PORT_MIDI_NOTE;
    ui->note_knob.min = 0;
    ui->note_knob.max = 127;
    ui->note_knob.value = 60;
    ui->note_knob.radius = NOTE_KNOB_RADIUS;
    ui->note_knob.active = false;
    ui->note_knob.x = logic->x + logic->width - GROUP_PADDING - NOTE_KNOB_RADIUS;
    ui->note_knob.y = logic->y + GROUP_PADDING + TITLE_HEIGHT + NOTE_KNOB_RADIUS + 4;

    const int logic_inner_y = ui->note_box.y + ui->note_box.height + 10;

    ui->invert_a.port = PORT_INVERT_A;
    ui->invert_a.label = "INV A";
    ui->invert_a.value = false;
    ui->invert_a.x = logic_inner_x;
    ui->invert_a.y = logic_inner_y;
    ui->invert_a.width = logic_controls_w;
    ui->invert_a.height = TOGGLE_HEIGHT;

    ui->invert_b.port = PORT_INVERT_B;
    ui->invert_b.label = "INV B";
    ui->invert_b.value = false;
    ui->invert_b.x = logic_inner_x;
    ui->invert_b.y = logic_inner_y + TOGGLE_HEIGHT + 6;
    ui->invert_b.width = logic_controls_w;
    ui->invert_b.height = TOGGLE_HEIGHT;

    ui->logic_dropdown.port = PORT_LOGIC_OP;
    ui->logic_dropdown.value = LOGIC_AND;
    ui->logic_dropdown.open = false;
    ui->logic_dropdown.item_height = 18;
    ui->logic_dropdown.x = logic_inner_x;
    ui->logic_dropdown.y = ui->invert_b.y + TOGGLE_HEIGHT + 8;
    ui->logic_dropdown.width = logic_controls_w;
    ui->logic_dropdown.height = DROPDOWN_HEIGHT;
}

static void handle_motion(EuclidMonoUI* ui, XMotionEvent* motion) {
    if (ui->active_note_knob) {
        const int dy = ui->note_knob.last_y - motion->y;
        if (dy != 0) {
            set_note_value(ui, ui->note_knob.value + dy, true);
            ui->note_knob.last_y = motion->y;
        }
        return;
    }

    if (ui->active_slider < 0) {
        return;
    }
    Slider* slider = &ui->sliders[ui->active_slider];
    float value = slider_value_from_y(slider, motion->y);
    set_slider_value(ui, slider, value);
}

static void handle_button_press(EuclidMonoUI* ui, XButtonEvent* button) {
    if (button->button != Button1) {
        return;
    }

    const bool note_hit = point_in_rect(button->x, button->y,
                                        ui->note_box.x, ui->note_box.y,
                                        ui->note_box.width, ui->note_box.height);
    if (ui->note_box.editing && !note_hit) {
        commit_note_edit(ui, false);
    }

    if (ui->logic_dropdown.open) {
        const int menu_x = ui->logic_dropdown.x;
        const int menu_y = ui->logic_dropdown.y + ui->logic_dropdown.height;
        const int menu_h = ui->logic_dropdown.item_height * LOGIC_COUNT;
        if (point_in_rect(button->x, button->y, menu_x, menu_y, ui->logic_dropdown.width, menu_h)) {
            const int idx = (button->y - menu_y) / ui->logic_dropdown.item_height;
            set_logic_value(ui, idx);
            ui->logic_dropdown.open = false;
            ui->needs_redraw = true;
            return;
        }
        if (point_in_rect(button->x, button->y,
                          ui->logic_dropdown.x, ui->logic_dropdown.y,
                          ui->logic_dropdown.width, ui->logic_dropdown.height)) {
            ui->logic_dropdown.open = false;
            ui->needs_redraw = true;
            return;
        }
        ui->logic_dropdown.open = false;
        ui->needs_redraw = true;
    }

    const int random_group = random_button_at(ui, button->x, button->y);
    if (random_group >= 0) {
        ui->active_slider = -1;
        randomize_group(ui, (GroupIndex)random_group);
        return;
    }

    if (note_hit) {
        ui->note_box.editing = true;
        reset_note_text(ui);
        XSetInputFocus(ui->display, ui->window, RevertToParent, CurrentTime);
        ui->needs_redraw = true;
        return;
    }

    const int kdx = button->x - ui->note_knob.x;
    const int kdy = button->y - ui->note_knob.y;
    if ((kdx * kdx + kdy * kdy) <= (ui->note_knob.radius * ui->note_knob.radius)) {
        ui->active_note_knob = true;
        ui->note_knob.active = true;
        ui->note_knob.last_y = button->y;
        return;
    }

    if (point_in_rect(button->x, button->y,
                      ui->invert_a.x, ui->invert_a.y,
                      ui->invert_a.width, ui->invert_a.height)) {
        set_toggle_value(ui, &ui->invert_a, !ui->invert_a.value);
        return;
    }

    if (point_in_rect(button->x, button->y,
                      ui->invert_b.x, ui->invert_b.y,
                      ui->invert_b.width, ui->invert_b.height)) {
        set_toggle_value(ui, &ui->invert_b, !ui->invert_b.value);
        return;
    }

    if (point_in_rect(button->x, button->y,
                      ui->logic_dropdown.x, ui->logic_dropdown.y,
                      ui->logic_dropdown.width, ui->logic_dropdown.height)) {
        ui->logic_dropdown.open = true;
        ui->needs_redraw = true;
        return;
    }

    int index = slider_at(ui, button->x, button->y);
    if (index >= 0) {
        ui->active_slider = index;
        Slider* slider = &ui->sliders[index];
        float value = slider_value_from_y(slider, button->y);
        set_slider_value(ui, slider, value);
    }
}

static void handle_button_release(EuclidMonoUI* ui, XButtonEvent* button) {
    if (button->button == Button1) {
        ui->active_slider = -1;
        ui->active_note_knob = false;
        ui->note_knob.active = false;
    }
}

static void handle_key_press(EuclidMonoUI* ui, XKeyEvent* key) {
    if (!ui->note_box.editing) {
        return;
    }

    KeySym keysym = NoSymbol;
    char buf[8] = {0};
    int len = XLookupString(key, buf, sizeof(buf) - 1, &keysym, NULL);

    if (keysym == XK_Return || keysym == XK_KP_Enter) {
        commit_note_edit(ui, false);
        return;
    }
    if (keysym == XK_Escape) {
        commit_note_edit(ui, true);
        return;
    }
    if (keysym == XK_BackSpace || keysym == XK_Delete) {
        size_t n = strlen(ui->note_box.text);
        if (n > 0) {
            ui->note_box.text[n - 1] = '\0';
            ui->needs_redraw = true;
        }
        return;
    }

    if (len <= 0) {
        return;
    }

    for (int i = 0; i < len; ++i) {
        if (buf[i] < '0' || buf[i] > '9') {
            continue;
        }
        const size_t n = strlen(ui->note_box.text);
        if (n < 3) {
            ui->note_box.text[n] = buf[i];
            ui->note_box.text[n + 1] = '\0';
            ui->needs_redraw = true;
        }
    }
}

static void* event_thread_main(void* arg) {
    EuclidMonoUI* ui = (EuclidMonoUI*)arg;

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
                    handle_button_press(ui, &ev.xbutton);
                    break;
                case ButtonRelease:
                    handle_button_release(ui, &ev.xbutton);
                    break;
                case KeyPress:
                    handle_key_press(ui, &ev.xkey);
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

    EuclidMonoUI* ui = (EuclidMonoUI*)calloc(1, sizeof(EuclidMonoUI));
    if (!ui) {
        return NULL;
    }

    pthread_mutex_init(&ui->mutex, NULL);
    ui->write = write_function;
    ui->controller = controller;
    ui->active_slider = -1;
    ui->active_note_knob = false;
    ui->rand_state = (uint32_t)time(NULL) ^ 0x9E3779B9u;

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
    attrs.event_mask = ExposureMask | StructureNotifyMask |
                       ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                       KeyPressMask;
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
    pthread_create(&ui->thread, NULL, event_thread_main, ui);

    *widget = (LV2UI_Widget)(uintptr_t)ui->window;
    return ui;
}

static void ui_cleanup(LV2UI_Handle handle) {
    EuclidMonoUI* ui = (EuclidMonoUI*)handle;
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

static void ui_port_event(
    LV2UI_Handle handle,
    uint32_t port_index,
    uint32_t buffer_size,
    uint32_t format,
    const void* buffer
) {
    EuclidMonoUI* ui = (EuclidMonoUI*)handle;
    if (!ui || !buffer || format != 0) {
        return;
    }
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
    } else if (port_index == PORT_MIDI_NOTE) {
        const int note = clamp_int((int)lroundf(value), 0, 127);
        if (ui->note_box.value != note) {
            ui->note_box.value = note;
            ui->note_knob.value = note;
            if (!ui->note_box.editing) {
                reset_note_text(ui);
            }
            ui->needs_redraw = true;
        }
    } else if (port_index == PORT_INVERT_A) {
        const bool b = value >= 0.5f;
        if (ui->invert_a.value != b) {
            ui->invert_a.value = b;
            ui->needs_redraw = true;
        }
    } else if (port_index == PORT_INVERT_B) {
        const bool b = value >= 0.5f;
        if (ui->invert_b.value != b) {
            ui->invert_b.value = b;
            ui->needs_redraw = true;
        }
    } else if (port_index == PORT_LOGIC_OP) {
        const int op = clamp_int((int)lroundf(value), 0, LOGIC_COUNT - 1);
        if (ui->logic_dropdown.value != op) {
            ui->logic_dropdown.value = op;
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
