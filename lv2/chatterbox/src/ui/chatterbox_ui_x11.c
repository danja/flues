// chatterbox_ui_x11.c
// X11/Cairo UI for Chatterbox speech synthesizer LV2 plugin

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

#define CHATTERBOX_URI "https://danja.github.io/flues/plugins/chatterbox"
#define CHATTERBOX_UI_URI CHATTERBOX_URI "#ui"
#define LOG_PREFIX "[Chatterbox UI] "

#define DEFAULT_WINDOW_WIDTH 880
#define DEFAULT_WINDOW_HEIGHT 520

#define GROUP_PADDING 14
#define GROUP_GAP_X 14
#define GROUP_GAP_Y 20
#define TITLE_HEIGHT 20
#define KNOB_SIZE 72
#define KNOB_HEIGHT 88
#define KNOB_SPACING_X 10
#define KNOB_SPACING_Y 12

typedef enum {
    PORT_AUDIO_OUT = 0,
    PORT_MIDI_IN,
    PORT_PITCH,
    PORT_VOICED,
    PORT_ASPIRATED,
    PORT_NOISE_LEVEL,
    PORT_F1,
    PORT_F2,
    PORT_F3,
    PORT_F4,
    PORT_NASAL,
    PORT_SING,
    PORT_SHOUT,
    PORT_FRY,
    PORT_STRESS,
    PORT_ATTACK,
    PORT_RELEASE,
    PORT_REVERB_SIZE,
    PORT_REVERB_LEVEL,
    PORT_MASTER_GAIN,
    PORT_TOTAL_COUNT
} PortIndex;

typedef enum {
    GROUP_SOURCE = 0,
    GROUP_JOYSTICK,
    GROUP_FORMANTS,
    GROUP_VOCAL_MODES,
    GROUP_DYNAMICS,
    GROUP_SPACE,
    GROUP_OUTPUT,
    GROUP_COUNT
} GroupIndex;

typedef struct {
    GroupIndex group;
    const char* label;
    uint32_t port;
    float min;
    float max;
    float def;
    bool is_toggle;
} ControlDesc;

static const ControlDesc kControlInfo[] = {
    // Source group
    { GROUP_SOURCE, "PITCH", PORT_PITCH, 0.0f, 1.0f, 0.30f, false },
    { GROUP_SOURCE, "VOICED", PORT_VOICED, 0.0f, 1.0f, 1.0f, true },
    { GROUP_SOURCE, "ASPIRATED", PORT_ASPIRATED, 0.0f, 1.0f, 0.0f, true },
    { GROUP_SOURCE, "NOISE", PORT_NOISE_LEVEL, 0.0f, 1.0f, 0.20f, false },

    // Joystick group - F1/F2 controlled by canvas (no knobs here)

    // Formants group - only F3/F4
    { GROUP_FORMANTS, "F3 (LIPS)", PORT_F3, 0.0f, 1.0f, 0.50f, false },
    { GROUP_FORMANTS, "F4 (QUAL)", PORT_F4, 0.0f, 1.0f, 0.50f, false },

    // Vocal modes group
    { GROUP_VOCAL_MODES, "NASAL", PORT_NASAL, 0.0f, 1.0f, 0.0f, true },
    { GROUP_VOCAL_MODES, "SING", PORT_SING, 0.0f, 1.0f, 0.0f, true },
    { GROUP_VOCAL_MODES, "SHOUT", PORT_SHOUT, 0.0f, 1.0f, 0.0f, true },
    { GROUP_VOCAL_MODES, "FRY", PORT_FRY, 0.0f, 1.0f, 0.0f, true },

    // Dynamics group
    { GROUP_DYNAMICS, "STRESS", PORT_STRESS, 0.0f, 1.0f, 0.30f, false },
    { GROUP_DYNAMICS, "ATTACK", PORT_ATTACK, 0.0f, 1.0f, 0.33f, false },
    { GROUP_DYNAMICS, "RELEASE", PORT_RELEASE, 0.0f, 1.0f, 0.33f, false },

    // Space group
    { GROUP_SPACE, "SIZE", PORT_REVERB_SIZE, 0.0f, 1.0f, 0.30f, false },
    { GROUP_SPACE, "LEVEL", PORT_REVERB_LEVEL, 0.0f, 1.0f, 0.20f, false },

    // Output group
    { GROUP_OUTPUT, "MASTER", PORT_MASTER_GAIN, 0.0f, 1.0f, 0.80f, false }
};

