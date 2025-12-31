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
#include "../include/padseq_ui_state.h"
#include "../include/launchpad_config.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo.h>
#include <cairo-xlib.h>

#define PADSEQ_UI_URI "https://danja.github.io/flues/plugins/padseq#ui"

#define GRID_DIM 8
#define PAD_SIZE 40
#define PAD_GAP 4
#define MARGIN 20
#define SIDE_WIDTH 30
#define TOP_HEIGHT 30
#define INFO_HEIGHT 60
#define PORT_LAUNCHPAD_OUT 2
#define PORT_PLAY_STATE 5  // Matches padseq.ttl lv2:index for play_state
#define PORT_CURRENT_STEP 6
#define PORT_CONTROL_IN 0

#define WINDOW_WIDTH (MARGIN * 2 + GRID_DIM * (PAD_SIZE + PAD_GAP) + SIDE_WIDTH)
#define WINDOW_HEIGHT (MARGIN * 2 + GRID_DIM * (PAD_SIZE + PAD_GAP) + TOP_HEIGHT + INFO_HEIGHT)

// Color definitions
typedef struct {
    double r, g, b;
} Color;

static const Color UI_COLOR_BG = {0.1, 0.1, 0.12};
static const Color UI_COLOR_PAD_OFF = {0.15, 0.15, 0.18};
static const Color UI_COLOR_PAD_RED = {0.9, 0.2, 0.2};
static const Color UI_COLOR_PAD_BLUE = {0.2, 0.4, 0.9};
static const Color UI_COLOR_PAD_GREEN = {0.2, 0.9, 0.3};
static const Color UI_COLOR_PAD_PURPLE = {0.7, 0.3, 0.9};
static const Color UI_COLOR_PLAYHEAD = {0.3, 1.0, 0.4};
static const Color UI_COLOR_TEXT = {0.9, 0.9, 0.9};

// UI State
typedef struct {
    uint8_t grid[GRID_DIM][GRID_DIM];    // Launchpad palette index (0-127)
    uint8_t side_buttons[GRID_DIM];      // Launchpad palette index (0-127)
    uint8_t top_buttons[9];              // Launchpad palette index (0-127)
    uint8_t playing;
    uint8_t pattern;
    uint8_t selected_voice;
    uint16_t bpm;
    uint8_t current_step;
    uint8_t euclid_pulses;
    uint8_t euclid_offset;
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
    uint8_t last_step;
    int playhead_flash;
    int sync_ping_countdown;

    LV2UI_Write_Function write_function;
    LV2UI_Controller controller;
    LV2_URID_Map* map;
    LV2_Atom_Forge forge;
    LV2_URID atom_event_transfer;
    LV2_URID midi_event_urid;
    LV2_URID atom_chunk_urid;

    int has_state_sync;
} PadSeqUI;

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

static Color palette_color(uint8_t index) {
    switch (index) {
        case 0: return (Color){0.05, 0.05, 0.06};  // Off
        case 1: return (Color){0.2, 0.2, 0.22};    // Gray dim
        case 2: return (Color){0.4, 0.4, 0.42};    // Gray med
        case 3: return (Color){0.9, 0.9, 0.92};    // White
        case 4: return (Color){0.4, 0.1, 0.1};     // Red dim
        case 5: return (Color){0.8, 0.2, 0.2};     // Red
        case 6: return (Color){1.0, 0.3, 0.3};     // Red bright
        case 7: return (Color){0.6, 0.3, 0.1};     // Orange dim
        case 9: return (Color){0.9, 0.45, 0.15};   // Orange
        case 84: return (Color){1.0, 0.6, 0.2};    // Orange bright
        case 13: return (Color){0.9, 0.9, 0.2};    // Yellow
        case 14: return (Color){1.0, 1.0, 0.4};    // Yellow bright
        case 20: return (Color){0.1, 0.5, 0.2};    // Green dim
        case 21: return (Color){0.2, 0.8, 0.3};    // Green
        case 22: return (Color){0.3, 1.0, 0.4};    // Green bright
        case 33: return (Color){0.1, 0.4, 0.4};    // Cyan dim
        case 37: return (Color){0.2, 0.7, 0.7};    // Cyan
        case 38: return (Color){0.3, 0.9, 0.9};    // Cyan bright
        case 44: return (Color){0.1, 0.2, 0.5};    // Blue dim
        case 45: return (Color){0.2, 0.4, 0.9};    // Blue
        case 46: return (Color){0.3, 0.6, 1.0};    // Blue bright
        case 48: return (Color){0.3, 0.15, 0.5};   // Purple dim
        case 53: return (Color){0.6, 0.3, 0.9};    // Purple
        case 54: return (Color){0.8, 0.45, 1.0};   // Purple bright
        case 56: return (Color){0.6, 0.2, 0.5};    // Pink dim
        case 57: return (Color){0.9, 0.3, 0.7};    // Pink
        case 58: return (Color){1.0, 0.45, 0.8};   // Pink bright
        default: {
            double t = (double)(index % 32) / 31.0;
            return (Color){0.2 + 0.6 * t, 0.2 + 0.4 * (1.0 - t), 0.3 + 0.5 * (0.5 - fabs(t - 0.5))};
        }
    }
}

