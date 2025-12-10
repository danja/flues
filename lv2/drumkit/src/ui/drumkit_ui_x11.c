// drumkit_ui_x11.c
// X11/Cairo UI for hardcore industrial drumkit
// 4 rows: Kick+Snare, Toms+HiHats, Clap+Crash, Master

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

#define PLUGIN_URI "https://danja.github.io/flues/plugins/drumkit"
#define UI_URI PLUGIN_URI "#ui"

// Port indices (must match plugin)
typedef enum {
    PORT_AUDIO_OUT = 0,
    PORT_MIDI_IN,
    PORT_KICK_PITCH,
    PORT_KICK_DECAY,
    PORT_KICK_DRIVE,
    PORT_KICK_PUNCH,
    PORT_SNARE_TONE,
    PORT_SNARE_SNAP,
    PORT_CLAP_DENSITY,
    PORT_CLAP_TONE,
    PORT_TOM_PITCH,
    PORT_TOM_DECAY,
    PORT_HH_BRIGHTNESS,
    PORT_HH_DECAY,
    PORT_CRASH_BRIGHTNESS,
    PORT_CRASH_DECAY,
    PORT_BIT_CRUSH,
    PORT_MASTER_DRIVE,
    PORT_MASTER_REVERB,
    PORT_MASTER_GAIN,
    PORT_TOTAL_COUNT
} PortIndex;

// Group indices
typedef enum {
    GROUP_KICK = 0,
    GROUP_SNARE,
    GROUP_CLAP,
    GROUP_CRASH,
    GROUP_TOMS,
    GROUP_HIHATS,
    GROUP_MASTER,
    GROUP_COUNT
} GroupIndex;

// Layout constants
#define GROUP_PADDING 16
#define GROUP_GAP_X 18
#define GROUP_GAP_Y 26
#define TITLE_HEIGHT 20
#define KNOB_DIAMETER 70
#define KNOB_LABEL_HEIGHT 30
#define KNOB_SIZE 92
#define KNOB_HEIGHT 108
#define KNOB_SPACING_X 16
#define KNOB_SPACING_Y 18

// Control descriptor
typedef struct {
    GroupIndex group;
    const char* label;
    uint32_t port;
    float min, max, def;
} ControlDesc;

// Runtime knob state
typedef struct {
    uint32_t port;
    const char* label;
    float min, max, def, value;
    int x, y, width, height;
} Knob;

// Group layout state
typedef struct {
    int row;
    int columns;
    int count;
    int assigned;
    int rows;
    int x, y, width, height;
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

    int width, height;
    int content_width, content_height;

    Knob knobs[PORT_TOTAL_COUNT];
    bool knob_used[PORT_TOTAL_COUNT];

    GroupState groups[GROUP_COUNT];

    volatile bool needs_redraw;
    int active_knob;
    double drag_start_y;
    float drag_start_value;
} DrumkitUI;