typedef struct {
    int row;
    int columns;
} GroupLayout;

static const GroupLayout kGroupLayout[GROUP_COUNT] = {
    [GROUP_SOURCE] = { 0, 4 },
    [GROUP_JOYSTICK] = { 0, 0 },  // Special: canvas, not knobs
    [GROUP_FORMANTS] = { 1, 2 },  // Only F3/F4
    [GROUP_VOCAL_MODES] = { 1, 4 },
    [GROUP_DYNAMICS] = { 1, 3 },
    [GROUP_SPACE] = { 2, 2 },
    [GROUP_OUTPUT] = { 2, 1 }
};

static const GroupIndex kRowGroups[][4] = {
    { GROUP_SOURCE, GROUP_JOYSTICK, GROUP_COUNT, GROUP_COUNT },
    { GROUP_FORMANTS, GROUP_VOCAL_MODES, GROUP_DYNAMICS, GROUP_COUNT },
    { GROUP_SPACE, GROUP_OUTPUT, GROUP_COUNT, GROUP_COUNT }
};

static const char* const kGroupTitles[GROUP_COUNT] = {
    "Source",
    "Vowel Space (F1/F2)",
    "Formants",
    "Vocal Modes",
    "Dynamics",
    "Reverb",
    "Output"
};

typedef struct {
    uint32_t port;
    const char* label;
    float min;
    float max;
    float def;
    float value;
    bool is_toggle;
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
    const char* label;
    float f1;  // Normalized 0-1
    float f2;  // Normalized 0-1
} VowelMarker;

static const VowelMarker kVowelMarkers[] = {
    { "i", 0.09f, 0.71f },  // see: close front
    { "e", 0.41f, 0.53f },  // bed: mid front
    { "a", 0.66f, 0.24f },  // father: open central
    { "o", 0.46f, 0.14f },  // home: mid back
    { "u", 0.13f, 0.15f }   // boot: close back
};

typedef struct {
    int x, y;
    int width, height;
    float f1, f2;  // Current position
    bool active;   // Being dragged
} JoystickState;

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
    JoystickState joystick;

    volatile bool needs_redraw;
    int active_knob;
    double drag_start_y;
    float drag_start_value;
} ChatterboxUI;

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

static void setup_layout(ChatterboxUI* ui, int available_width);

static float clamp_value(const Knob* knob, float value) {
    if (value < knob->min) {
        value = knob->min;
    } else if (value > knob->max) {
        value = knob->max;
    }
    if (knob->is_toggle) {
        value = (value > 0.5f) ? 1.0f : 0.0f;
    }
    return value;
}

static void draw_group_background(cairo_t* cr, const GroupState* group, const char* title) {
    const double x = group->x;
    const double y = group->y;
    const double w = group->width;
    const double h = group->height;

    cairo_save(cr);
    cairo_new_path(cr);
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source_rgb(cr, 0.15, 0.15, 0.15);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.35, 0.35, 0.35);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    cairo_new_path(cr);
    cairo_restore(cr);

    if (title && title[0]) {
        cairo_save(cr);
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 11.0);
        cairo_set_source_rgb(cr, 0.65, 0.65, 0.65);
        cairo_move_to(cr, x + 8, y + 14);
        cairo_show_text(cr, title);
        cairo_restore(cr);
    }
}

