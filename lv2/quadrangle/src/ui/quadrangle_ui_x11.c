#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <lv2/core/lv2.h>
#include <lv2/ui/ui.h>

#define QUADRANGLE_UI_URI "https://danja.github.io/flues/plugins/quadrangle#ui"

#define GRID_SIZE 8
#define PAD_SIZE 40
#define PAD_GAP 4
#define MARGIN 20
#define SIDE_WIDTH 30
#define TOP_HEIGHT 30
#define INFO_HEIGHT 60

#define WINDOW_WIDTH (MARGIN * 2 + GRID_SIZE * (PAD_SIZE + PAD_GAP) + SIDE_WIDTH)
#define WINDOW_HEIGHT (MARGIN * 2 + GRID_SIZE * (PAD_SIZE + PAD_GAP) + TOP_HEIGHT + INFO_HEIGHT)

// Color definitions
typedef struct {
    double r, g, b;
} Color;

static const Color COLOR_BG = {0.1, 0.1, 0.12};
static const Color COLOR_PAD_OFF = {0.15, 0.15, 0.18};
static const Color COLOR_PAD_RED = {0.9, 0.2, 0.2};
static const Color COLOR_PAD_BLUE = {0.2, 0.4, 0.9};
static const Color COLOR_PAD_GREEN = {0.2, 0.9, 0.3};
static const Color COLOR_PAD_PURPLE = {0.7, 0.3, 0.9};
static const Color COLOR_PLAYHEAD = {0.3, 1.0, 0.4};
static const Color COLOR_TEXT = {0.9, 0.9, 0.9};
static const Color COLOR_BUTTON_ON = {1.0, 0.8, 0.0};

// UI State
typedef struct {
    uint8_t grid[GRID_SIZE][GRID_SIZE];  // 0=off, 1-4=quadrant colors, 5=playhead
    uint8_t side_buttons[GRID_SIZE];     // 0=off, 1=on
    uint8_t top_buttons[9];              // 0=off, 1=on
    uint8_t playing;
    uint8_t pattern;
    uint8_t selected_voice;
    uint16_t bpm;
    uint8_t current_step;
} UIState;

typedef struct {
    Display *display;
    Window window;
    cairo_surface_t *surface;
    cairo_t *cr;

    // Double buffering
    cairo_surface_t *back_buffer;
    cairo_t *back_cr;

    int needs_redraw;

    UIState state;

    LV2UI_Write_Function write_function;
    LV2UI_Controller controller;

} QuadrangleUI;

// ============================================================================
// Drawing Functions
// ============================================================================

static void draw_pad(cairo_t *cr, double x, double y, double size, Color color, double brightness) {
    // Outer glow
    cairo_pattern_t *glow = cairo_pattern_create_radial(
        x + size/2, y + size/2, 0,
        x + size/2, y + size/2, size * 0.6
    );
    cairo_pattern_add_color_stop_rgba(glow, 0, color.r, color.g, color.b, brightness * 0.3);
    cairo_pattern_add_color_stop_rgba(glow, 1, color.r, color.g, color.b, 0);
    cairo_set_source(cr, glow);
    cairo_rectangle(cr, x - 4, y - 4, size + 8, size + 8);
    cairo_fill(cr);
    cairo_pattern_destroy(glow);

    // Pad background
    cairo_rectangle(cr, x, y, size, size);
    cairo_set_source_rgb(cr, color.r * 0.3, color.g * 0.3, color.b * 0.3);
    cairo_fill(cr);

    // Pad light
    if (brightness > 0.01) {
        cairo_pattern_t *gradient = cairo_pattern_create_radial(
            x + size * 0.3, y + size * 0.3, size * 0.1,
            x + size * 0.5, y + size * 0.5, size * 0.7
        );
        cairo_pattern_add_color_stop_rgb(gradient, 0,
            fmin(1.0, color.r * brightness * 1.3),
            fmin(1.0, color.g * brightness * 1.3),
            fmin(1.0, color.b * brightness * 1.3)
        );
        cairo_pattern_add_color_stop_rgb(gradient, 1,
            color.r * brightness * 0.5,
            color.g * brightness * 0.5,
            color.b * brightness * 0.5
        );
        cairo_set_source(cr, gradient);
        cairo_rectangle(cr, x, y, size, size);
        cairo_fill(cr);
        cairo_pattern_destroy(gradient);
    }

    // Border
    cairo_rectangle(cr, x, y, size, size);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.1);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
}

static Color get_quadrant_color(int row, int col) {
    if (row >= 4) {
        return (col < 4) ? COLOR_PAD_RED : COLOR_PAD_BLUE;
    } else {
        return (col < 4) ? COLOR_PAD_GREEN : COLOR_PAD_PURPLE;
    }
}

