#include <lv2/ui/ui.h>

#include <X11/Xlib.h>
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

#define GREMLIN_URI "https://danja.github.io/flues/plugins/gremlin"
#define GREMLIN_UI_URI GREMLIN_URI "#ui"
#define LOG_PREFIX "[Gremlin UI] "

#define DEFAULT_WINDOW_WIDTH 1160
#define DEFAULT_WINDOW_HEIGHT 880

#define GROUP_PADDING 16
#define GROUP_GAP_X 18
#define GROUP_GAP_Y 22
#define TITLE_HEIGHT 20
#define KNOB_SIZE 88
#define KNOB_HEIGHT 106
#define KNOB_SPACING_X 14
#define KNOB_SPACING_Y 16

typedef enum {
    PORT_AUDIO_OUT_L = 0,
    PORT_AUDIO_OUT_R,
    PORT_MIDI_IN,
    PORT_CONTROLLER_IN,
    PORT_MODE,
    PORT_DAMAGE,
    PORT_CHAOS,
    PORT_NOISE,
    PORT_DRIFT,
    PORT_CRUNCH,
    PORT_FOLD,
    PORT_DELAY_TIME,
    PORT_FEEDBACK,
    PORT_WARP,
    PORT_STUTTER,
    PORT_TONE,
    PORT_DAMPING,
    PORT_SPACE,
    PORT_ATTACK,
    PORT_RELEASE,
    PORT_OUTPUT,
    PORT_SOURCE_GAIN,
    PORT_BURST,
    PORT_PITCH_SPREAD,
    PORT_DELAY_MIX,
    PORT_CROSS_FEEDBACK,
    PORT_GLITCH_LENGTH,
    PORT_CHAOS_RATE,
    PORT_DUCK,
    PORT_CONTROLLER_OUT,
    PORT_STATUS_LIVE_MODE,
    PORT_STATUS_LIVE_DAMAGE,
    PORT_STATUS_LIVE_CHAOS,
    PORT_STATUS_LIVE_NOISE,
    PORT_STATUS_LIVE_DRIFT,
    PORT_STATUS_LIVE_CRUNCH,
    PORT_STATUS_LIVE_FOLD,
    PORT_STATUS_LIVE_DELAY_TIME,
    PORT_STATUS_LIVE_FEEDBACK,
    PORT_STATUS_LIVE_WARP,
    PORT_STATUS_LIVE_STUTTER,
    PORT_STATUS_LIVE_TONE,
    PORT_STATUS_LIVE_DAMPING,
    PORT_STATUS_LIVE_SPACE,
    PORT_STATUS_LIVE_ATTACK,
    PORT_STATUS_LIVE_RELEASE,
    PORT_STATUS_LIVE_OUTPUT,
    PORT_STATUS_LIVE_SOURCE_GAIN,
    PORT_STATUS_LIVE_BURST,
    PORT_STATUS_LIVE_PITCH_SPREAD,
    PORT_STATUS_LIVE_DELAY_MIX,
    PORT_STATUS_LIVE_CROSS_FEEDBACK,
    PORT_STATUS_LIVE_GLITCH_LENGTH,
    PORT_STATUS_LIVE_CHAOS_RATE,
    PORT_STATUS_LIVE_DUCK,
    PORT_STATUS_SCENE,
    PORT_STATUS_CONTROLLER_ACTIVITY,
    PORT_STATUS_CONTROLLER_OUT_ACTIVE,
    PORT_STATUS_SOLO_HELD,
    PORT_STATUS_MASTER_TRIM,
    PORT_STATUS_MACRO_1,
    PORT_STATUS_MACRO_2,
    PORT_STATUS_MACRO_3,
    PORT_STATUS_MACRO_4,
    PORT_STATUS_MACRO_5,
    PORT_STATUS_MACRO_6,
    PORT_STATUS_MACRO_7,
    PORT_STATUS_MACRO_8,
    PORT_STATUS_MOMENTARY_1,
    PORT_STATUS_MOMENTARY_2,
    PORT_STATUS_MOMENTARY_3,
    PORT_STATUS_MOMENTARY_4,
    PORT_STATUS_MOMENTARY_5,
    PORT_STATUS_MOMENTARY_6,
    PORT_STATUS_MOMENTARY_7,
    PORT_STATUS_MOMENTARY_8,
    PORT_TOTAL_COUNT
} PortIndex;

typedef enum {
    GROUP_STATUS = 0,
    GROUP_SOURCE,
    GROUP_PERFORMANCE,
    GROUP_DELAY,
    GROUP_DELAY_EXTRAS,
    GROUP_COUNT
} GroupIndex;

typedef struct {
    GroupIndex group;
    const char* label;
    uint32_t control_port;
    uint32_t display_port;
    float min;
    float max;
    float def;
    uint32_t steps;
    const char* const* scale_labels;
    uint32_t scale_count;
} ControlDesc;

typedef struct {
    uint32_t control_port;
    uint32_t display_port;
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
} Knob;