// Control definitions (18 parameters)
static const ControlDesc kControls[] = {
    // Kick (4 params)
    { GROUP_KICK, "PITCH", PORT_KICK_PITCH, 0.0f, 1.0f, 0.35f },
    { GROUP_KICK, "DECAY", PORT_KICK_DECAY, 0.0f, 1.0f, 0.4f },
    { GROUP_KICK, "DRIVE", PORT_KICK_DRIVE, 0.0f, 1.0f, 0.3f },
    { GROUP_KICK, "PUNCH", PORT_KICK_PUNCH, 0.0f, 1.0f, 0.15f },

    // Snare (2 params)
    { GROUP_SNARE, "TONE", PORT_SNARE_TONE, 0.0f, 1.0f, 0.5f },
    { GROUP_SNARE, "SNAP", PORT_SNARE_SNAP, 0.0f, 1.0f, 0.6f },

    // Clap (2 params)
    { GROUP_CLAP, "DENSITY", PORT_CLAP_DENSITY, 0.0f, 1.0f, 0.55f },
    { GROUP_CLAP, "TONE", PORT_CLAP_TONE, 0.0f, 1.0f, 0.45f },

    // Crash (2 params)
    { GROUP_CRASH, "BRIGHT", PORT_CRASH_BRIGHTNESS, 0.0f, 1.0f, 0.65f },
    { GROUP_CRASH, "DECAY", PORT_CRASH_DECAY, 0.0f, 1.0f, 0.5f },

    // Toms (2 params)
    { GROUP_TOMS, "PITCH", PORT_TOM_PITCH, 0.0f, 1.0f, 0.4f },
    { GROUP_TOMS, "DECAY", PORT_TOM_DECAY, 0.0f, 1.0f, 0.45f },

    // Hi-Hats (2 params)
    { GROUP_HIHATS, "BRIGHT", PORT_HH_BRIGHTNESS, 0.0f, 1.0f, 0.6f },
    { GROUP_HIHATS, "DECAY", PORT_HH_DECAY, 0.0f, 1.0f, 0.35f },

    // Master (4 params)
    { GROUP_MASTER, "CRUSH", PORT_BIT_CRUSH, 0.0f, 1.0f, 0.0f },
    { GROUP_MASTER, "DRIVE", PORT_MASTER_DRIVE, 0.0f, 1.0f, 0.25f },
    { GROUP_MASTER, "REVERB", PORT_MASTER_REVERB, 0.0f, 1.0f, 0.2f },
    { GROUP_MASTER, "GAIN", PORT_MASTER_GAIN, 0.0f, 1.0f, 0.7f }
};

static const int kControlCount = sizeof(kControls) / sizeof(kControls[0]);

// Group names
static const char* kGroupNames[GROUP_COUNT] = {
    "KICK", "SNARE", "CLAP", "CRASH", "TOMS", "HI-HATS", "MASTER"
};

// Row layout (which groups in which row)
static const GroupIndex kRowGroups[][3] = {
    { GROUP_KICK, GROUP_SNARE, GROUP_COUNT },              // Row 0
    { GROUP_TOMS, GROUP_HIHATS, GROUP_COUNT },             // Row 1
    { GROUP_CLAP, GROUP_CRASH, GROUP_COUNT },              // Row 2
    { GROUP_MASTER, GROUP_COUNT, GROUP_COUNT }             // Row 3
};

static const int kRowCount = sizeof(kRowGroups) / sizeof(kRowGroups[0]);

// Group columns
static const int kGroupColumns[GROUP_COUNT] = {
    4,  // Kick: 4 columns
    2,  // Snare: 2 columns
    2,  // Clap: 2 columns
    2,  // Crash: 2 columns
    2,  // Toms: 2 columns
    2,  // Hi-Hats: 2 columns
    4   // Master: 4 columns
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

static float clamp_value(const Knob* knob, float value) {
    if (value < knob->min) return knob->min;
    if (value > knob->max) return knob->max;
    return value;
}

static void notify_host(DrumkitUI* ui, uint32_t port, float value) {
    if (ui->write) {
        ui->write(ui->controller, port, sizeof(float), 0, &value);
    }
}

// ===== Drawing Functions =====

static void draw_group_background(cairo_t* cr, const GroupState* group, const char* title) {
    const int x = group->x;
    const int y = group->y;
    const int w = group->width;
    const int h = group->height;

    // Dark background
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source_rgb(cr, 0.14, 0.15, 0.19);
    cairo_fill(cr);

    // Border
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source_rgb(cr, 0.30, 0.31, 0.37);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);

    // Title
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12.0);
    cairo_set_source_rgb(cr, 0.95, 0.82, 0.46);
    cairo_move_to(cr, x + GROUP_PADDING, y + GROUP_PADDING + 10);
    cairo_show_text(cr, title);
}

