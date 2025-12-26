#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>
#include <lv2/ui/ui.h>
#include "../include/quadrangle_ui_state.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo.h>
#include <cairo-xlib.h>

#define QUADRANGLE_UI_URI "https://danja.github.io/flues/plugins/quadrangle#ui"

#define GRID_SIZE 8
#define PAD_SIZE 40
#define PAD_GAP 4
#define MARGIN 20
#define SIDE_WIDTH 30
#define TOP_HEIGHT 30
#define INFO_HEIGHT 60
#define PORT_PLAY_STATE 5  // Matches quadrangle.ttl lv2:index for play_state
#define PORT_CURRENT_STEP 6

#define WINDOW_WIDTH (MARGIN * 2 + GRID_SIZE * (PAD_SIZE + PAD_GAP) + SIDE_WIDTH)
#define WINDOW_HEIGHT (MARGIN * 2 + GRID_SIZE * (PAD_SIZE + PAD_GAP) + TOP_HEIGHT + INFO_HEIGHT)

static const uint8_t GRID_NOTES[GRID_SIZE][GRID_SIZE] = {
    {11, 12, 13, 14, 15, 16, 17, 18},
    {21, 22, 23, 24, 25, 26, 27, 28},
    {31, 32, 33, 34, 35, 36, 37, 38},
    {41, 42, 43, 44, 45, 46, 47, 48},
    {51, 52, 53, 54, 55, 56, 57, 58},
    {61, 62, 63, 64, 65, 66, 67, 68},
    {71, 72, 73, 74, 75, 76, 77, 78},
    {81, 82, 83, 84, 85, 86, 87, 88}
};

static const uint8_t SIDE_BUTTONS[GRID_SIZE] = {19, 29, 39, 49, 59, 69, 79, 89};
static const uint8_t TOP_BUTTONS[9] = {91, 92, 93, 94, 95, 96, 97, 98, 99};

static inline uint8_t grid_to_note(uint8_t row, uint8_t col) {
    if (row >= GRID_SIZE || col >= GRID_SIZE) return 0;
    return GRID_NOTES[row][col];
}

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
    int width;
    int height;
    int screen;
    Window parent;
    pthread_t thread;
    pthread_mutex_t mutex;
    int running;

    UIState state;
    int hover_active;
    int hover_row;
    int hover_col;
    char hover_text[128];

    LV2UI_Write_Function write_function;
    LV2UI_Controller controller;
    LV2_URID_Map* map;
    LV2_Atom_Forge forge;
    LV2_URID atom_event_transfer;
    LV2_URID midi_event_urid;
    LV2_URID atom_chunk_urid;

} QuadrangleUI;