static void draw_knob(cairo_t* cr, const Knob* knob) {
    const double cx = knob->x + knob->width / 2.0;
    const double cy = knob->y + 26.0;
    const double radius = 28.0;

    float normalized = (knob->value - knob->min) / (knob->max - knob->min);
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    // Draw toggle as LED indicator if applicable
    if (knob->is_toggle) {
        // LED circle
        cairo_save(cr);
        cairo_new_path(cr);
        cairo_arc(cr, cx, cy, radius * 0.6, 0, 2 * M_PI);
        if (knob->value > 0.5f) {
            cairo_set_source_rgb(cr, 0.2, 0.8, 0.3);  // Green when on
        } else {
            cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);  // Dark when off
        }
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        cairo_set_line_width(cr, 2.0);
        cairo_stroke(cr);
        cairo_new_path(cr);
        cairo_restore(cr);
    } else {
        // Draw rotary knob
        cairo_save(cr);
        cairo_new_path(cr);
        cairo_arc(cr, cx, cy, radius, 0, 2 * M_PI);
        cairo_set_source_rgb(cr, 0.25, 0.25, 0.25);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0.45, 0.45, 0.45);
        cairo_set_line_width(cr, 2.0);
        cairo_stroke(cr);
        cairo_new_path(cr);
        cairo_restore(cr);

        // Draw value indicator line
        const double angle_min = -2.4;
        const double angle_max = 2.4;
        const double angle = angle_min + normalized * (angle_max - angle_min);
        const double line_len = radius * 0.7;
        const double x2 = cx + cos(angle) * line_len;
        const double y2 = cy + sin(angle) * line_len;

        cairo_save(cr);
        cairo_new_path(cr);
        cairo_move_to(cr, cx, cy);
        cairo_line_to(cr, x2, y2);
        cairo_set_source_rgb(cr, 0.8, 0.8, 0.9);
        cairo_set_line_width(cr, 3.0);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_stroke(cr);
        cairo_new_path(cr);
        cairo_restore(cr);
    }

    // Draw label
    cairo_save(cr);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 9.0);
    cairo_set_source_rgb(cr, 0.75, 0.75, 0.75);

    cairo_text_extents_t extents;
    cairo_text_extents(cr, knob->label, &extents);
    const double label_x = cx - extents.width / 2.0;
    const double label_y = cy + radius + 18.0;
    cairo_move_to(cr, label_x, label_y);
    cairo_show_text(cr, knob->label);
    cairo_restore(cr);
}

static void draw_joystick(cairo_t* cr, const JoystickState* joy) {
    const double x = joy->x;
    const double y = joy->y;
    const double w = joy->width;
    const double h = joy->height;
    const double pad = 10.0;
    const double canvas_x = x + pad;
    const double canvas_y = y + TITLE_HEIGHT + pad;
    const double canvas_w = w - 2 * pad;
    const double canvas_h = h - TITLE_HEIGHT - 2 * pad;

    // Draw canvas background
    cairo_save(cr);
    cairo_new_path(cr);
    cairo_rectangle(cr, canvas_x, canvas_y, canvas_w, canvas_h);
    cairo_set_source_rgb(cr, 0.08, 0.08, 0.08);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    cairo_new_path(cr);
    cairo_restore(cr);

    // Draw grid lines
    cairo_save(cr);
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_set_line_width(cr, 0.5);
    for (int i = 1; i < 4; i++) {
        double gx = canvas_x + (canvas_w * i / 4.0);
        double gy = canvas_y + (canvas_h * i / 4.0);
        cairo_new_path(cr);
        cairo_move_to(cr, gx, canvas_y);
        cairo_line_to(cr, gx, canvas_y + canvas_h);
        cairo_stroke(cr);
        cairo_new_path(cr);
        cairo_move_to(cr, canvas_x, gy);
        cairo_line_to(cr, canvas_x + canvas_w, gy);
        cairo_stroke(cr);
        cairo_new_path(cr);
    }
    cairo_restore(cr);

    // Draw vowel markers
    cairo_save(cr);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    for (size_t i = 0; i < sizeof(kVowelMarkers) / sizeof(kVowelMarkers[0]); i++) {
        const VowelMarker* v = &kVowelMarkers[i];
        double vx = canvas_x + v->f2 * canvas_w;
        double vy = canvas_y + v->f1 * canvas_h;

        // Draw circle
        cairo_new_path(cr);
        cairo_arc(cr, vx, vy, 4.0, 0, 2 * M_PI);
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.6);
        cairo_fill(cr);
        cairo_new_path(cr);

        // Draw label
        cairo_set_source_rgb(cr, 0.7, 0.7, 0.8);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, v->label, &ext);
        cairo_move_to(cr, vx - ext.width / 2.0, vy - 8.0);
        cairo_show_text(cr, v->label);
    }
    cairo_restore(cr);

    // Draw current position crosshair
    cairo_save(cr);
    double pos_x = canvas_x + joy->f2 * canvas_w;
    double pos_y = canvas_y + joy->f1 * canvas_h;

    cairo_new_path(cr);
    cairo_arc(cr, pos_x, pos_y, 6.0, 0, 2 * M_PI);
    cairo_set_source_rgba(cr, 0.9, 0.3, 0.3, 0.8);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 1.0, 0.5, 0.5);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
    cairo_new_path(cr);

    // Crosshair lines
    cairo_set_source_rgba(cr, 0.9, 0.3, 0.3, 0.5);
    cairo_set_line_width(cr, 1.0);
    cairo_new_path(cr);
    cairo_move_to(cr, canvas_x, pos_y);
    cairo_line_to(cr, canvas_x + canvas_w, pos_y);
    cairo_stroke(cr);
    cairo_new_path(cr);
    cairo_move_to(cr, pos_x, canvas_y);
    cairo_line_to(cr, pos_x, canvas_y + canvas_h);
    cairo_stroke(cr);
    cairo_new_path(cr);
    cairo_restore(cr);

    // Draw axis labels
    cairo_save(cr);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 9.0);
    cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
    cairo_move_to(cr, canvas_x + canvas_w + 5, canvas_y + 10);
    cairo_show_text(cr, "front");
    cairo_move_to(cr, canvas_x - 25, canvas_y + 10);
    cairo_show_text(cr, "back");
    cairo_move_to(cr, canvas_x + 5, canvas_y + 12);
    cairo_show_text(cr, "close");
    cairo_move_to(cr, canvas_x + 5, canvas_y + canvas_h - 2);
    cairo_show_text(cr, "open");
    cairo_restore(cr);
}