typedef struct {
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
    cairo_t* cr;
    cairo_surface_t* back_buffer;
    cairo_t* back_cr;

    pthread_t thread;
    pthread_mutex_t mutex;
    volatile bool running;

    int width;
    int height;
    int content_width;
    int content_height;
    int board_x;
    int board_y;
    int board_width;
    int board_height;
    int master_x;
    int master_y;
    int master_width;
    int master_height;
    int strip_x[8];
    int strip_y;
    int strip_width;
    int strip_height;
    int macro_y;
    int macro_height;
    int rec_y;
    int solo_y;
    int mute_y;

    GroupState groups[GROUP_COUNT];
    Knob knobs[32];
    size_t knob_count;

    volatile bool needs_redraw;
    int active_knob;
    double drag_start_y;
    float drag_start_value;

    float controller_activity;
    float controller_out_active;
    float solo_held;
    float master_trim;
    int live_mode;
    int scene_index;
    float macro[8];
    float momentary[8];
} GremlinUI;

static pthread_mutex_t g_xlib_init_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_xlib_threads_ready = false;

static const char* const kModeLabels[] = {
    "Shard",
    "Servo",
    "Spray",
    "Collapse"
};

static const char* const kSceneLabels[] = {
    "Manual",
    "Splinter",
    "Melt",
    "Rust",
    "Tunnel"
};

static const char* const kMacroLabels[8] = {
    "SRC", "PIT", "BRK", "DLY", "SPC", "STT", "TON", "OUT"
};

static const char* const kMomentaryLabels[8] = {
    "FRZ", "STT", "CHA", "CRN", "FDB", "WRP", "NOI", "DCK"
};

static const char* const kRecLabels[8] = {
    "SHD", "SRV", "SPR", "COL", "SPL", "MLT", "RST", "TUN"
};

static const char* const kSoloLabels[8] = {
    "RSD", "BST", "RNS", "RND", "ALL", "<SC", ">SC", "RST"
};

static const uint32_t kTopRowPorts[8] = {
    PORT_DAMAGE,
    PORT_CHAOS,
    PORT_NOISE,
    PORT_DRIFT,
    PORT_CRUNCH,
    PORT_FOLD,
    PORT_ATTACK,
    PORT_RELEASE
};

static const uint32_t kMiddleRowPorts[8] = {
    PORT_DELAY_TIME,
    PORT_FEEDBACK,
    PORT_WARP,
    PORT_STUTTER,
    PORT_TONE,
    PORT_DAMPING,
    PORT_SPACE,
    PORT_OUTPUT
};

static const uint32_t kBottomRowPorts[8] = {
    PORT_SOURCE_GAIN,
    PORT_BURST,
    PORT_PITCH_SPREAD,
    PORT_DELAY_MIX,
    PORT_CROSS_FEEDBACK,
    PORT_GLITCH_LENGTH,
    PORT_CHAOS_RATE,
    PORT_DUCK
};

static const ControlDesc kControlInfo[] = {
    { GROUP_SOURCE, "MODE", PORT_MODE, PORT_STATUS_LIVE_MODE, 0.0f, 3.0f, 0.0f, 4, kModeLabels, 4 },
    { GROUP_SOURCE, "DAMAGE", PORT_DAMAGE, PORT_STATUS_LIVE_DAMAGE, 0.0f, 1.0f, 0.55f, 0, NULL, 0 },
    { GROUP_SOURCE, "CHAOS", PORT_CHAOS, PORT_STATUS_LIVE_CHAOS, 0.0f, 1.0f, 0.60f, 0, NULL, 0 },
    { GROUP_SOURCE, "NOISE", PORT_NOISE, PORT_STATUS_LIVE_NOISE, 0.0f, 1.0f, 0.30f, 0, NULL, 0 },
    { GROUP_SOURCE, "DRIFT", PORT_DRIFT, PORT_STATUS_LIVE_DRIFT, 0.0f, 1.0f, 0.35f, 0, NULL, 0 },
    { GROUP_SOURCE, "CRUNCH", PORT_CRUNCH, PORT_STATUS_LIVE_CRUNCH, 0.0f, 1.0f, 0.45f, 0, NULL, 0 },
    { GROUP_SOURCE, "FOLD", PORT_FOLD, PORT_STATUS_LIVE_FOLD, 0.0f, 1.0f, 0.35f, 0, NULL, 0 },

    { GROUP_PERFORMANCE, "ATTACK", PORT_ATTACK, PORT_STATUS_LIVE_ATTACK, 0.0f, 1.0f, 0.05f, 0, NULL, 0 },
    { GROUP_PERFORMANCE, "RELEASE", PORT_RELEASE, PORT_STATUS_LIVE_RELEASE, 0.0f, 1.0f, 0.25f, 0, NULL, 0 },
    { GROUP_PERFORMANCE, "OUTPUT", PORT_OUTPUT, PORT_STATUS_LIVE_OUTPUT, 0.0f, 1.0f, 0.45f, 0, NULL, 0 },
    { GROUP_PERFORMANCE, "SRC GAIN", PORT_SOURCE_GAIN, PORT_STATUS_LIVE_SOURCE_GAIN, 0.0f, 1.0f, 0.58f, 0, NULL, 0 },
    { GROUP_PERFORMANCE, "BURST", PORT_BURST, PORT_STATUS_LIVE_BURST, 0.0f, 1.0f, 0.40f, 0, NULL, 0 },
    { GROUP_PERFORMANCE, "SPREAD", PORT_PITCH_SPREAD, PORT_STATUS_LIVE_PITCH_SPREAD, 0.0f, 1.0f, 0.50f, 0, NULL, 0 },

    { GROUP_DELAY, "DLY TIME", PORT_DELAY_TIME, PORT_STATUS_LIVE_DELAY_TIME, 0.0f, 1.0f, 0.32f, 0, NULL, 0 },
    { GROUP_DELAY, "FDBK", PORT_FEEDBACK, PORT_STATUS_LIVE_FEEDBACK, 0.0f, 1.0f, 0.55f, 0, NULL, 0 },
    { GROUP_DELAY, "WARP", PORT_WARP, PORT_STATUS_LIVE_WARP, 0.0f, 1.0f, 0.45f, 0, NULL, 0 },
    { GROUP_DELAY, "STUTTER", PORT_STUTTER, PORT_STATUS_LIVE_STUTTER, 0.0f, 1.0f, 0.35f, 0, NULL, 0 },
    { GROUP_DELAY, "TONE", PORT_TONE, PORT_STATUS_LIVE_TONE, 0.0f, 1.0f, 0.65f, 0, NULL, 0 },
    { GROUP_DELAY, "DAMP", PORT_DAMPING, PORT_STATUS_LIVE_DAMPING, 0.0f, 1.0f, 0.45f, 0, NULL, 0 },
    { GROUP_DELAY, "SPACE", PORT_SPACE, PORT_STATUS_LIVE_SPACE, 0.0f, 1.0f, 0.55f, 0, NULL, 0 },

    { GROUP_DELAY_EXTRAS, "DLY MIX", PORT_DELAY_MIX, PORT_STATUS_LIVE_DELAY_MIX, 0.0f, 1.0f, 0.55f, 0, NULL, 0 },
    { GROUP_DELAY_EXTRAS, "X-FDBK", PORT_CROSS_FEEDBACK, PORT_STATUS_LIVE_CROSS_FEEDBACK, 0.0f, 1.0f, 0.50f, 0, NULL, 0 },
    { GROUP_DELAY_EXTRAS, "GLITCH", PORT_GLITCH_LENGTH, PORT_STATUS_LIVE_GLITCH_LENGTH, 0.0f, 1.0f, 0.50f, 0, NULL, 0 },
    { GROUP_DELAY_EXTRAS, "CHAOS RT", PORT_CHAOS_RATE, PORT_STATUS_LIVE_CHAOS_RATE, 0.0f, 1.0f, 0.55f, 0, NULL, 0 },
    { GROUP_DELAY_EXTRAS, "DUCK", PORT_DUCK, PORT_STATUS_LIVE_DUCK, 0.0f, 1.0f, 0.10f, 0, NULL, 0 }
};