// Xlib threading guard (match pm-synth UI pattern to avoid host crashes)
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

            // Playhead overlay for sequencer quadrants
            int playhead_row = ui->state.current_step / 4;
            int playhead_col = ui->state.current_step % 4;
            int is_drum_head = (row >= 4 && row - 4 == playhead_row && col < 4 && col == playhead_col);
            int is_melody_head = (row >= 4 && row - 4 == playhead_row && col >= 4 && (col - 4) == playhead_col);

            if (ui->state.playing && (is_drum_head || is_melody_head)) {
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

    if (ui->hover_active && ui->hover_text[0] != '\0') {
        cairo_set_font_size(cr, 11);
        cairo_set_source_rgba(cr, COLOR_TEXT.r, COLOR_TEXT.g, COLOR_TEXT.b, 0.8);
        cairo_move_to(cr, MARGIN, info_y + 72);
        cairo_show_text(cr, ui->hover_text);
    }
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
    if (!ui || !ui->display || !ui->window) {
        return 1;
    }

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
// Event Thread (mirrors euclid/pm-synth stability pattern)
// ============================================================================

static void *event_thread_main(void *arg) {
    QuadrangleUI *ui = (QuadrangleUI*)arg;
    static const uint8_t bottom_ccs[4] = {74, 71, 1, 27};
    static const char *scale_names[4] = {"Chromatic", "Major", "Minor", "Pentatonic"};
    static const char *bank_names[4] = {"Degrees 1-4", "Degrees 5-8",
                                        "Degrees 9-12", "Degrees 13-16"};

    while (ui->running) {
        while (XPending(ui->display)) {
            XEvent event;
            XNextEvent(ui->display, &event);

            switch (event.type) {
                case Expose:
                    ui->needs_redraw = 1;
                    break;
                case ConfigureNotify: {
                    ui->width = event.xconfigure.width;
                    ui->height = event.xconfigure.height;

                    if (ui->surface) {
                        cairo_surface_destroy(ui->surface);
                    }
                    Visual *visual = DefaultVisual(ui->display, ui->screen);
                    ui->surface = cairo_xlib_surface_create(
                        ui->display,
                        ui->window,
                        visual,
                        ui->width, ui->height
                    );
                    if (ui->cr) {
                        cairo_destroy(ui->cr);
                    }
                    ui->cr = cairo_create(ui->surface);

                    if (ui->back_cr) {
                        cairo_destroy(ui->back_cr);
                        ui->back_cr = NULL;
                    }
                    if (ui->back_buffer) {
                        cairo_surface_destroy(ui->back_buffer);
                        ui->back_buffer = NULL;
                    }
                    ui->back_buffer = cairo_image_surface_create(CAIRO_FORMAT_RGB24, ui->width, ui->height);
                    ui->back_cr = cairo_create(ui->back_buffer);
                    ui->needs_redraw = 1;
                    break;
                }
                case MotionNotify: {
                    int x = event.xmotion.x;
                    int y = event.xmotion.y;
                    ui->hover_active = 0;
                    ui->hover_text[0] = '\0';

                    int grid_x = x - MARGIN;
                    int grid_y = y - (MARGIN + TOP_HEIGHT);
                    if (grid_x >= 0 && grid_y >= 0) {
                        int cell = PAD_SIZE + PAD_GAP;
                        int col = grid_x / cell;
                        int row_from_top = grid_y / cell;
                        int in_pad_x = (grid_x % cell) < PAD_SIZE;
                        int in_pad_y = (grid_y % cell) < PAD_SIZE;
                        if (col >= 0 && col < GRID_SIZE && row_from_top >= 0 && row_from_top < GRID_SIZE &&
                            in_pad_x && in_pad_y) {
                            int row = (GRID_SIZE - 1) - row_from_top;
                            ui->hover_active = 1;
                            ui->hover_row = row;
                            ui->hover_col = col;

                            if (row >= 4 && col < 4) {
                                snprintf(ui->hover_text, sizeof(ui->hover_text),
                                         "Drums: step %d (voice via side buttons)", (row - 4) * 4 + col + 1);
                            } else if (row >= 4 && col >= 4) {
                                snprintf(ui->hover_text, sizeof(ui->hover_text),
                                         "Melody: step %d", (row - 4) * 4 + (col - 4) + 1);
                            } else if (row < 4 && col < 4) {
                                snprintf(ui->hover_text, sizeof(ui->hover_text),
                                         "Live pad %d (ch 2)", row * 4 + col + 1);
                            } else {
                                int local_row = row;
                                int local_col = col - 4;
                                int is_top = (local_row >= 2);
                                if (is_top) {
                                    if (local_row == 3) {
                                        const char *scale = scale_names[local_col & 3];
                                        snprintf(ui->hover_text, sizeof(ui->hover_text),
                                                 "Melody scale: %s", scale);
                                    } else {
                                        const char *bank = bank_names[local_col & 3];
                                        snprintf(ui->hover_text, sizeof(ui->hover_text),
                                                 "Melody bank: %s", bank);
                                    }
                                } else {
                                    int bit = local_row;
                                    uint8_t cc = bottom_ccs[local_col];
                                    snprintf(ui->hover_text, sizeof(ui->hover_text),
                                             "Param CC %u bit %d", cc, bit);
                                }
                            }
                            ui->needs_redraw = 1;
                            break;
                        }
                    }
                    ui->needs_redraw = 1;
                    break;
                }
                case ButtonPress:
                case ButtonRelease: {
                    int is_press = (event.type == ButtonPress);
                    int x = event.xbutton.x;
                    int y = event.xbutton.y;

                    int grid_x = x - MARGIN;
                    int grid_y = y - (MARGIN + TOP_HEIGHT);
                    if (grid_x >= 0 && grid_y >= 0) {
                        int cell = PAD_SIZE + PAD_GAP;
                        int col = grid_x / cell;
                        int row_from_top = grid_y / cell;
                        int in_pad_x = (grid_x % cell) < PAD_SIZE;
                        int in_pad_y = (grid_y % cell) < PAD_SIZE;
                        if (col >= 0 && col < GRID_SIZE && row_from_top >= 0 && row_from_top < GRID_SIZE &&
                            in_pad_x && in_pad_y) {
                            int row = (GRID_SIZE - 1) - row_from_top;
                            uint8_t note = grid_to_note((uint8_t)row, (uint8_t)col);
                            uint8_t msg[3] = { (uint8_t)(is_press ? 0x90 : 0x80), note, (uint8_t)(is_press ? 96 : 0) };

                            if (ui->write_function && ui->atom_event_transfer && ui->midi_event_urid) {
                                uint8_t buf[16];
                                lv2_atom_forge_set_buffer(&ui->forge, buf, sizeof(buf));
                                lv2_atom_forge_atom(&ui->forge, 3, ui->midi_event_urid);
                                lv2_atom_forge_write(&ui->forge, msg, 3);
                                LV2_Atom* atom = (LV2_Atom*)buf;
                                ui->write_function(ui->controller, 0,
                                                  lv2_atom_total_size(atom),
                                                  ui->atom_event_transfer,
                                                  atom);
                            }

                            if (is_press) {
                                ui->state.grid[row][col] = ui->state.grid[row][col] ? 0 : 1;
                                ui->needs_redraw = 1;
                            }
                            break;
                        }
                    }

                    int top_y = y - MARGIN;
                    if (top_y >= 0 && top_y < TOP_HEIGHT) {
                        int cell = PAD_SIZE + PAD_GAP;
                        int col = (x - MARGIN) / cell;
                        int in_pad_x = ((x - MARGIN) % cell) < (TOP_HEIGHT - 4);
                        if (col >= 0 && col < 8 && in_pad_x) {
                            uint8_t cc = TOP_BUTTONS[col];
                            uint8_t msg[3] = {0xB0, cc, (uint8_t)(is_press ? 127 : 0)};
                            if (ui->write_function && ui->atom_event_transfer && ui->midi_event_urid) {
                                uint8_t buf[16];
                                lv2_atom_forge_set_buffer(&ui->forge, buf, sizeof(buf));
                                lv2_atom_forge_atom(&ui->forge, 3, ui->midi_event_urid);
                                lv2_atom_forge_write(&ui->forge, msg, 3);
                                LV2_Atom* atom = (LV2_Atom*)buf;
                                ui->write_function(ui->controller, 0,
                                                  lv2_atom_total_size(atom),
                                                  ui->atom_event_transfer,
                                                  atom);
                            }
                            if (is_press) {
                                ui->state.top_buttons[col] = ui->state.top_buttons[col] ? 0 : 1;
                                ui->needs_redraw = 1;
                            }
                            break;
                        }
                    }

                    int side_x = x - (MARGIN + GRID_SIZE * (PAD_SIZE + PAD_GAP) + PAD_GAP);
                    if (side_x >= 0 && side_x < SIDE_WIDTH) {
                        int cell = PAD_SIZE + PAD_GAP;
                        int row_from_top = (y - (MARGIN + TOP_HEIGHT)) / cell;
                        int in_pad_y = ((y - (MARGIN + TOP_HEIGHT)) % cell) < PAD_SIZE;
                        if (row_from_top >= 0 && row_from_top < GRID_SIZE && in_pad_y) {
                            int row = (GRID_SIZE - 1) - row_from_top;
                            uint8_t cc = SIDE_BUTTONS[row];
                            uint8_t msg[3] = {0xB0, cc, (uint8_t)(is_press ? 127 : 0)};
                            if (ui->write_function && ui->atom_event_transfer && ui->midi_event_urid) {
                                uint8_t buf[16];
                                lv2_atom_forge_set_buffer(&ui->forge, buf, sizeof(buf));
                                lv2_atom_forge_atom(&ui->forge, 3, ui->midi_event_urid);
                                lv2_atom_forge_write(&ui->forge, msg, 3);
                                LV2_Atom* atom = (LV2_Atom*)buf;
                                ui->write_function(ui->controller, 0,
                                                  lv2_atom_total_size(atom),
                                                  ui->atom_event_transfer,
                                                  atom);
                            }
                            if (is_press) {
                                ui->state.selected_voice = row;
                                ui->state.side_buttons[row] = 1;
                                ui->needs_redraw = 1;
                            }
                            break;
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }

        if (ui->needs_redraw) {
            pthread_mutex_lock(&ui->mutex);
            redraw(ui);
            ui->needs_redraw = 0;
            pthread_mutex_unlock(&ui->mutex);
        }

        usleep(16000);
    }

    return NULL;
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
    ui->width = WINDOW_WIDTH;
    ui->height = WINDOW_HEIGHT;
    ui->running = 1;
    pthread_mutex_init(&ui->mutex, NULL);

    ensure_xlib_threads();

    // Initialize X11
    ui->display = XOpenDisplay(NULL);
    if (!ui->display) {
        free(ui);
        return NULL;
    }

    int screen = DefaultScreen(ui->display);

    ui->screen = screen;

    // Allow host to supply parent window
    ui->parent = RootWindow(ui->display, screen);
    for (int i = 0; features && features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_UI__parent)) {
            ui->parent = (Window)(uintptr_t)features[i]->data;
        }
    }

    XSetWindowAttributes attrs;
    attrs.event_mask = ExposureMask | StructureNotifyMask |
                       ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
    attrs.background_pixel = BlackPixel(ui->display, screen);

    ui->window = XCreateWindow(
        ui->display,
        ui->parent,
        0, 0,
        ui->width, ui->height,
        0,
        CopyFromParent,
        InputOutput,
        CopyFromParent,
        CWBackPixel | CWEventMask,
        &attrs
    );

    XStoreName(ui->display, ui->window, "Quadrangle");

    XMapWindow(ui->display, ui->window);

    // Create Cairo surface
    Visual *visual = DefaultVisual(ui->display, screen);
    ui->surface = cairo_xlib_surface_create(
        ui->display,
        ui->window,
        visual,
        ui->width, ui->height
    );

    ui->cr = cairo_create(ui->surface);

    // Create back buffer for double buffering
    ui->back_buffer = cairo_image_surface_create(CAIRO_FORMAT_RGB24, ui->width, ui->height);
    ui->back_cr = cairo_create(ui->back_buffer);

    // Map URIDs for MIDI writeback
    ui->map = NULL;
    for (int i = 0; features && features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_URID__map)) {
            ui->map = (LV2_URID_Map*)features[i]->data;
            break;
        }
    }
    if (ui->map) {
        lv2_atom_forge_init(&ui->forge, ui->map);
        ui->atom_event_transfer = ui->map->map(ui->map->handle, LV2_ATOM__eventTransfer);
        ui->midi_event_urid = ui->map->map(ui->map->handle, LV2_MIDI__MidiEvent);
        ui->atom_chunk_urid = ui->map->map(ui->map->handle, LV2_ATOM__Chunk);
    }

    // Initial draw
    redraw(ui);

    *widget = (LV2UI_Widget)(intptr_t)ui->window;

    // Start event thread to process X11 events and redraw
    pthread_create(&ui->thread, NULL, event_thread_main, ui);

    return (LV2UI_Handle)ui;
}

static void cleanup(LV2UI_Handle handle) {
    QuadrangleUI *ui = (QuadrangleUI*)handle;
    if (!ui) return;

    ui->running = 0;
    if (ui->thread) {
        pthread_join(ui->thread, NULL);
        ui->thread = 0;
    }

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
    QuadrangleUI *ui = (QuadrangleUI*)handle;
    if (!ui) return;

    // play_state is a plain control port (float)
    if (port_index == PORT_PLAY_STATE && buffer && buffer_size >= sizeof(float)) {
        float value = *(const float*)buffer;
        ui->state.playing = (value > 0.5f) ? 1 : 0;
        ui->state.top_buttons[7] = ui->state.playing ? 1 : 0;
        ui->needs_redraw = 1;
        return;
    }
    if (port_index == PORT_CURRENT_STEP && buffer && buffer_size >= sizeof(float)) {
        float value = *(const float*)buffer;
        if (value < 0) value = 0;
        if (value > 15) value = 15;
        ui->state.current_step = (uint8_t)value;
        ui->needs_redraw = 1;
        return;
    }
    if (port_index == 7 && buffer && buffer_size >= sizeof(LV2_Atom_Sequence)) {
        const LV2_Atom_Sequence* seq = (const LV2_Atom_Sequence*)buffer;
        LV2_ATOM_SEQUENCE_FOREACH(seq, ev) {
            if (ev->body.type == ui->atom_chunk_urid && ev->body.size >= sizeof(QuadrangleUiState)) {
                const QuadrangleUiState* state = (const QuadrangleUiState*)(ev + 1);
                if (state->magic != QUADRANGLE_UI_STATE_MAGIC ||
                    state->version != QUADRANGLE_UI_STATE_VERSION) {
                    continue;
                }
                for (uint8_t r = 0; r < 8; ++r) {
                    for (uint8_t c = 0; c < 8; ++c) {
                        ui->state.grid[r][c] = state->grid[r][c] ? 1 : 0;
                    }
                }
                for (uint8_t i = 0; i < 8; ++i) {
                    ui->state.side_buttons[i] = state->side[i] ? 1 : 0;
                }
                for (uint8_t i = 0; i < 9; ++i) {
                    ui->state.top_buttons[i] = state->top[i] ? 1 : 0;
                }
                ui->state.selected_voice = state->selected_voice;
                ui->state.pattern = state->pattern;
                ui->state.playing = state->playing;
                ui->state.current_step = state->current_step;
                ui->needs_redraw = 1;
            }
        }
        return;
    }
}

static const LV2UI_Idle_Interface idle_interface = {
    idle
};

static const void* extension_data(const char *uri) {
    (void)uri;
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