static void draw_knob(cairo_t* cr, const Knob* knob) {
    const double cx = knob->x + knob->width / 2.0;
    const double cy = knob->y + knob->height / 2.0 - 8.0;
    const double radius = KNOB_DIAMETER / 2.0;

    // Outer ring
    cairo_arc(cr, cx, cy, radius, 0, 2 * M_PI);
    cairo_set_source_rgb(cr, 0.16, 0.18, 0.22);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, 2.0);
    cairo_set_source_rgb(cr, 0.82, 0.48, 0.18);
    cairo_stroke(cr);

    // Inner circle
    cairo_arc(cr, cx, cy, radius * 0.72, 0, 2 * M_PI);
    cairo_set_source_rgb(cr, 0.21, 0.23, 0.28);
    cairo_fill(cr);

    // Tick marks
    for (int i = 0; i < 11; ++i) {
        double t = (double)i / 10.0;
        double angle = (1.5 * M_PI * t) + (0.75 * M_PI);
        double inner = radius * 0.82;
        double outer = radius * 0.92;

        cairo_set_line_width(cr, 1.5);
        cairo_set_source_rgb(cr, 0.4, 0.42, 0.48);
        cairo_move_to(cr, cx + cos(angle) * inner, cy + sin(angle) * inner);
        cairo_line_to(cr, cx + cos(angle) * outer, cy + sin(angle) * outer);
        cairo_stroke(cr);
    }

    // Value indicator
    double norm = (knob->value - knob->min) / (knob->max - knob->min);
    double angle = (norm * 1.5 * M_PI) + (0.75 * M_PI);
    double inner = radius * 0.3;
    double outer = radius * 0.85;

    cairo_set_line_width(cr, 4.0);
    cairo_set_source_rgb(cr, 0.97, 0.63, 0.26);
    cairo_move_to(cr, cx + cos(angle) * inner, cy + sin(angle) * inner);
    cairo_line_to(cr, cx + cos(angle) * outer, cy + sin(angle) * outer);
    cairo_stroke(cr);

    // Value text
    char value_str[16];
    snprintf(value_str, sizeof(value_str), "%.2f", knob->value);
    cairo_text_extents_t extents;

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_text_extents(cr, value_str, &extents);
    cairo_move_to(cr, cx - extents.width / 2.0, cy + radius * 0.46);
    cairo_show_text(cr, value_str);

    // Label
    cairo_set_font_size(cr, 10.0);
    cairo_text_extents(cr, knob->label, &extents);
    cairo_move_to(cr, cx - extents.width / 2.0, knob->y + knob->height - 7.0);
    cairo_show_text(cr, knob->label);
}

static void draw_ui(DrumkitUI* ui) {
    if (!ui->surface) return;

    cairo_t* cr = cairo_create(ui->surface);

    // Clear background
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.12);
    cairo_paint(cr);

    // Draw groups
    for (int g = 0; g < GROUP_COUNT; ++g) {
        if (ui->groups[g].count > 0) {
            draw_group_background(cr, &ui->groups[g], kGroupNames[g]);
        }
    }

    // Draw knobs
    for (int i = 0; i < PORT_TOTAL_COUNT; ++i) {
        if (ui->knob_used[i]) {
            draw_knob(cr, &ui->knobs[i]);
        }
    }

    cairo_destroy(cr);

    // Force redraw - don't clear window
    XSync(ui->display, False);

    pthread_mutex_lock(&ui->mutex);
    ui->needs_redraw = false;
    pthread_mutex_unlock(&ui->mutex);
}

// ===== Layout Calculation =====