static void ensure_xlib_threads(void) {
    pthread_mutex_lock(&g_xlib_init_lock);
    if (!g_xlib_threads_ready) {
        XInitThreads();
        g_xlib_threads_ready = true;
    }
    pthread_mutex_unlock(&g_xlib_init_lock);
}

static float clamp_float(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static float clamp_knob_value(const Knob* knob, float value) {
    value = clamp_float(value, knob->min, knob->max);
    if (knob->steps > 1) {
        float step = (knob->max - knob->min) / (float)(knob->steps - 1);
        value = knob->min + roundf((value - knob->min) / step) * step;
    }
    return value;
}

static Knob* find_knob_by_port(GremlinUI* ui, uint32_t control_port) {
    for (size_t i = 0; i < ui->knob_count; ++i) {
        if (ui->knobs[i].control_port == control_port) {
            return &ui->knobs[i];
        }
    }
    return NULL;
}

static void draw_group_background(cairo_t* cr, const GroupState* group, const char* title) {
    cairo_save(cr);
    cairo_rectangle(cr, group->x, group->y, group->width, group->height);
    cairo_set_source_rgb(cr, 0.12, 0.13, 0.16);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.34, 0.35, 0.40);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12.0);
    cairo_set_source_rgb(cr, 0.94, 0.70, 0.26);
    cairo_move_to(cr, group->x + GROUP_PADDING, group->y + GROUP_PADDING + 10);
    cairo_show_text(cr, title);
    cairo_restore(cr);
}

static void draw_strip_background(cairo_t* cr, double x, double y, double w, double h, int index) {
    cairo_save(cr);
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source_rgb(cr, 0.10, 0.11, 0.14);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.24, 0.26, 0.31);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.86, 0.88, 0.84);
    char label[2] = { (char)('1' + index), '\0' };
    cairo_move_to(cr, x + 10.0, y + 16.0);
    cairo_show_text(cr, label);
    cairo_restore(cr);
}