static void draw_ui(ChatterboxUI* ui) {
    pthread_mutex_lock(&ui->mutex);

    cairo_t* cr = cairo_create(ui->surface);

    // Background
    cairo_set_source_rgb(cr, 0.10, 0.10, 0.10);
    cairo_paint(cr);

    // Draw groups
    for (int i = 0; i < GROUP_COUNT; i++) {
        if (ui->groups[i].count > 0 || i == GROUP_JOYSTICK) {
            draw_group_background(cr, &ui->groups[i], kGroupTitles[i]);
        }
    }

    // Draw joystick
    if (ui->joystick.width > 0) {
        draw_joystick(cr, &ui->joystick);
    }

    // Draw knobs
    for (uint32_t i = 0; i < PORT_TOTAL_COUNT; i++) {
        if (ui->knob_used[i]) {
            draw_knob(cr, &ui->knobs[i]);
        }
    }

    cairo_destroy(cr);
    XFlush(ui->display);

    ui->needs_redraw = false;
    pthread_mutex_unlock(&ui->mutex);
}

static void notify_host(ChatterboxUI* ui, uint32_t port, float value) {
    if (ui->write && ui->controller) {
        ui->write(ui->controller, port, sizeof(float), 0, &value);
    }
}

static bool is_in_joystick(const JoystickState* joy, int x, int y) {
    const double pad = 10.0;
    const double canvas_x = joy->x + pad;
    const double canvas_y = joy->y + TITLE_HEIGHT + pad;
    const double canvas_w = joy->width - 2 * pad;
    const double canvas_h = joy->height - TITLE_HEIGHT - 2 * pad;

    return x >= canvas_x && x < canvas_x + canvas_w &&
           y >= canvas_y && y < canvas_y + canvas_h;
}

static void update_joystick_position(ChatterboxUI* ui, int x, int y) {
    const double pad = 10.0;
    const double canvas_x = ui->joystick.x + pad;
    const double canvas_y = ui->joystick.y + TITLE_HEIGHT + pad;
    const double canvas_w = ui->joystick.width - 2 * pad;
    const double canvas_h = ui->joystick.height - TITLE_HEIGHT - 2 * pad;

    float f2 = (x - canvas_x) / canvas_w;  // Horizontal = F2
    float f1 = (y - canvas_y) / canvas_h;  // Vertical = F1

    // Clamp to 0-1
    if (f1 < 0.0f) f1 = 0.0f;
    if (f1 > 1.0f) f1 = 1.0f;
    if (f2 < 0.0f) f2 = 0.0f;
    if (f2 > 1.0f) f2 = 1.0f;

    ui->joystick.f1 = f1;
    ui->joystick.f2 = f2;

    // Update knob values and notify host
    ui->knobs[PORT_F1].value = f1;
    ui->knobs[PORT_F2].value = f2;
    notify_host(ui, PORT_F1, f1);
    notify_host(ui, PORT_F2, f2);
    ui->needs_redraw = true;
}