static void draw_grid(QuadrangleUI *ui) {
    cairo_t *cr = ui->back_cr;

    // Draw grid
    for (int row = 0; row < GRID_SIZE; row++) {
        for (int col = 0; col < GRID_SIZE; col++) {
            double x = MARGIN + col * (PAD_SIZE + PAD_GAP);
            double y = MARGIN + TOP_HEIGHT + (GRID_SIZE - 1 - row) * (PAD_SIZE + PAD_GAP);

            uint8_t value = ui->state.grid[row][col];
            Color color;
            double brightness;

            if (value == 5) {  // Playhead
                color = COLOR_PLAYHEAD;
                brightness = 1.0;
            } else if (value > 0) {
                color = get_quadrant_color(row, col);
                brightness = 0.8;
            } else {
                color = COLOR_PAD_OFF;
                brightness = 0.3;
            }

            draw_pad(cr, x, y, PAD_SIZE, color, brightness);
        }
    }

    // Draw side buttons
    for (int i = 0; i < GRID_SIZE; i++) {
        double x = MARGIN + GRID_SIZE * (PAD_SIZE + PAD_GAP) + PAD_GAP;
        double y = MARGIN + TOP_HEIGHT + (GRID_SIZE - 1 - i) * (PAD_SIZE + PAD_GAP);

        Color color = (i == ui->state.selected_voice) ? COLOR_BUTTON_ON : COLOR_PAD_RED;
        double brightness = ui->state.side_buttons[i] ? 0.9 : 0.3;

        draw_pad(cr, x, y, SIDE_WIDTH - 4, color, brightness);
    }

    // Draw top buttons
    for (int i = 0; i < 8; i++) {
        double x = MARGIN + i * (PAD_SIZE + PAD_GAP);
        double y = MARGIN;

        Color color = COLOR_BUTTON_ON;
        if (i >= 2 && i <= 5) {  // Pattern buttons
            color = (i - 2 == ui->state.pattern) ? COLOR_BUTTON_ON : COLOR_PAD_OFF;
        }
        double brightness = ui->state.top_buttons[i] ? 0.9 : 0.3;

        draw_pad(cr, x, y, TOP_HEIGHT - 4, color, brightness);
    }

    // Draw info panel
    double info_y = MARGIN + TOP_HEIGHT + GRID_SIZE * (PAD_SIZE + PAD_GAP) + PAD_GAP;
    cairo_set_source_rgb(cr, COLOR_TEXT.r, COLOR_TEXT.g, COLOR_TEXT.b);
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14);

    char info[256];
    snprintf(info, sizeof(info),
        "%s | Pattern %c | Voice %d | BPM %d | Step %d/16",
        ui->state.playing ? "▶ PLAYING" : "⏸ STOPPED",
        'A' + ui->state.pattern,
        ui->state.selected_voice + 1,
        ui->state.bpm,
        ui->state.current_step + 1
    );

    cairo_move_to(cr, MARGIN, info_y + 20);
    cairo_show_text(cr, info);

    // Draw quadrant labels
    cairo_set_font_size(cr, 10);
    cairo_set_source_rgba(cr, COLOR_TEXT.r, COLOR_TEXT.g, COLOR_TEXT.b, 0.5);

    cairo_move_to(cr, MARGIN + 5, info_y + 40);
    cairo_show_text(cr, "DRUMS");

    cairo_move_to(cr, MARGIN + GRID_SIZE/2 * (PAD_SIZE + PAD_GAP) + 5, info_y + 40);
    cairo_show_text(cr, "MELODY");

    cairo_move_to(cr, MARGIN + 5, info_y + 55);
    cairo_show_text(cr, "LIVE");

    cairo_move_to(cr, MARGIN + GRID_SIZE/2 * (PAD_SIZE + PAD_GAP) + 5, info_y + 55);
    cairo_show_text(cr, "PARAMS");
}

static void redraw(QuadrangleUI *ui) {
    if (!ui->back_cr) return;

    // Clear back buffer
    cairo_set_source_rgb(ui->back_cr, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b);
    cairo_paint(ui->back_cr);

    // Draw grid to back buffer
    draw_grid(ui);

    // Copy back buffer to screen
    cairo_set_source_surface(ui->cr, ui->back_buffer, 0, 0);
    cairo_paint(ui->cr);

    // Flush to X11
    cairo_surface_flush(ui->surface);
    XFlush(ui->display);
}

// ============================================================================
// Idle Callback
// ============================================================================