static void draw_knob(cairo_t* cr, const Knob* knob) {
    const double x = knob->x;
    const double y = knob->y;
    const double w = knob->width;
    const double h = knob->height;
    const double pad = 8.0;
    const double diameter = w - pad * 2.0;
    const double radius = diameter / 2.0;
    const double cx = x + w * 0.5;
    const double cy = y + h * 0.5 - 8.0;

    cairo_save(cr);
    cairo_rectangle(cr, x, y, w, h);
    cairo_clip(cr);

    cairo_set_source_rgb(cr, 0.08, 0.09, 0.11);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);

    cairo_arc(cr, cx, cy, radius, 0.0, 2.0 * M_PI);
    cairo_set_source_rgb(cr, 0.17, 0.18, 0.22);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.73, 0.42, 0.15);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);

    cairo_arc(cr, cx, cy, radius * 0.72, 0.0, 2.0 * M_PI);
    cairo_set_source_rgb(cr, 0.22, 0.24, 0.29);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 0.90, 0.58, 0.18, 0.50);
    cairo_set_line_width(cr, 1.4);
    {
        uint32_t ticks = knob->steps > 1 ? knob->steps : 11;
        for (uint32_t i = 0; i < ticks; ++i) {
            double t = (double)i / (double)(ticks - 1);
            double angle = (1.5 * M_PI * t) + (0.75 * M_PI);
            double r1 = radius * 0.82;
            double r2 = radius * 0.92;
            cairo_move_to(cr, cx + cos(angle) * r1, cy + sin(angle) * r1);
            cairo_line_to(cr, cx + cos(angle) * r2, cy + sin(angle) * r2);
        }
        cairo_stroke(cr);
    }

    {
        double norm = (knob->value - knob->min) / (knob->max - knob->min);
        double angle = (norm * 1.5 * M_PI) + (0.75 * M_PI);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_source_rgb(cr, 0.97, 0.66, 0.24);
        cairo_set_line_width(cr, 4.0);
        cairo_move_to(cr, cx + cos(angle) * radius * 0.22, cy + sin(angle) * radius * 0.22);
        cairo_line_to(cr, cx + cos(angle) * radius * 0.88, cy + sin(angle) * radius * 0.88);
        cairo_stroke(cr);
    }

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.92, 0.88, 0.76);

    char value_str[64];
    if (knob->scale_labels && knob->scale_count > 0) {
        uint32_t index = 0;
        if (knob->steps > 1) {
            float step = (knob->max - knob->min) / (float)(knob->steps - 1);
            index = (uint32_t)lrintf((knob->value - knob->min) / step);
            if (index >= knob->scale_count) {
                index = knob->scale_count - 1;
            }
        }
        snprintf(value_str, sizeof(value_str), "%s", knob->scale_labels[index]);
    } else {
        snprintf(value_str, sizeof(value_str), "%.2f", knob->value);
    }

    cairo_text_extents_t extents;
    cairo_text_extents(cr, value_str, &extents);
    cairo_move_to(cr, cx - extents.width * 0.5, cy + radius * 0.46);
    cairo_show_text(cr, value_str);

    cairo_set_source_rgb(cr, 0.74, 0.70, 0.61);
    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);
    cairo_text_extents(cr, knob->label, &extents);
    cairo_move_to(cr, cx - extents.width * 0.5, y + h - 7.0);
    cairo_show_text(cr, knob->label);

    cairo_restore(cr);
}

static void draw_status_pill(cairo_t* cr,
                             double x,
                             double y,
                             double w,
                             double h,
                             const char* label,
                             double amount,
                             double r,
                             double g,
                             double b) {
    amount = clamp_float((float)amount, 0.0f, 1.0f);
    cairo_save(cr);
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source_rgb(cr, 0.11, 0.12, 0.15);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.28, 0.30, 0.36);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_rectangle(cr, x + 2.0, y + 2.0, (w - 4.0) * amount, h - 4.0);
    cairo_set_source_rgba(cr, r, g, b, 0.20 + amount * 0.55);
    cairo_fill(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.92, 0.90, 0.84);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, label, &extents);
    cairo_move_to(cr, x + (w - extents.width) * 0.5, y + h * 0.64);
    cairo_show_text(cr, label);
    cairo_restore(cr);
}

static void draw_master_bar(cairo_t* cr, double x, double y, double w, double h, float value) {
    value = clamp_float(value, 0.0f, 1.0f);

    cairo_save(cr);
    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.92, 0.90, 0.84);
    cairo_move_to(cr, x, y - 8.0);
    cairo_show_text(cr, "Master Trim");

    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source_rgb(cr, 0.10, 0.11, 0.13);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.28, 0.30, 0.35);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_rectangle(cr, x + 2.0, y + 2.0, (w - 4.0) * value, h - 4.0);
    cairo_set_source_rgb(cr, 0.90, 0.46, 0.14);
    cairo_fill(cr);

    char value_str[32];
    snprintf(value_str, sizeof(value_str), "%.2f", value);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, value_str, &extents);
    cairo_move_to(cr, x + w + 10.0, y + h * 0.70);
    cairo_show_text(cr, value_str);
    cairo_restore(cr);
}

static void draw_macro_bar(cairo_t* cr, double x, double y, double w, double h, float value, const char* label) {
    value = clamp_float(value, 0.0f, 1.0f);
    double mid = y + h * 0.5;
    double fill = fabs(value - 0.5f) * h;
    double bar_y = value >= 0.5f ? (mid - fill) : mid;

    cairo_save(cr);
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source_rgb(cr, 0.09, 0.10, 0.12);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.24, 0.26, 0.31);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_move_to(cr, x, mid);
    cairo_line_to(cr, x + w, mid);
    cairo_set_source_rgb(cr, 0.42, 0.44, 0.49);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_rectangle(cr, x + 3.0, bar_y + 3.0, w - 6.0, fill - 6.0 > 0.0 ? fill - 6.0 : 0.0);
    if (value >= 0.5f) {
        cairo_set_source_rgb(cr, 0.90, 0.56, 0.18);
    } else {
        cairo_set_source_rgb(cr, 0.76, 0.21, 0.18);
    }
    cairo_fill(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.86, 0.84, 0.78);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, label, &extents);
    cairo_move_to(cr, x + (w - extents.width) * 0.5, y + h + 14.0);
    cairo_show_text(cr, label);
    cairo_restore(cr);
}