static int find_knob_at(ChatterboxUI* ui, int x, int y) {
    for (uint32_t i = 0; i < PORT_TOTAL_COUNT; i++) {
        if (!ui->knob_used[i]) {
            continue;
        }
        const Knob* knob = &ui->knobs[i];
        if (x >= knob->x && x < knob->x + knob->width &&
            y >= knob->y && y < knob->y + knob->height) {
            return i;
        }
    }
    return -1;
}

static void handle_button_press(ChatterboxUI* ui, const XButtonEvent* event) {
    pthread_mutex_lock(&ui->mutex);

    // Check joystick first
    if (is_in_joystick(&ui->joystick, event->x, event->y)) {
        ui->joystick.active = true;
        update_joystick_position(ui, event->x, event->y);
        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    int knob_index = find_knob_at(ui, event->x, event->y);
    if (knob_index >= 0) {
        Knob* knob = &ui->knobs[knob_index];
        if (knob->is_toggle) {
            // Toggle on click
            float new_value = (knob->value > 0.5f) ? 0.0f : 1.0f;
            knob->value = new_value;
            ui->needs_redraw = true;
            notify_host(ui, knob->port, new_value);
        } else {
            // Start drag
            ui->active_knob = knob_index;
            ui->drag_start_y = event->y;
            ui->drag_start_value = knob->value;
        }
    }
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_button_release(ChatterboxUI* ui, const XButtonEvent* event) {
    (void)event;
    pthread_mutex_lock(&ui->mutex);
    ui->active_knob = -1;
    ui->joystick.active = false;
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_scroll(ChatterboxUI* ui, const XButtonEvent* event) {
    int knob_index = find_knob_at(ui, event->x, event->y);
    if (knob_index < 0 || !ui->knob_used[knob_index]) {
        return;
    }

    pthread_mutex_lock(&ui->mutex);
    Knob* knob = &ui->knobs[knob_index];
    if (knob->is_toggle) {
        pthread_mutex_unlock(&ui->mutex);
        return;  // No scroll for toggles
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

static void handle_motion(ChatterboxUI* ui, const XMotionEvent* event) {
    pthread_mutex_lock(&ui->mutex);

    // Handle joystick drag
    if (ui->joystick.active) {
        update_joystick_position(ui, event->x, event->y);
        pthread_mutex_unlock(&ui->mutex);
        return;
    }

    int knob_index = ui->active_knob;
    if (knob_index >= 0 && ui->knob_used[knob_index]) {
        Knob* knob = &ui->knobs[knob_index];
        if (!knob->is_toggle) {
            double delta = ui->drag_start_y - event->y;
            double sensitivity = (knob->max - knob->min) / 200.0;
            float value = clamp_value(knob, ui->drag_start_value + (float)(delta * sensitivity));
            if (fabsf(value - knob->value) > 0.0001f) {
                knob->value = value;
                ui->needs_redraw = true;
                notify_host(ui, knob->port, knob->value);
            }
        }
    }
    pthread_mutex_unlock(&ui->mutex);
}

static void process_x_event(ChatterboxUI* ui, XEvent* event) {
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
    ChatterboxUI* ui = (ChatterboxUI*)data;
    while (ui->running) {
        while (XPending(ui->display) > 0) {
            XEvent event;
            XNextEvent(ui->display, &event);
            process_x_event(ui, &event);
        }

        if (ui->needs_redraw) {
            draw_ui(ui);
        }

        usleep(16000);  // ~60 FPS
    }
    return NULL;
}

static void setup_layout(ChatterboxUI* ui, int available_width) {
    memset(ui->groups, 0, sizeof(ui->groups));
    memset(ui->knob_used, 0, sizeof(ui->knob_used));

    const int row_count = 3;
    int row_heights[3] = {0};
    int row_widths[3] = {0};

    // Count controls per group
    for (size_t i = 0; i < sizeof(kControlInfo) / sizeof(kControlInfo[0]); i++) {
        GroupIndex group = kControlInfo[i].group;
        ui->groups[group].count++;
        ui->groups[group].row = kGroupLayout[group].row;
        ui->groups[group].columns = kGroupLayout[group].columns;
    }

    // Calculate group dimensions
    for (int g = 0; g < GROUP_COUNT; g++) {
        GroupState* group = &ui->groups[g];

        // Special handling for joystick group
        if (g == GROUP_JOYSTICK) {
            group->row = 0;
            group->width = 220;
            group->height = 220;
            int row = group->row;
            if (group->height > row_heights[row]) {
                row_heights[row] = group->height;
            }
            continue;
        }

        if (group->count == 0) continue;

        group->rows = (group->count + group->columns - 1) / group->columns;
        group->width = group->columns * (KNOB_SIZE + KNOB_SPACING_X) + 2 * GROUP_PADDING - KNOB_SPACING_X;
        group->height = TITLE_HEIGHT + group->rows * KNOB_HEIGHT +
                       (group->rows - 1) * KNOB_SPACING_Y + GROUP_PADDING;

        int row = group->row;
        if (group->height > row_heights[row]) {
            row_heights[row] = group->height;
        }
    }

    // Position groups
    int y_offset = 20;
    for (int row = 0; row < row_count; row++) {
        int x_offset = 20;
        for (int col = 0; col < 4 && kRowGroups[row][col] != GROUP_COUNT; col++) {
            GroupIndex g = kRowGroups[row][col];
            ui->groups[g].x = x_offset;
            ui->groups[g].y = y_offset;
            x_offset += ui->groups[g].width + GROUP_GAP_X;
        }
        y_offset += row_heights[row] + GROUP_GAP_Y;
    }

    // Set up joystick state
    ui->joystick.x = ui->groups[GROUP_JOYSTICK].x;
    ui->joystick.y = ui->groups[GROUP_JOYSTICK].y;
    ui->joystick.width = ui->groups[GROUP_JOYSTICK].width;
    ui->joystick.height = ui->groups[GROUP_JOYSTICK].height;
    ui->joystick.f1 = 0.50f;  // Default to schwa
    ui->joystick.f2 = 0.40f;
    ui->joystick.active = false;

    // Position controls within groups
    for (size_t i = 0; i < sizeof(kControlInfo) / sizeof(kControlInfo[0]); i++) {
        const ControlDesc* desc = &kControlInfo[i];
        GroupState* group = &ui->groups[desc->group];

        int col = group->assigned % group->columns;
        int row = group->assigned / group->columns;

        Knob* knob = &ui->knobs[desc->port];
        knob->port = desc->port;
        knob->label = desc->label;
        knob->min = desc->min;
        knob->max = desc->max;
        knob->def = desc->def;
        knob->value = desc->def;
        knob->is_toggle = desc->is_toggle;
        knob->width = KNOB_SIZE;
        knob->height = KNOB_HEIGHT;
        knob->x = group->x + GROUP_PADDING + col * (KNOB_SIZE + KNOB_SPACING_X);
        knob->y = group->y + TITLE_HEIGHT + row * (KNOB_HEIGHT + KNOB_SPACING_Y);

        ui->knob_used[desc->port] = true;
        group->assigned++;
    }

    // Initialize F1/F2 knobs (controlled by joystick, no UI elements)
    ui->knobs[PORT_F1].port = PORT_F1;
    ui->knobs[PORT_F1].min = 0.0f;
    ui->knobs[PORT_F1].max = 1.0f;
    ui->knobs[PORT_F1].value = ui->joystick.f1;
    ui->knobs[PORT_F2].port = PORT_F2;
    ui->knobs[PORT_F2].min = 0.0f;
    ui->knobs[PORT_F2].max = 1.0f;
    ui->knobs[PORT_F2].value = ui->joystick.f2;

    ui->content_width = ui->width;
    ui->content_height = y_offset;
}

// LV2 UI interface functions

static LV2UI_Handle instantiate(const LV2UI_Descriptor* descriptor,
                                const char* plugin_uri,
                                const char* bundle_path,
                                LV2UI_Write_Function write_function,
                                LV2UI_Controller controller,
                                LV2UI_Widget* widget,
                                const LV2_Feature* const* features) {
    (void)descriptor;
    (void)bundle_path;

    if (strcmp(plugin_uri, CHATTERBOX_URI) != 0) {
        fprintf(stderr, LOG_PREFIX "Wrong plugin URI: %s\n", plugin_uri);
        return NULL;
    }

    void* parent = NULL;
    for (const LV2_Feature* const* f = features; f && *f; ++f) {
        if (!strcmp((*f)->URI, LV2_UI__parent)) {
            parent = (*f)->data;
        }
    }

    if (!parent) {
        fprintf(stderr, LOG_PREFIX "No parent window provided\n");
        return NULL;
    }

    ensure_xlib_threads();

    ChatterboxUI* ui = (ChatterboxUI*)calloc(1, sizeof(ChatterboxUI));
    if (!ui) {
        return NULL;
    }

    ui->write = write_function;
    ui->controller = controller;
    ui->width = DEFAULT_WINDOW_WIDTH;
    ui->height = DEFAULT_WINDOW_HEIGHT;
    ui->active_knob = -1;
    ui->running = true;
    ui->needs_redraw = true;

    pthread_mutex_init(&ui->mutex, NULL);

    ui->display = XOpenDisplay(NULL);
    if (!ui->display) {
        fprintf(stderr, LOG_PREFIX "Cannot open X display\n");
        free(ui);
        return NULL;
    }

    ui->screen = DefaultScreen(ui->display);
    Window parent_window = (Window)(uintptr_t)parent;

    XSetWindowAttributes attr;
    attr.background_pixel = BlackPixel(ui->display, ui->screen);
    attr.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask |
                     ButtonReleaseMask | PointerMotionMask;

    ui->window = XCreateWindow(ui->display, parent_window,
                               0, 0, ui->width, ui->height, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWBackPixel | CWEventMask, &attr);

    ui->surface = cairo_xlib_surface_create(ui->display, ui->window,
                                            DefaultVisual(ui->display, ui->screen),
                                            ui->width, ui->height);

    setup_layout(ui, ui->width - 40);

    XMapWindow(ui->display, ui->window);
    XFlush(ui->display);

    pthread_create(&ui->thread, NULL, event_thread_main, ui);

    *widget = (LV2UI_Widget)(uintptr_t)ui->window;
    return ui;
}

static void cleanup(LV2UI_Handle instance) {
    ChatterboxUI* ui = (ChatterboxUI*)instance;

    ui->running = false;
    pthread_join(ui->thread, NULL);

    cairo_surface_destroy(ui->surface);
    XDestroyWindow(ui->display, ui->window);
    XCloseDisplay(ui->display);

    pthread_mutex_destroy(&ui->mutex);
    free(ui);
}

static void port_event(LV2UI_Handle instance,
                      uint32_t port_index,
                      uint32_t buffer_size,
                      uint32_t format,
                      const void* buffer) {
    ChatterboxUI* ui = (ChatterboxUI*)instance;
    (void)buffer_size;
    (void)format;

    if (port_index >= PORT_TOTAL_COUNT) {
        return;
    }

    pthread_mutex_lock(&ui->mutex);
    float value = *(const float*)buffer;
    if (fabsf(ui->knobs[port_index].value - value) > 0.0001f) {
        ui->knobs[port_index].value = value;

        // Sync joystick with F1/F2 changes
        if (port_index == PORT_F1) {
            ui->joystick.f1 = value;
        } else if (port_index == PORT_F2) {
            ui->joystick.f2 = value;
        }

        ui->needs_redraw = true;
    }
    pthread_mutex_unlock(&ui->mutex);
}

static const LV2UI_Descriptor descriptor = {
    CHATTERBOX_UI_URI,
    instantiate,
    cleanup,
    port_event,
    NULL
};

LV2_SYMBOL_EXPORT const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : NULL;
}