static void setup_layout(DrumkitUI* ui, int available_width) {
    // Count controls per group
    for (int i = 0; i < kControlCount; ++i) {
        ui->groups[kControls[i].group].count++;
    }

    // Set columns per group and calculate dimensions
    int row_heights[4] = {0, 0, 0, 0};

    for (int g = 0; g < GROUP_COUNT; ++g) {
        GroupState* group = &ui->groups[g];
        if (group->count == 0) continue;

        group->columns = kGroupColumns[g];
        group->rows = (group->count + group->columns - 1) / group->columns;

        group->width = (GROUP_PADDING * 2) + group->columns * KNOB_SIZE +
                       (group->columns - 1) * KNOB_SPACING_X;
        group->height = GROUP_PADDING + TITLE_HEIGHT +
                        group->rows * KNOB_HEIGHT +
                        (group->rows - 1) * KNOB_SPACING_Y + GROUP_PADDING;

        // Track max height for this group's row
        for (int r = 0; r < 4; ++r) {
            for (int gi = 0; gi < 3 && kRowGroups[r][gi] != GROUP_COUNT; ++gi) {
                if (kRowGroups[r][gi] == g) {
                    if (group->height > row_heights[r]) {
                        row_heights[r] = group->height;
                    }
                }
            }
        }
    }

    // Position groups
    int current_y = 20;
    for (int r = 0; r < 4; ++r) {
        int row_width = 0;
        int group_count = 0;

        // Calculate row width
        for (int gi = 0; gi < 3 && kRowGroups[r][gi] != GROUP_COUNT; ++gi) {
            GroupState* group = &ui->groups[kRowGroups[r][gi]];
            if (group->count > 0) {
                row_width += group->width;
                group_count++;
            }
        }
        if (group_count > 0) {
            row_width += (group_count - 1) * GROUP_GAP_X;
        }

        // Center row
        int start_x = (available_width - row_width) / 2;
        int current_x = start_x;

        // Position groups in row
        for (int gi = 0; gi < 3 && kRowGroups[r][gi] != GROUP_COUNT; ++gi) {
            GroupState* group = &ui->groups[kRowGroups[r][gi]];
            if (group->count > 0) {
                group->x = current_x;
                group->y = current_y;
                group->row = r;
                current_x += group->width + GROUP_GAP_X;
            }
        }

        current_y += row_heights[r] + GROUP_GAP_Y;
    }

    // Position knobs within groups
    for (int i = 0; i < kControlCount; ++i) {
        const ControlDesc* desc = &kControls[i];
        GroupState* group = &ui->groups[desc->group];

        Knob* knob = &ui->knobs[desc->port];
        knob->port = desc->port;
        knob->label = desc->label;
        knob->min = desc->min;
        knob->max = desc->max;
        knob->def = desc->def;
        knob->value = desc->def;

        int col = group->assigned % group->columns;
        int row = group->assigned / group->columns;

        knob->x = group->x + GROUP_PADDING + col * (KNOB_SIZE + KNOB_SPACING_X);
        knob->y = group->y + GROUP_PADDING + TITLE_HEIGHT + row * (KNOB_HEIGHT + KNOB_SPACING_Y);
        knob->width = KNOB_SIZE;
        knob->height = KNOB_HEIGHT;

        ui->knob_used[desc->port] = true;
        group->assigned++;
    }

    // Calculate content size
    ui->content_width = available_width;
    ui->content_height = current_y;
}

// ===== Event Handling =====

static int find_knob_at(DrumkitUI* ui, int x, int y) {
    for (int i = 0; i < PORT_TOTAL_COUNT; ++i) {
        if (!ui->knob_used[i]) continue;

        const Knob* knob = &ui->knobs[i];
        if (x >= knob->x && x < knob->x + knob->width &&
            y >= knob->y && y < knob->y + knob->height) {
            return i;
        }
    }
    return -1;
}

static void handle_button_press(DrumkitUI* ui, const XButtonEvent* event) {
    pthread_mutex_lock(&ui->mutex);

    int knob_index = find_knob_at(ui, event->x, event->y);
    if (knob_index >= 0) {
        ui->active_knob = knob_index;
        ui->drag_start_y = event->y;
        ui->drag_start_value = ui->knobs[knob_index].value;
    }

    pthread_mutex_unlock(&ui->mutex);
}

static void handle_button_release(DrumkitUI* ui, const XButtonEvent* event) {
    (void)event;
    pthread_mutex_lock(&ui->mutex);
    ui->active_knob = -1;
    pthread_mutex_unlock(&ui->mutex);
}