static int idle(LV2UI_Handle handle) {
    QuadrangleUI *ui = (QuadrangleUI*)handle;

    // Process X11 events
    while (XPending(ui->display) > 0) {
        XEvent event;
        XNextEvent(ui->display, &event);

        switch (event.type) {
            case ButtonPress:
                // TODO: handle mouse clicks
                break;

            case Expose:
                ui->needs_redraw = 1;
                break;
        }
    }

    // Redraw if needed
    if (ui->needs_redraw) {
        redraw(ui);
        ui->needs_redraw = 0;
    }

    return 0;
}

// ============================================================================
// LV2 UI Callbacks
// ============================================================================

static LV2UI_Handle instantiate(const LV2UI_Descriptor *descriptor,
                                 const char *plugin_uri,
                                 const char *bundle_path,
                                 LV2UI_Write_Function write_function,
                                 LV2UI_Controller controller,
                                 LV2UI_Widget *widget,
                                 const LV2_Feature *const *features) {
    (void)descriptor;
    (void)plugin_uri;
    (void)bundle_path;

    QuadrangleUI *ui = (QuadrangleUI*)calloc(1, sizeof(QuadrangleUI));
    if (!ui) return NULL;

    ui->write_function = write_function;
    ui->controller = controller;

    // Initialize state
    memset(&ui->state, 0, sizeof(UIState));
    ui->state.bpm = 120;
    ui->needs_redraw = 1;

    // Initialize X11
    ui->display = XOpenDisplay(NULL);
    if (!ui->display) {
        free(ui);
        return NULL;
    }

    int screen = DefaultScreen(ui->display);

    ui->window = XCreateSimpleWindow(
        ui->display,
        RootWindow(ui->display, screen),
        0, 0,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        1,
        BlackPixel(ui->display, screen),
        BlackPixel(ui->display, screen)
    );

    XStoreName(ui->display, ui->window, "Quadrangle");

    XSelectInput(ui->display, ui->window, ExposureMask | StructureNotifyMask);
    XMapWindow(ui->display, ui->window);

    // Create Cairo surface
    Visual *visual = DefaultVisual(ui->display, screen);
    ui->surface = cairo_xlib_surface_create(
        ui->display,
        ui->window,
        visual,
        WINDOW_WIDTH, WINDOW_HEIGHT
    );

    ui->cr = cairo_create(ui->surface);

    // Create back buffer for double buffering
    ui->back_buffer = cairo_image_surface_create(CAIRO_FORMAT_RGB24, WINDOW_WIDTH, WINDOW_HEIGHT);
    ui->back_cr = cairo_create(ui->back_buffer);

    // Initial draw
    redraw(ui);

    *widget = (LV2UI_Widget)(intptr_t)ui->window;

    return (LV2UI_Handle)ui;
}

static void cleanup(LV2UI_Handle handle) {
    QuadrangleUI *ui = (QuadrangleUI*)handle;
    if (!ui) return;

    // Clean up Cairo resources first
    if (ui->back_cr) {
        cairo_destroy(ui->back_cr);
        ui->back_cr = NULL;
    }
    if (ui->back_buffer) {
        cairo_surface_destroy(ui->back_buffer);
        ui->back_buffer = NULL;
    }
    if (ui->cr) {
        cairo_destroy(ui->cr);
        ui->cr = NULL;
    }
    if (ui->surface) {
        cairo_surface_flush(ui->surface);
        cairo_surface_destroy(ui->surface);
        ui->surface = NULL;
    }

    // Clean up X11 resources
    if (ui->display) {
        if (ui->window) {
            XUnmapWindow(ui->display, ui->window);
            XSync(ui->display, False);
            XDestroyWindow(ui->display, ui->window);
            ui->window = 0;
        }
        XCloseDisplay(ui->display);
        ui->display = NULL;
    }

    free(ui);
}

static void port_event(LV2UI_Handle handle,
                       uint32_t port_index,
                       uint32_t buffer_size,
                       uint32_t format,
                       const void *buffer) {
    // For future: receive state updates from plugin
    (void)handle;
    (void)port_index;
    (void)buffer_size;
    (void)format;
    (void)buffer;
}

static const LV2UI_Idle_Interface idle_interface = {
    idle
};

static const void* extension_data(const char *uri) {
    if (!strcmp(uri, LV2_UI__idleInterface)) {
        return &idle_interface;
    }
    return NULL;
}

// ============================================================================
// LV2 UI Descriptor
// ============================================================================

static const LV2UI_Descriptor descriptor = {
    QUADRANGLE_UI_URI,
    instantiate,
    cleanup,
    port_event,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : NULL;
}