static double palette_brightness(uint8_t index) {
    switch (index) {
        case 0: return 0.05;
        case 1: return 0.2;
        case 2: return 0.4;
        case 3: return 0.9;
        case 4: return 0.4;
        case 5: return 0.7;
        case 6: return 0.95;
        case 7: return 0.45;
        case 9: return 0.75;
        case 84: return 0.95;
        case 13: return 0.8;
        case 14: return 1.0;
        case 20: return 0.45;
        case 21: return 0.75;
        case 22: return 0.95;
        case 33: return 0.45;
        case 37: return 0.75;
        case 38: return 0.95;
        case 44: return 0.45;
        case 45: return 0.75;
        case 46: return 0.95;
        case 48: return 0.45;
        case 53: return 0.75;
        case 54: return 0.95;
        case 56: return 0.45;
        case 57: return 0.75;
        case 58: return 0.95;
        default: return 0.7;
    }
}

static int is_playhead_cell(const PadSeqUI *ui, int row, int col) {
    int playhead_row = ui->state.current_step / GRID_DIM;
    int playhead_col = ui->state.current_step % GRID_DIM;
    return ui->state.playing && row == playhead_row && col == playhead_col;
}

static void apply_default_ui_layout(PadSeqUI *ui) {
    for (uint8_t r = 0; r < GRID_DIM; ++r) {
        for (uint8_t c = 0; c < GRID_DIM; ++c) {
            ui->state.grid[r][c] = COLOR_OFF;
        }
    }
    for (uint8_t i = 0; i < GRID_DIM; ++i) {
        ui->state.side_buttons[i] = (i == 0) ? COLOR_SELECTED : COLOR_DRUMS;
    }
    for (uint8_t i = 0; i < 9; ++i) {
        ui->state.top_buttons[i] = COLOR_OFF;
    }
    ui->state.top_buttons[4] = COLOR_SELECTED;
    ui->state.top_buttons[5] = COLOR_YELLOW_DIM;
    ui->state.top_buttons[6] = COLOR_RED_DIM;
    ui->state.top_buttons[7] = COLOR_RED_DIM;
}