static void handle_motion(DrumkitUI* ui, const XMotionEvent* event) {
    pthread_mutex_lock(&ui->mutex);

    if (ui->active_knob >= 0) {
        Knob* knob = &ui->knobs[ui->active_knob];
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

static void handle_scroll(DrumkitUI* ui, const XButtonEvent* event) {
    int knob_index = find_knob_at(ui, event->x, event->y);
    if (knob_index < 0) return;

    pthread_mutex_lock(&ui->mutex);

    Knob* knob = &ui->knobs[knob_index];
    float step = (knob->max - knob->min) / 100.0f;
    float value = knob->value;

    if (event->button == Button4) {  // Scroll up
        value += step * 4.0f;
    } else if (event->button == Button5) {  // Scroll down
        value -= step * 4.0f;
    }

    value = clamp_value(knob, value);
    if (fabsf(value - knob->value) > 0.0001f) {
        knob->value = value;
        ui->needs_redraw = true;
        notify_host(ui, knob->port, value);
    }

    pthread_mutex_unlock(&ui->mutex);
}

static void process_x_event(DrumkitUI* ui, const XEvent* event) {
    switch (event->type) {
        case Expose:
            if (event->xexpose.count == 0) {
                draw_ui(ui);
            }
            break;

        case ConfigureNotify:
            if (ui->surface) {
                cairo_xlib_surface_set_size(ui->surface,
                    event->xconfigure.width, event->xconfigure.height);
            }
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
    }
}

static void* event_thread_main(void* data) {
    DrumkitUI* ui = (DrumkitUI*)data;

    // Force initial draw
    draw_ui(ui);

    while (ui->running) {
        // Process X events
        while (XPending(ui->display) > 0) {
            XEvent event;
            XNextEvent(ui->display, &event);
            process_x_event(ui, &event);
        }

        // Redraw if needed
        pthread_mutex_lock(&ui->mutex);
        bool need_draw = ui->needs_redraw;
        pthread_mutex_unlock(&ui->mutex);

        if (need_draw) {
            draw_ui(ui);
        }

        usleep(16000);  // ~60 FPS
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

    DrumkitUI* ui = (DrumkitUI*)calloc(1, sizeof(DrumkitUI));
    pthread_mutex_init(&ui->mutex, NULL);
    ui->write = write_function;
    ui->controller = controller;
    ui->active_knob = -1;

    // Open display
    ui->display = XOpenDisplay(NULL);
    if (!ui->display) {
        free(ui);
        return NULL;
    }
    ui->screen = DefaultScreen(ui->display);

    // Find parent window
    Window parent = DefaultRootWindow(ui->display);
    for (int i = 0; features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_UI__parent)) {
            parent = (Window)(uintptr_t)features[i]->data;
        }
    }

    // Setup layout
    const int default_width = 820;
    setup_layout(ui, default_width);
    ui->width = default_width;
    ui->height = ui->content_height + 20;

    // Create window
    XSetWindowAttributes attrs;
    attrs.event_mask = ExposureMask | StructureNotifyMask |
                       ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
    attrs.background_pixel = BlackPixel(ui->display, ui->screen);

    ui->window = XCreateWindow(ui->display, parent, 0, 0, ui->width, ui->height, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWBackPixel | CWEventMask, &attrs);

    XMapWindow(ui->display, ui->window);

    // Create Cairo surface
    ui->surface = cairo_xlib_surface_create(ui->display, ui->window,
                                            DefaultVisual(ui->display, ui->screen),
                                            ui->width, ui->height);

    // Start event thread
    ui->running = true;
    pthread_create(&ui->thread, NULL, event_thread_main, ui);

    *widget = (LV2UI_Widget)(uintptr_t)ui->window;
    return ui;
}

static void ui_cleanup(LV2UI_Handle handle) {
    DrumkitUI* ui = (DrumkitUI*)handle;

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
    DrumkitUI* ui = (DrumkitUI*)handle;
    if (!ui || !buffer || format != 0) return;

    float value = *((const float*)buffer);

    pthread_mutex_lock(&ui->mutex);
    if (port_index < PORT_TOTAL_COUNT && ui->knob_used[port_index]) {
        Knob* knob = &ui->knobs[port_index];
        value = clamp_value(knob, value);
        if (fabsf(value - knob->value) > 0.0001f) {
            knob->value = value;
            ui->needs_redraw = true;
        }
    }
    pthread_mutex_unlock(&ui->mutex);
}

// LV2 UI descriptor
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