static void draw_button_cell(cairo_t* cr,
                             double x,
                             double y,
                             double w,
                             double h,
                             float active,
                             const char* label,
                             double r,
                             double g,
                             double b) {
    active = clamp_float(active, 0.0f, 1.0f);
    cairo_save(cr);
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source_rgb(cr, 0.10, 0.11, 0.13);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.29, 0.31, 0.36);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_rectangle(cr, x + 3.0, y + 3.0, w - 6.0, h - 6.0);
    cairo_set_source_rgba(cr, r, g, b, 0.14 + active * 0.74);
    cairo_fill(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 9.0);
    cairo_set_source_rgb(cr, 0.92, 0.90, 0.84);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, label, &extents);
    cairo_move_to(cr, x + (w - extents.width) * 0.5, y + h * 0.63);
    cairo_show_text(cr, label);
    cairo_restore(cr);
}

static void draw_status_panel(cairo_t* cr, const GremlinUI* ui) {
    const double x = ui->master_x;
    const double y = ui->master_y;
    const double w = ui->master_width;
    const double h = ui->master_height;

    cairo_save(cr);
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source_rgb(cr, 0.11, 0.12, 0.15);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.36, 0.37, 0.41);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14.0);
    cairo_set_source_rgb(cr, 0.95, 0.72, 0.22);
    cairo_move_to(cr, x + 18.0, y + 24.0);
    cairo_show_text(cr, "Master / MIDImix");

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.72, 0.76, 0.78);
    cairo_move_to(cr, x + 18.0, y + 42.0);
    cairo_show_text(cr, "Mode knob + master fader. Live status stays visible even if hardware LEDs fail.");

    draw_status_pill(cr, x + 18.0, y + 54.0, 88.0, 22.0, "CTRL IN", ui->controller_activity, 0.44, 0.96, 0.26);
    draw_status_pill(cr, x + 114.0, y + 54.0, 88.0, 22.0, "CTRL OUT", ui->controller_out_active, 0.24, 0.82, 0.96);
    draw_status_pill(cr, x + 210.0, y + 54.0, 58.0, 22.0, "SOLO", ui->solo_held, 0.96, 0.38, 0.12);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12.0);
    cairo_set_source_rgb(cr, 0.64, 0.69, 0.74);
    cairo_move_to(cr, x + 18.0, y + 228.0);
    cairo_show_text(cr, "MODE");
    cairo_move_to(cr, x + 18.0, y + 286.0);
    cairo_show_text(cr, "SCENE");

    cairo_set_font_size(cr, 28.0);
    cairo_set_source_rgb(cr, 0.96, 0.93, 0.87);
    cairo_move_to(cr, x + 18.0, y + 256.0);
    cairo_show_text(cr, kModeLabels[ui->live_mode < 0 ? 0 : ui->live_mode > 3 ? 3 : ui->live_mode]);
    cairo_move_to(cr, x + 18.0, y + 314.0);
    cairo_show_text(cr, kSceneLabels[ui->scene_index < 0 ? 0 : ui->scene_index > 4 ? 4 : ui->scene_index]);

    draw_macro_bar(cr, x + 208.0, y + 168.0, 42.0, 188.0, ui->master_trim, "MSTR");

    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.78, 0.83, 0.88);
    cairo_move_to(cr, x + 18.0, y + 352.0);
    cairo_show_text(cr, "REC row: modes 1-4, scenes 5-8");
    cairo_move_to(cr, x + 18.0, y + 370.0);
    cairo_show_text(cr, "SOLO row: action bank while SOLO held");
    cairo_move_to(cr, x + 18.0, y + 388.0);
    cairo_show_text(cr, "MUTE row: momentary performance layer");

    cairo_restore(cr);
}