static void send_ui_sync_request(PadSeqUI *ui) {
    if (!ui->write_function || !ui->atom_event_transfer || !ui->midi_event_urid) {
        return;
    }

    uint8_t msg[3] = {0xB0, 119, 127};
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

static void draw_grid(PadSeqUI *ui) {
    cairo_t *cr = ui->back_cr;

    // Draw grid
    if (ui->has_state_sync) {
        for (int row = 0; row < GRID_DIM; row++) {
            for (int col = 0; col < GRID_DIM; col++) {
                double x = MARGIN + col * (PAD_SIZE + PAD_GAP);
                double y = MARGIN + TOP_HEIGHT + (GRID_DIM - 1 - row) * (PAD_SIZE + PAD_GAP);

                uint8_t palette = ui->state.grid[row][col];
                if (palette == COLOR_PLAYHEAD && ui->playhead_flash == 0) {
                    palette = COLOR_STEP_OFF;
                }
                Color color = palette_color(palette);
                double brightness = palette_brightness(palette);
                if (ui->playhead_flash > 0 && is_playhead_cell(ui, row, col)) {
                    color = UI_COLOR_PLAYHEAD;
                    brightness = 1.0;
                }
                draw_pad(cr, x, y, PAD_SIZE, color, brightness);
            }
        }

        // Draw side buttons
        for (int i = 0; i < GRID_DIM; i++) {
            double x = MARGIN + GRID_DIM * (PAD_SIZE + PAD_GAP) + PAD_GAP;
            double y = MARGIN + TOP_HEIGHT + (GRID_DIM - 1 - i) * (PAD_SIZE + PAD_GAP);

            uint8_t palette = ui->state.side_buttons[i];
            Color color = palette_color(palette);
            double brightness = palette_brightness(palette);

            draw_pad(cr, x, y, SIDE_WIDTH - 4, color, brightness);
        }

        // Draw top buttons
        for (int i = 0; i < 8; i++) {
            double x = MARGIN + i * (PAD_SIZE + PAD_GAP);
            double y = MARGIN;

            uint8_t palette = ui->state.top_buttons[i];
            Color color = palette_color(palette);
            double brightness = palette_brightness(palette);

            draw_pad(cr, x, y, TOP_HEIGHT - 4, color, brightness);
        }
    } else {
        for (int row = 0; row < GRID_DIM; row++) {
            for (int col = 0; col < GRID_DIM; col++) {
                double x = MARGIN + col * (PAD_SIZE + PAD_GAP);
                double y = MARGIN + TOP_HEIGHT + (GRID_DIM - 1 - row) * (PAD_SIZE + PAD_GAP);

                uint8_t value = ui->state.grid[row][col];
                Color color;
                double brightness;

                if (ui->playhead_flash > 0 && is_playhead_cell(ui, row, col)) {
                    color = UI_COLOR_PLAYHEAD;
                    brightness = 1.0;
                } else if (value > 0) {
                    if (row >= 4) {
                        color = (col < 4) ? UI_COLOR_PAD_RED : UI_COLOR_PAD_BLUE;
                    } else {
                        color = (col < 4) ? UI_COLOR_PAD_GREEN : UI_COLOR_PAD_PURPLE;
                    }
                    brightness = 0.8;
                } else {
                    color = UI_COLOR_PAD_OFF;
                    brightness = 0.3;
                }

                draw_pad(cr, x, y, PAD_SIZE, color, brightness);
            }
        }

        for (int i = 0; i < GRID_DIM; i++) {
            double x = MARGIN + GRID_DIM * (PAD_SIZE + PAD_GAP) + PAD_GAP;
            double y = MARGIN + TOP_HEIGHT + (GRID_DIM - 1 - i) * (PAD_SIZE + PAD_GAP);

            Color color = (i == ui->state.selected_voice) ? (Color){1.0, 0.8, 0.0} : UI_COLOR_PAD_RED;
            double brightness = ui->state.side_buttons[i] ? 0.9 : 0.3;

            draw_pad(cr, x, y, SIDE_WIDTH - 4, color, brightness);
        }

        for (int i = 0; i < 8; i++) {
            double x = MARGIN + i * (PAD_SIZE + PAD_GAP);
            double y = MARGIN;

            Color color = (i >= 2 && i <= 5 && i - 2 == ui->state.pattern) ? (Color){1.0, 0.8, 0.0} : UI_COLOR_PAD_OFF;
            double brightness = ui->state.top_buttons[i] ? 0.9 : 0.3;

            draw_pad(cr, x, y, TOP_HEIGHT - 4, color, brightness);
        }
    }

    // Draw info panel
    double info_y = MARGIN + TOP_HEIGHT + GRID_DIM * (PAD_SIZE + PAD_GAP) + PAD_GAP;
    cairo_set_source_rgb(cr, UI_COLOR_TEXT.r, UI_COLOR_TEXT.g, UI_COLOR_TEXT.b);
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14);

    char info[256];
    snprintf(info, sizeof(info),
        "%s | Pattern %c | Voice %d | BPM %d | Step %d/64 | Euclid %u/%u",
        ui->state.playing ? "▶ PLAYING" : "⏸ STOPPED",
        'A' + ui->state.pattern,
        ui->state.selected_voice + 1,
        ui->state.bpm,
        ui->state.current_step + 1,
        ui->state.euclid_pulses,
        ui->state.euclid_offset
    );

    cairo_move_to(cr, MARGIN, info_y + 20);
    cairo_show_text(cr, info);

    // Draw quadrant labels
    cairo_set_font_size(cr, 10);
    cairo_set_source_rgba(cr, UI_COLOR_TEXT.r, UI_COLOR_TEXT.g, UI_COLOR_TEXT.b, 0.5);

    cairo_move_to(cr, MARGIN + 5, info_y + 40);
    cairo_show_text(cr, "STEPS");

    if (ui->hover_active && ui->hover_text[0] != '\0') {
        cairo_set_font_size(cr, 11);
        cairo_set_source_rgba(cr, UI_COLOR_TEXT.r, UI_COLOR_TEXT.g, UI_COLOR_TEXT.b, 0.8);
        cairo_move_to(cr, MARGIN, info_y + 72);
        cairo_show_text(cr, ui->hover_text);
    }

}