static void draw_ui(GremlinUI* ui) {
    pthread_mutex_lock(&ui->mutex);
    if (!ui->surface || !ui->cr || !ui->back_buffer || !ui->back_cr) {
        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    cairo_t* cr = ui->back_cr;

    cairo_rectangle(cr, 0.0, 0.0, ui->width, ui->height);
    cairo_set_source_rgb(cr, 0.05, 0.06, 0.08);
    cairo_fill(cr);

    cairo_rectangle(cr, ui->board_x, ui->board_y, ui->board_width, ui->board_height);
    cairo_set_source_rgb(cr, 0.08, 0.09, 0.11);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.30, 0.32, 0.36);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14.0);
    cairo_set_source_rgb(cr, 0.95, 0.72, 0.22);
    cairo_move_to(cr, ui->board_x + 18.0, ui->board_y + 24.0);
    cairo_show_text(cr, "MIDImix Layout");

    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.72, 0.76, 0.78);
    cairo_move_to(cr, ui->board_x + 18.0, ui->board_y + 42.0);
    cairo_show_text(cr, "Three knob rows, channel macros, and button rows aligned to the controller.");

    {
        const char* row_labels[] = { "TOP", "MID", "LOW", "FADER", "REC", "SOLO", "MUTE" };
        const double row_y[] = {
            ui->strip_y + 52.0,
            ui->strip_y + 52.0 + (KNOB_HEIGHT + 18),
            ui->strip_y + 52.0 + 2.0 * (KNOB_HEIGHT + 18),
            ui->macro_y + ui->macro_height * 0.52,
            ui->rec_y + 14.0,
            ui->solo_y + 14.0,
            ui->mute_y + 14.0
        };
        cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 10.0);
        cairo_set_source_rgb(cr, 0.67, 0.70, 0.76);
        for (int i = 0; i < 7; ++i) {
            cairo_move_to(cr, ui->board_x + 14.0, row_y[i]);
            cairo_show_text(cr, row_labels[i]);
        }
    }

    for (int i = 0; i < 8; ++i) {
        draw_strip_background(cr,
                              ui->strip_x[i],
                              ui->strip_y,
                              ui->strip_width,
                              ui->strip_height,
                              i);
    }

    draw_status_panel(cr, ui);

    for (size_t i = 0; i < ui->knob_count; ++i) {
        draw_knob(cr, &ui->knobs[i]);
    }

    for (int i = 0; i < 8; ++i) {
        const double macro_x = ui->strip_x[i] + (ui->strip_width - 34.0) * 0.5;
        draw_macro_bar(cr, macro_x, ui->macro_y, 34.0, ui->macro_height, ui->macro[i], kMacroLabels[i]);

        {
            const float rec_active = (i < 4)
                ? (ui->live_mode == i ? 1.0f : 0.0f)
                : (ui->scene_index == (i - 3) ? 1.0f : 0.0f);
            draw_button_cell(cr,
                             ui->strip_x[i] + 8.0,
                             ui->rec_y,
                             ui->strip_width - 16.0,
                             22.0,
                             rec_active,
                             kRecLabels[i],
                             0.96,
                             0.70,
                             0.18);
        }

        draw_button_cell(cr,
                         ui->strip_x[i] + 8.0,
                         ui->solo_y,
                         ui->strip_width - 16.0,
                         22.0,
                         ui->solo_held,
                         kSoloLabels[i],
                         0.26,
                         0.68,
                         0.96);

        draw_button_cell(cr,
                         ui->strip_x[i] + 8.0,
                         ui->mute_y,
                         ui->strip_width - 16.0,
                         22.0,
                         ui->momentary[i],
                         kMomentaryLabels[i],
                         0.55,
                         0.94,
                         0.22);
    }

    cairo_set_source_surface(ui->cr, ui->back_buffer, 0.0, 0.0);
    cairo_paint(ui->cr);
    cairo_surface_flush(ui->surface);
    XFlush(ui->display);
    ui->needs_redraw = false;
    pthread_mutex_unlock(&ui->mutex);
}

static int find_knob_at(const GremlinUI* ui, int x, int y) {
    for (size_t i = 0; i < ui->knob_count; ++i) {
        const Knob* knob = &ui->knobs[i];
        if (x >= knob->x && x <= knob->x + knob->width &&
            y >= knob->y && y <= knob->y + knob->height) {
            return (int)i;
        }
    }
    return -1;
}

static void notify_host(GremlinUI* ui, uint32_t port, float value) {
    if (ui->write) {
        ui->write(ui->controller, port, sizeof(float), 0, &value);
    }
}

static void handle_button_press(GremlinUI* ui, const XButtonEvent* event) {
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

static void handle_button_release(GremlinUI* ui, const XButtonEvent* event) {
    (void)event;
    pthread_mutex_lock(&ui->mutex);
    ui->active_knob = -1;
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_scroll(GremlinUI* ui, const XButtonEvent* event) {
    int knob_index = find_knob_at(ui, event->x, event->y);
    if (knob_index < 0) {
        return;
    }

    pthread_mutex_lock(&ui->mutex);
    Knob* knob = &ui->knobs[knob_index];
    float step = knob->steps > 1
        ? (knob->max - knob->min) / (float)(knob->steps - 1)
        : (knob->max - knob->min) / 100.0f;
    float value = knob->value;
    if (event->button == Button4) {
        value += step;
    } else if (event->button == Button5) {
        value -= step;
    }
    value = clamp_knob_value(knob, value);
    if (fabsf(value - knob->value) > 0.0001f) {
        knob->value = value;
        ui->needs_redraw = true;
        notify_host(ui, knob->control_port, knob->value);
    }
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_motion(GremlinUI* ui, const XMotionEvent* event) {
    pthread_mutex_lock(&ui->mutex);
    if (ui->active_knob >= 0 && (size_t)ui->active_knob < ui->knob_count) {
        Knob* knob = &ui->knobs[ui->active_knob];
        double delta = ui->drag_start_y - event->y;
        double sensitivity = (knob->max - knob->min) / 200.0;
        float value = clamp_knob_value(knob, ui->drag_start_value + (float)(delta * sensitivity));
        if (fabsf(value - knob->value) > 0.0001f) {
            knob->value = value;
            ui->needs_redraw = true;
            notify_host(ui, knob->control_port, knob->value);
        }
    }
    pthread_mutex_unlock(&ui->mutex);
}

static void process_x_event(GremlinUI* ui, XEvent* event);
static void setup_layout(GremlinUI* ui, int available_width);

static void* event_thread_main(void* data) {
    GremlinUI* ui = (GremlinUI*)data;
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

static void process_x_event(GremlinUI* ui, XEvent* event) {
    switch (event->type) {
        case Expose:
            pthread_mutex_lock(&ui->mutex);
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
            break;
        case ConfigureNotify:
            pthread_mutex_lock(&ui->mutex);
            if (event->xconfigure.width != ui->width || event->xconfigure.height != ui->height) {
                ui->width = event->xconfigure.width;
                ui->height = event->xconfigure.height;
                if (ui->surface) {
                    cairo_surface_destroy(ui->surface);
                    ui->surface = NULL;
                }
                if (ui->cr) {
                    cairo_destroy(ui->cr);
                    ui->cr = NULL;
                }
                if (ui->back_cr) {
                    cairo_destroy(ui->back_cr);
                    ui->back_cr = NULL;
                }
                if (ui->back_buffer) {
                    cairo_surface_destroy(ui->back_buffer);
                    ui->back_buffer = NULL;
                }

                ui->surface = cairo_xlib_surface_create(ui->display,
                                                        ui->window,
                                                        DefaultVisual(ui->display, ui->screen),
                                                        ui->width,
                                                        ui->height);
                ui->cr = cairo_create(ui->surface);
                ui->back_buffer = cairo_image_surface_create(CAIRO_FORMAT_RGB24, ui->width, ui->height);
                ui->back_cr = cairo_create(ui->back_buffer);
                setup_layout(ui, ui->width - 40);
                ui->needs_redraw = true;
            }
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

static void setup_layout(GremlinUI* ui, int available_width) {
    (void)available_width;

    memset(ui->groups, 0, sizeof(ui->groups));
    ui->knob_count = sizeof(kControlInfo) / sizeof(kControlInfo[0]);

    for (size_t i = 0; i < ui->knob_count; ++i) {
        const ControlDesc* desc = &kControlInfo[i];
        Knob* knob = &ui->knobs[i];
        knob->control_port = desc->control_port;
        knob->display_port = desc->display_port;
        knob->label = desc->label;
        knob->min = desc->min;
        knob->max = desc->max;
        knob->def = desc->def;
        knob->steps = desc->steps;
        knob->scale_labels = desc->scale_labels;
        knob->scale_count = desc->scale_count;
        knob->width = KNOB_SIZE;
        knob->height = KNOB_HEIGHT;
    }

    ui->board_x = 20;
    ui->board_y = 20;
    ui->strip_width = 104;
    ui->strip_y = ui->board_y + 58;
    ui->macro_y = ui->strip_y + 3 * (KNOB_HEIGHT + 18) + 12;
    ui->macro_height = 132;
    ui->rec_y = ui->macro_y + ui->macro_height + 18;
    ui->solo_y = ui->rec_y + 30;
    ui->mute_y = ui->solo_y + 30;
    ui->strip_height = (ui->mute_y + 22 + 14) - ui->strip_y;

    {
        const int left_gutter = 54;
        const int strip_gap = 8;
        const int strips_total = 8 * ui->strip_width + 7 * strip_gap;
        ui->board_width = left_gutter + strips_total + 18;
        ui->board_height = ui->strip_height + 72;

        for (int i = 0; i < 8; ++i) {
            ui->strip_x[i] = ui->board_x + left_gutter + i * (ui->strip_width + strip_gap);
        }
    }

    ui->master_x = ui->board_x + ui->board_width + 18;
    ui->master_y = ui->board_y;
    ui->master_width = 282;
    ui->master_height = ui->board_height;

    ui->content_width = ui->master_x + ui->master_width + 20;
    ui->content_height = ui->board_y + ui->board_height + 20;

    {
        const int top_row_y = ui->strip_y + 24;
        const int middle_row_y = top_row_y + KNOB_HEIGHT + 18;
        const int bottom_row_y = middle_row_y + KNOB_HEIGHT + 18;

        for (int i = 0; i < 8; ++i) {
            const int knob_x = ui->strip_x[i] + (ui->strip_width - KNOB_SIZE) / 2;

            Knob* top = find_knob_by_port(ui, kTopRowPorts[i]);
            if (top) {
                top->x = knob_x;
                top->y = top_row_y;
            }

            Knob* middle = find_knob_by_port(ui, kMiddleRowPorts[i]);
            if (middle) {
                middle->x = knob_x;
                middle->y = middle_row_y;
            }

            Knob* bottom = find_knob_by_port(ui, kBottomRowPorts[i]);
            if (bottom) {
                bottom->x = knob_x;
                bottom->y = bottom_row_y;
            }
        }
    }

    {
        Knob* mode = find_knob_by_port(ui, PORT_MODE);
        if (mode) {
            mode->x = ui->master_x + 18;
            mode->y = ui->master_y + 82;
        }
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

    if (strcmp(plugin_uri, GREMLIN_URI) != 0) {
        fprintf(stderr, LOG_PREFIX "Plugin URI mismatch (%s)\n", plugin_uri);
        return NULL;
    }

    ensure_xlib_threads();

    GremlinUI* ui = (GremlinUI*)calloc(1, sizeof(GremlinUI));
    if (!ui) {
        return NULL;
    }

    pthread_mutex_init(&ui->mutex, NULL);
    ui->write = write_function;
    ui->controller = controller;
    ui->active_knob = -1;
    ui->needs_redraw = true;
    ui->controller_activity = 0.0f;
    ui->controller_out_active = 0.0f;
    ui->solo_held = 0.0f;
    ui->master_trim = 0.45f;
    ui->live_mode = 0;
    ui->scene_index = 0;
    for (int i = 0; i < 8; ++i) {
        ui->macro[i] = 0.5f;
        ui->momentary[i] = 0.0f;
    }

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

    setup_layout(ui, DEFAULT_WINDOW_WIDTH - 40);

    ui->width = DEFAULT_WINDOW_WIDTH;
    ui->height = DEFAULT_WINDOW_HEIGHT;
    if (ui->content_width > ui->width) {
        ui->width = ui->content_width;
    }
    if (ui->content_height > ui->height) {
        ui->height = ui->content_height;
    }

    XSetWindowAttributes attrs;
    attrs.background_pixel = BlackPixel(display, ui->screen);
    attrs.event_mask = ExposureMask |
                       StructureNotifyMask |
                       ButtonPressMask |
                       ButtonReleaseMask |
                       PointerMotionMask;

    ui->window = XCreateWindow(display,
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

    XStoreName(display, ui->window, "Gremlin");
    XMapWindow(display, ui->window);
    XFlush(display);

    ui->surface = cairo_xlib_surface_create(display,
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

    ui->cr = cairo_create(ui->surface);
    ui->back_buffer = cairo_image_surface_create(CAIRO_FORMAT_RGB24, ui->width, ui->height);
    ui->back_cr = cairo_create(ui->back_buffer);
    if (!ui->cr || !ui->back_buffer || !ui->back_cr) {
        fprintf(stderr, LOG_PREFIX "Failed to create Cairo drawing contexts\n");
        if (ui->back_cr) {
            cairo_destroy(ui->back_cr);
        }
        if (ui->back_buffer) {
            cairo_surface_destroy(ui->back_buffer);
        }
        if (ui->cr) {
            cairo_destroy(ui->cr);
        }
        cairo_surface_destroy(ui->surface);
        XDestroyWindow(display, ui->window);
        XCloseDisplay(display);
        pthread_mutex_destroy(&ui->mutex);
        free(ui);
        return NULL;
    }

    for (size_t i = 0; i < ui->knob_count; ++i) {
        ui->knobs[i].value = ui->knobs[i].def;
    }

    ui->running = true;
    if (pthread_create(&ui->thread, NULL, event_thread_main, ui) != 0) {
        fprintf(stderr, LOG_PREFIX "Failed to start event thread\n");
        cairo_destroy(ui->back_cr);
        cairo_surface_destroy(ui->back_buffer);
        cairo_destroy(ui->cr);
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
    GremlinUI* ui = (GremlinUI*)handle;
    if (!ui) {
        return;
    }

    ui->running = false;
    pthread_join(ui->thread, NULL);

    if (ui->back_cr) {
        cairo_destroy(ui->back_cr);
    }
    if (ui->back_buffer) {
        cairo_surface_destroy(ui->back_buffer);
    }
    if (ui->cr) {
        cairo_destroy(ui->cr);
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

static void ui_port_event(LV2UI_Handle handle,
                          uint32_t port_index,
                          uint32_t buffer_size,
                          uint32_t format,
                          const void* buffer) {
    GremlinUI* ui = (GremlinUI*)handle;
    if (!ui || !buffer || format != 0 || buffer_size < sizeof(float)) {
        return;
    }

    const float value = *((const float*)buffer);
    bool updated = false;

    pthread_mutex_lock(&ui->mutex);

    for (size_t i = 0; i < ui->knob_count; ++i) {
        Knob* knob = &ui->knobs[i];
        if (port_index == knob->control_port || port_index == knob->display_port) {
            float clamped = clamp_knob_value(knob, value);
            if (fabsf(clamped - knob->value) > 0.0001f) {
                knob->value = clamped;
                updated = true;
            }
        }
    }

    switch (port_index) {
        case PORT_STATUS_LIVE_MODE:
        case PORT_MODE:
            ui->live_mode = (int)lrintf(clamp_float(value, 0.0f, 3.0f));
            updated = true;
            break;
        case PORT_STATUS_SCENE:
            ui->scene_index = (int)lrintf(clamp_float(value, 0.0f, 4.0f));
            updated = true;
            break;
        case PORT_STATUS_CONTROLLER_ACTIVITY:
            ui->controller_activity = clamp_float(value, 0.0f, 1.0f);
            updated = true;
            break;
        case PORT_STATUS_CONTROLLER_OUT_ACTIVE:
            ui->controller_out_active = clamp_float(value, 0.0f, 1.0f);
            updated = true;
            break;
        case PORT_STATUS_SOLO_HELD:
            ui->solo_held = clamp_float(value, 0.0f, 1.0f);
            updated = true;
            break;
        case PORT_STATUS_MASTER_TRIM:
            ui->master_trim = clamp_float(value, 0.0f, 1.0f);
            updated = true;
            break;
        default:
            if (port_index >= PORT_STATUS_MACRO_1 && port_index <= PORT_STATUS_MACRO_8) {
                ui->macro[port_index - PORT_STATUS_MACRO_1] = clamp_float(value, 0.0f, 1.0f);
                updated = true;
            } else if (port_index >= PORT_STATUS_MOMENTARY_1 && port_index <= PORT_STATUS_MOMENTARY_8) {
                ui->momentary[port_index - PORT_STATUS_MOMENTARY_1] = clamp_float(value, 0.0f, 1.0f);
                updated = true;
            }
            break;
    }

    if (updated) {
        ui->needs_redraw = true;
    }

    pthread_mutex_unlock(&ui->mutex);
}

static const void* ui_extension_data(const char* uri) {
    (void)uri;
    return NULL;
}

static const LV2UI_Descriptor ui_descriptor = {
    GREMLIN_UI_URI,
    ui_instantiate,
    ui_cleanup,
    ui_port_event,
    ui_extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return index == 0 ? &ui_descriptor : NULL;
}