static void redraw(PadSeqUI *ui) {
    if (!ui->back_cr) return;

    // Clear back buffer
    cairo_set_source_rgb(ui->back_cr, UI_COLOR_BG.r, UI_COLOR_BG.g, UI_COLOR_BG.b);
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
    PadSeqUI *ui = (PadSeqUI*)handle;
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
    PadSeqUI *ui = (PadSeqUI*)arg;

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
                        if (col >= 0 && col < GRID_DIM && row_from_top >= 0 && row_from_top < GRID_DIM &&
                            in_pad_x && in_pad_y) {
                            int row = (GRID_DIM - 1) - row_from_top;
                            ui->hover_active = 1;
                            ui->hover_row = row;
                            ui->hover_col = col;

                            uint8_t step = (uint8_t)(row * GRID_DIM + col);
                            snprintf(ui->hover_text, sizeof(ui->hover_text),
                                     "Step %u (voice %u)", step + 1, ui->state.selected_voice + 1);
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
                        if (col >= 0 && col < GRID_DIM && row_from_top >= 0 && row_from_top < GRID_DIM &&
                            in_pad_x && in_pad_y) {
                            int row = (GRID_DIM - 1) - row_from_top;
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

                            if (!ui->has_state_sync && is_press) {
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
                            if (!ui->has_state_sync && is_press) {
                                ui->state.top_buttons[col] = ui->state.top_buttons[col] ? 0 : 1;
                                ui->needs_redraw = 1;
                            }
                            break;
                        }
                    }

                    int side_x = x - (MARGIN + GRID_DIM * (PAD_SIZE + PAD_GAP) + PAD_GAP);
                    if (side_x >= 0 && side_x < SIDE_WIDTH) {
                        int cell = PAD_SIZE + PAD_GAP;
                        int row_from_top = (y - (MARGIN + TOP_HEIGHT)) / cell;
                        int in_pad_y = ((y - (MARGIN + TOP_HEIGHT)) % cell) < PAD_SIZE;
                        if (row_from_top >= 0 && row_from_top < GRID_DIM && in_pad_y) {
                            int row = (GRID_DIM - 1) - row_from_top;
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
                            if (!ui->has_state_sync && is_press) {
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

        if (ui->playhead_flash > 0) {
            ui->playhead_flash--;
            ui->needs_redraw = 1;
        }

        if (!ui->has_state_sync) {
            if (ui->sync_ping_countdown > 0) {
                ui->sync_ping_countdown--;
            } else {
                send_ui_sync_request(ui);
                ui->sync_ping_countdown = 60;
            }
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

    PadSeqUI *ui = (PadSeqUI*)calloc(1, sizeof(PadSeqUI));
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
    ui->has_state_sync = 0;
    ui->last_step = 0;
    ui->playhead_flash = 0;
    ui->sync_ping_countdown = 20;
    apply_default_ui_layout(ui);
    ui->state.euclid_pulses = 0;
    ui->state.euclid_offset = 0;

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

    XStoreName(ui->display, ui->window, "PadSeq");

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

    // Request initial state sync from the plugin.
    send_ui_sync_request(ui);

    // Start event thread to process X11 events and redraw
    pthread_create(&ui->thread, NULL, event_thread_main, ui);

    return (LV2UI_Handle)ui;
}

static void cleanup(LV2UI_Handle handle) {
    PadSeqUI *ui = (PadSeqUI*)handle;
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
    PadSeqUI *ui = (PadSeqUI*)handle;
    if (!ui) return;

    // play_state is a plain control port (float)
    if (port_index == PORT_PLAY_STATE && buffer && buffer_size >= sizeof(float)) {
        float value = *(const float*)buffer;
        ui->state.playing = (value > 0.5f) ? 1 : 0;
        if (!ui->has_state_sync) {
            ui->state.top_buttons[8] = ui->state.playing ? 1 : 0;
        }
        ui->needs_redraw = 1;
        return;
    }
    if (port_index == PORT_LAUNCHPAD_OUT && buffer && buffer_size >= sizeof(LV2_Atom_Sequence)) {
        return;
    }
    if (port_index == PORT_CONTROL_IN && buffer && buffer_size >= sizeof(LV2_Atom_Sequence)) {
        if (ui->has_state_sync) {
            return;
        }
        const LV2_Atom_Sequence* seq = (const LV2_Atom_Sequence*)buffer;
        bool updated = false;
        LV2_ATOM_SEQUENCE_FOREACH(seq, ev) {
            if (ev->body.type != ui->midi_event_urid || ev->body.size < 3) {
                continue;
            }
            const uint8_t *midi = (const uint8_t*)(ev + 1);
            uint8_t status = midi[0] & 0xF0;
            if (status == 0x90 && midi[2] > 0) {
                uint8_t row = 0;
                uint8_t col = 0;
                if (note_to_grid(midi[1], &row, &col)) {
                    ui->state.grid[row][col] = ui->state.grid[row][col] ? 0 : 1;
                    updated = true;
                }
            } else if (status == 0xB0) {
                uint8_t index = 0;
                if (is_side_button(midi[1], &index)) {
                    ui->state.selected_voice = index;
                    ui->state.side_buttons[index] = ui->state.side_buttons[index] ? 0 : 1;
                    updated = true;
                } else if (is_top_button(midi[1], &index)) {
                    ui->state.top_buttons[index] = ui->state.top_buttons[index] ? 0 : 1;
                    updated = true;
                }
            }
        }
        if (updated) {
            ui->needs_redraw = 1;
        }
        return;
    }
    if (port_index == PORT_CURRENT_STEP && buffer && buffer_size >= sizeof(float)) {
        float value = *(const float*)buffer;
        if (value < 0) value = 0;
        if (value > 63) value = 63;
        ui->state.current_step = (uint8_t)value;
        if (ui->state.current_step != ui->last_step) {
            ui->playhead_flash = 6;
            ui->last_step = ui->state.current_step;
        }
        ui->needs_redraw = 1;
        return;
    }
    if (port_index == 7 && buffer && buffer_size >= sizeof(LV2_Atom_Sequence)) {
        const LV2_Atom_Sequence* seq = (const LV2_Atom_Sequence*)buffer;
        LV2_ATOM_SEQUENCE_FOREACH(seq, ev) {
            if (ev->body.type == ui->atom_chunk_urid && ev->body.size >= sizeof(PadSeqUiState)) {
                const PadSeqUiState* state = (const PadSeqUiState*)(ev + 1);
                if (state->magic == PADSEQ_UI_STATE_MAGIC &&
                    (state->version == PADSEQ_UI_STATE_VERSION || state->version == 1)) {
                    ui->has_state_sync = 1;
                    for (uint8_t r = 0; r < 8; ++r) {
                        for (uint8_t c = 0; c < 8; ++c) {
                            if (state->grid[r][c] != COLOR_PLAYHEAD) {
                                ui->state.grid[r][c] = state->grid[r][c];
                            }
                        }
                    }
                    for (uint8_t i = 0; i < 8; ++i) {
                        ui->state.side_buttons[i] = state->side[i];
                    }
                    for (uint8_t i = 0; i < 9; ++i) {
                        ui->state.top_buttons[i] = state->top[i];
                    }
                    ui->state.selected_voice = state->selected_voice;
                    ui->state.pattern = state->pattern;
                    ui->state.playing = state->playing;
                    ui->state.current_step = state->current_step;
                    if (state->version >= 2) {
                        ui->state.euclid_pulses = state->euclid_pulses;
                        ui->state.euclid_offset = state->euclid_offset;
                    }
                    ui->needs_redraw = 1;
                }
            } else if (ev->body.type == ui->atom_chunk_urid && ev->body.size >= sizeof(PadSeqUiDelta)) {
                const PadSeqUiDelta* delta = (const PadSeqUiDelta*)(ev + 1);
                if (delta->magic != PADSEQ_UI_DELTA_MAGIC ||
                    delta->version != PADSEQ_UI_DELTA_VERSION) {
                    continue;
                }
                if (delta->color == COLOR_PLAYHEAD) {
                    continue;
                }
                ui->has_state_sync = 1;
                if (delta->target == PADSEQ_UI_DELTA_GRID) {
                    if (delta->index_a < 8 && delta->index_b < 8) {
                        ui->state.grid[delta->index_a][delta->index_b] = delta->color;
                    }
                } else if (delta->target == PADSEQ_UI_DELTA_SIDE) {
                    if (delta->index_a < 8) {
                        ui->state.side_buttons[delta->index_a] = delta->color;
                    }
                } else if (delta->target == PADSEQ_UI_DELTA_TOP) {
                    if (delta->index_a < 9) {
                        ui->state.top_buttons[delta->index_a] = delta->color;
                    }
                }
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
    PADSEQ_UI_URI,
    instantiate,
    cleanup,
    port_event,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : NULL;
}
