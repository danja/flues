#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>
#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>
#include <lv2/ui/ui.h>
#include "../include/arpiso_ui_state.h"
#include "../include/launchpad_config.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo.h>
#include <cairo-xlib.h>

#define ARPISO_UI_URI "https://danja.github.io/flues/plugins/arpiso#ui"

#define GRID_DIM 8
#define PAD_SIZE 40
#define PAD_GAP 4
#define MARGIN 20
#define SIDE_WIDTH 30
#define TOP_HEIGHT 30
#define INFO_HEIGHT 170
#define PORT_LAUNCHPAD_OUT 2
#define PORT_PLAY_STATE 5  // Matches arpiso.ttl lv2:index for play_state
#define PORT_CURRENT_STEP 6
#define PORT_CONTROL_IN 0
#define UI_CC_HOLD_MODE 116
#define UI_CC_GM_DRUM_MODE 117

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
static const uint8_t k_gm_drum_notes[] = {36, 38, 42, 46, 49, 51, 45, 41, 39, 37, 43, 50};

// UI State
typedef struct {
    uint8_t grid[GRID_DIM][GRID_DIM];    // Launchpad palette index (0-127)
    uint8_t side_buttons[GRID_DIM];      // Launchpad palette index (0-127)
    uint8_t top_buttons[9];              // Launchpad palette index (0-127)
    uint8_t playing;
    uint8_t pattern;
    uint8_t held_count;
    uint16_t bpm;
    uint8_t current_step;
    uint8_t euclid_pulses;
    uint8_t euclid_offset;
    uint8_t root_note;
    uint8_t scale_index;
    uint8_t gate_percent;
    uint8_t hold_latch_mode;
    uint8_t gm_drum_mode;
    uint8_t motion_mode;
    uint8_t clock_division_index;
    uint8_t cycle_length_index;
    uint8_t density_bias;
    uint8_t phase_bias;
    uint8_t gravity_strength;
    uint8_t travel_scale;
    uint8_t velocity_curve;
    uint8_t humanize;
} UIState;

typedef struct {
    Display *display;
    Window window;
    cairo_surface_t *surface;
    cairo_t *cr;

    // Double buffering
    cairo_surface_t *back_buffer;
    cairo_t *back_cr;

    volatile int needs_redraw;
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
    char status_text[128];
    uint8_t last_step;
    int playhead_flash;
    int status_frames;
    int sync_ping_countdown;

    LV2UI_Write_Function write_function;
    LV2UI_Controller controller;
    LV2_URID_Map* map;
    LV2_Atom_Forge forge;
    LV2_URID atom_event_transfer;
    LV2_URID midi_event_urid;
    LV2_URID atom_chunk_urid;

    volatile int has_state_sync;
    volatile int sync_source; // 0=unknown, 1=notify_out, 2=launchpad_out
} ArpIsoUI;

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

static void set_status(ArpIsoUI *ui, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(ui->status_text, sizeof(ui->status_text), fmt, args);
    va_end(args);
    ui->status_frames = 120;
    ui->needs_redraw = 1;
}

static void request_redraw(ArpIsoUI *ui) {
    if (!ui) {
        return;
    }
    ui->needs_redraw = 1;
}

static const char *scale_name(uint8_t index) {
    static const char *names[8] = {
        "Major", "Natural Minor", "Dorian", "Major Pent", "Mixolydian",
        "Phrygian", "Harm Minor", "Blues"
    };
    return names[index & 7];
}

static const char *motion_mode_name(uint8_t index) {
    static const char *names[3] = {"Near", "Far", "Alt"};
    return names[index % 3];
}

static const char *clock_div_name(uint8_t index) {
    static const char *names[4] = {"1x", "2x", "4x", "8x"};
    return names[index & 3];
}

static uint8_t cycle_length_value(uint8_t index) {
    static const uint8_t vals[4] = {8, 12, 16, 24};
    return vals[index & 3];
}

static uint8_t clamp_u8(int v, int lo, int hi) {
    if (v < lo) return (uint8_t)lo;
    if (v > hi) return (uint8_t)hi;
    return (uint8_t)v;
}

static uint8_t quantize_scale(uint8_t semitone, uint8_t scale_index) {
    static const uint8_t major[] = {0, 2, 4, 5, 7, 9, 11};
    static const uint8_t minor[] = {0, 2, 3, 5, 7, 8, 10};
    static const uint8_t dorian[] = {0, 2, 3, 5, 7, 9, 10};
    static const uint8_t pent[] = {0, 2, 4, 7, 9};
    static const uint8_t mix[] = {0, 2, 4, 5, 7, 9, 10};
    static const uint8_t phryg[] = {0, 1, 3, 5, 7, 8, 10};
    static const uint8_t harmm[] = {0, 2, 3, 5, 7, 8, 11};
    static const uint8_t blues[] = {0, 3, 5, 6, 7, 10};

    const uint8_t *scale = major;
    uint8_t len = (uint8_t)(sizeof(major) / sizeof(major[0]));
    switch (scale_index & 7) {
        case 1: scale = minor; len = (uint8_t)(sizeof(minor) / sizeof(minor[0])); break;
        case 2: scale = dorian; len = (uint8_t)(sizeof(dorian) / sizeof(dorian[0])); break;
        case 3: scale = pent; len = (uint8_t)(sizeof(pent) / sizeof(pent[0])); break;
        case 4: scale = mix; len = (uint8_t)(sizeof(mix) / sizeof(mix[0])); break;
        case 5: scale = phryg; len = (uint8_t)(sizeof(phryg) / sizeof(phryg[0])); break;
        case 6: scale = harmm; len = (uint8_t)(sizeof(harmm) / sizeof(harmm[0])); break;
        case 7: scale = blues; len = (uint8_t)(sizeof(blues) / sizeof(blues[0])); break;
        default: break;
    }

    uint8_t oct = semitone / 12;
    uint8_t deg = semitone % 12;
    uint8_t nearest = scale[0];
    uint8_t best = 12;
    for (uint8_t i = 0; i < len; ++i) {
        uint8_t d = (uint8_t)abs((int)deg - (int)scale[i]);
        if (d < best) {
            best = d;
            nearest = scale[i];
        }
    }
    return (uint8_t)(oct * 12 + nearest);
}

static uint8_t melodic_note_from_grid(const UIState *state, uint8_t row, uint8_t col) {
    int semitone = (int)state->root_note + (int)col + (int)row * 5;
    semitone = clamp_u8(semitone, 24, 108);
    return quantize_scale((uint8_t)semitone, state->scale_index);
}

static const char *gm_drum_name(uint8_t note) {
    switch (note) {
        case 36: return "Kick";
        case 38: return "Snr";
        case 42: return "CHH";
        case 46: return "OHH";
        case 49: return "Crs";
        case 51: return "Ride";
        case 45: return "TomL";
        case 41: return "TomF";
        case 39: return "Clap";
        case 37: return "Rim";
        case 43: return "TomH";
        case 50: return "TomHi";
        default: return "Drm";
    }
}

static void pad_label_for(const UIState *state, uint8_t row, uint8_t col, char *out, size_t out_sz) {
    uint8_t note = melodic_note_from_grid(state, row, col);
    if (state->gm_drum_mode) {
        uint8_t idx = (uint8_t)(note % (sizeof(k_gm_drum_notes) / sizeof(k_gm_drum_notes[0])));
        uint8_t gm_note = k_gm_drum_notes[idx];
        snprintf(out, out_sz, "%s", gm_drum_name(gm_note));
        return;
    }

    static const char *names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    uint8_t pc = (uint8_t)(note % 12);
    int oct = (int)(note / 12) - 1;
    snprintf(out, out_sz, "%s%d", names[pc], oct);
}

static void send_cc(ArpIsoUI *ui, uint8_t cc, uint8_t value) {
    if (!ui->write_function || !ui->atom_event_transfer || !ui->midi_event_urid) {
        return;
    }
    uint8_t msg[3] = {0xB0, cc, value};
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

static uint8_t ui_active_columns(const ArpIsoUI *ui) {
    uint8_t count = 0;
    for (uint8_t c = 0; c < GRID_DIM; ++c) {
        if (ui->state.grid[0][c] != COLOR_GRAY_DIM) {
            count++;
        }
    }
    return count;
}

static const char *sync_source_label(const ArpIsoUI *ui) {
    switch (ui->sync_source) {
        case 1: return "notify_out";
        case 2: return "launchpad_out";
        default: return "pending";
    }
}

static int apply_launchpad_led_triplet(ArpIsoUI *ui, uint8_t key, uint8_t color) {
    uint8_t row = 0;
    uint8_t col = 0;
    uint8_t idx = 0;

    if (note_to_grid(key, &row, &col)) {
        ui->state.grid[row][col] = color;
        return 1;
    }
    if (is_side_button(key, &idx)) {
        ui->state.side_buttons[idx] = color;
        return 1;
    }
    if (is_top_button(key, &idx)) {
        ui->state.top_buttons[idx] = color;
        return 1;
    }
    return 0;
}

static int apply_launchpad_midi_to_ui_state(ArpIsoUI *ui, const uint8_t *midi, uint32_t size) {
    if (!ui || !midi || size < 1) {
        return 0;
    }

    // Launchpad bulk LED update SysEx:
    // F0 00 20 29 02 0D 03 [type key color]... F7
    if (midi[0] == 0xF0 && size >= 9) {
        if (size > 7 &&
            midi[1] == 0x00 &&
            midi[2] == 0x20 &&
            midi[3] == 0x29 &&
            midi[4] == 0x02 &&
            midi[5] == 0x0D &&
            midi[6] == SYSEX_CMD_LED_LIGHTING) {
            int updated = 0;
            uint32_t i = 7;
            while (i + 3 <= size) {
                if (midi[i] == 0xF7) {
                    break;
                }
                if (i + 2 >= size) {
                    break;
                }
                const uint8_t led_type = midi[i];
                const uint8_t key = midi[i + 1];
                const uint8_t color = midi[i + 2];
                (void)led_type; // currently ignored, we only mirror color
                updated |= apply_launchpad_led_triplet(ui, key, color);
                i += 3;
            }
            return updated;
        }
        return 0;
    }

    // Single LED updates (note/cc channel 1-3, value=color)
    if (size >= 3) {
        const uint8_t status = (uint8_t)(midi[0] & 0xF0);
        if (status == 0x90 || status == 0xB0) {
            const uint8_t key = midi[1];
            const uint8_t color = midi[2];
            return apply_launchpad_led_triplet(ui, key, color);
        }
    }

    return 0;
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
        case 1: return (Color){0.3, 0.3, 0.34};    // Gray dim
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
        case 1: return 0.35;
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

static void apply_default_ui_layout(ArpIsoUI *ui) {
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
    ui->state.top_buttons[8] = COLOR_RED_DIM;
}

static void send_ui_sync_request(ArpIsoUI *ui) {
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

static void draw_grid(ArpIsoUI *ui) {
    cairo_t *cr = ui->back_cr;

    // Draw grid
    if (ui->has_state_sync) {
        for (int row = 0; row < GRID_DIM; row++) {
            for (int col = 0; col < GRID_DIM; col++) {
                double x = MARGIN + col * (PAD_SIZE + PAD_GAP);
                double y = MARGIN + TOP_HEIGHT + (GRID_DIM - 1 - row) * (PAD_SIZE + PAD_GAP);

                uint8_t palette = ui->state.grid[row][col];
                Color color = palette_color(palette);
                double brightness = palette_brightness(palette);
                draw_pad(cr, x, y, PAD_SIZE, color, brightness);

                char label[16];
                pad_label_for(&ui->state, (uint8_t)row, (uint8_t)col, label, sizeof(label));
                cairo_set_source_rgba(cr, 0.98, 0.98, 0.98, palette == COLOR_OFF ? 0.42 : 0.9);
                cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                cairo_set_font_size(cr, 8.5);
                cairo_move_to(cr, x + 3, y + PAD_SIZE - 4);
                cairo_show_text(cr, label);
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

                if (value > 0) {
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

                char label[16];
                pad_label_for(&ui->state, (uint8_t)row, (uint8_t)col, label, sizeof(label));
                cairo_set_source_rgba(cr, 0.98, 0.98, 0.98, 0.75);
                cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                cairo_set_font_size(cr, 8.5);
                cairo_move_to(cr, x + 3, y + PAD_SIZE - 4);
                cairo_show_text(cr, label);
            }
        }

        for (int i = 0; i < GRID_DIM; i++) {
            double x = MARGIN + GRID_DIM * (PAD_SIZE + PAD_GAP) + PAD_GAP;
            double y = MARGIN + TOP_HEIGHT + (GRID_DIM - 1 - i) * (PAD_SIZE + PAD_GAP);

            Color color = (i < ui->state.held_count) ? (Color){1.0, 0.8, 0.0} : UI_COLOR_PAD_RED;
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
        "%s | Wells %u/5 | BPM %u | Root %u | %s | Gate %u%% | Sync %s",
        ui->state.playing ? "▶ PLAYING" : "⏸ STOPPED",
        (unsigned)ui->state.held_count,
        ui->state.bpm,
        (unsigned)ui->state.root_note,
        scale_name(ui->state.scale_index),
        (unsigned)ui->state.gate_percent,
        sync_source_label(ui)
    );

    cairo_move_to(cr, MARGIN, info_y + 20);
    cairo_show_text(cr, info);

    // Draw control summary
    cairo_set_font_size(cr, 10);
    cairo_set_source_rgba(cr, UI_COLOR_TEXT.r, UI_COLOR_TEXT.g, UI_COLOR_TEXT.b, 0.5);

    cairo_move_to(cr, MARGIN + 5, info_y + 40);
    cairo_show_text(cr, "Top: Start/Clock/Cycle/Root/Scale/Motion/Clear  |  Right: Density..Pattern");

    cairo_set_font_size(cr, 11);
    cairo_set_source_rgba(cr, UI_COLOR_TEXT.r, UI_COLOR_TEXT.g, UI_COLOR_TEXT.b, 0.9);
    char scale_line[96];
    snprintf(scale_line, sizeof(scale_line), "Selected Scale: %s", scale_name(ui->state.scale_index));
    cairo_move_to(cr, MARGIN + 5, info_y + 60);
    cairo_show_text(cr, scale_line);

    char control_line[256];
    snprintf(control_line, sizeof(control_line),
             "Motion: %s   Clock: %s   Cycle: %u   D/P/G/T: %u/%u/%u/%u   Vel/Hum: %u/%u",
             motion_mode_name(ui->state.motion_mode),
             clock_div_name(ui->state.clock_division_index),
             (unsigned)cycle_length_value(ui->state.cycle_length_index),
             (unsigned)ui->state.density_bias,
             (unsigned)ui->state.phase_bias,
             (unsigned)ui->state.gravity_strength,
             (unsigned)ui->state.travel_scale,
             (unsigned)ui->state.velocity_curve,
             (unsigned)ui->state.humanize);
    cairo_move_to(cr, MARGIN + 5, info_y + 76);
    cairo_show_text(cr, control_line);

    // Hold checkbox
    const double hold_x = MARGIN + 5;
    const double hold_y = info_y + 94;
    const double cb_s = 12;
    cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 0.8);
    cairo_rectangle(cr, hold_x, hold_y, cb_s, cb_s);
    cairo_stroke(cr);
    if (ui->state.hold_latch_mode) {
        cairo_set_source_rgb(cr, 0.5, 0.95, 0.65);
        cairo_rectangle(cr, hold_x + 2, hold_y + 2, cb_s - 4, cb_s - 4);
        cairo_fill(cr);
    }
    cairo_set_source_rgba(cr, UI_COLOR_TEXT.r, UI_COLOR_TEXT.g, UI_COLOR_TEXT.b, 0.9);
    cairo_move_to(cr, hold_x + cb_s + 8, hold_y + 10);
    cairo_show_text(cr, "Hold (latch pads until re-pressed)");

    // GM checkbox
    const double cb_x = MARGIN + 5;
    const double cb_y = info_y + 114;
    cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 0.8);
    cairo_rectangle(cr, cb_x, cb_y, cb_s, cb_s);
    cairo_stroke(cr);
    if (ui->state.gm_drum_mode) {
        cairo_set_source_rgb(cr, 0.95, 0.75, 0.2);
        cairo_rectangle(cr, cb_x + 2, cb_y + 2, cb_s - 4, cb_s - 4);
        cairo_fill(cr);
    }
    cairo_set_source_rgba(cr, UI_COLOR_TEXT.r, UI_COLOR_TEXT.g, UI_COLOR_TEXT.b, 0.9);
    cairo_move_to(cr, cb_x + cb_s + 8, cb_y + 10);
    cairo_show_text(cr, "GM Drum Output (Channel 10 + GM map)");

    if (ui->hover_active && ui->hover_text[0] != '\0') {
        cairo_set_font_size(cr, 11);
        cairo_set_source_rgba(cr, UI_COLOR_TEXT.r, UI_COLOR_TEXT.g, UI_COLOR_TEXT.b, 0.8);
        cairo_move_to(cr, MARGIN, info_y + 154);
        cairo_show_text(cr, ui->hover_text);
    }

    if (ui->status_frames > 0 && ui->status_text[0] != '\0') {
        cairo_set_font_size(cr, 11);
        cairo_set_source_rgba(cr, UI_COLOR_TEXT.r, UI_COLOR_TEXT.g, UI_COLOR_TEXT.b, 0.8);
        cairo_move_to(cr, MARGIN, info_y + 138);
        cairo_show_text(cr, ui->status_text);
    }

}

static void redraw(ArpIsoUI *ui) {
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
    ArpIsoUI *ui = (ArpIsoUI*)handle;
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
    ArpIsoUI *ui = (ArpIsoUI*)arg;

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
                                     "Pad %u (row %d, col %d)", step + 1, row + 1, col + 1);
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
                    double info_y = MARGIN + TOP_HEIGHT + GRID_DIM * (PAD_SIZE + PAD_GAP) + PAD_GAP;
                    if (is_press && y >= (int)(info_y + 92) && y <= (int)(info_y + 110) &&
                        x >= MARGIN + 5 && x <= MARGIN + 280) {
                        ui->state.hold_latch_mode = (uint8_t)(!ui->state.hold_latch_mode);
                        send_cc(ui, UI_CC_HOLD_MODE, ui->state.hold_latch_mode ? 127 : 0);
                        set_status(ui, "Hold %s", ui->state.hold_latch_mode ? "ON" : "OFF");
                        ui->needs_redraw = 1;
                        break;
                    }
                    if (is_press && y >= (int)(info_y + 112) && y <= (int)(info_y + 130) &&
                        x >= MARGIN + 5 && x <= MARGIN + 280) {
                        ui->state.gm_drum_mode = (uint8_t)(!ui->state.gm_drum_mode);
                        send_cc(ui, UI_CC_GM_DRUM_MODE, ui->state.gm_drum_mode ? 127 : 0);
                        set_status(ui, "GM Drum Output %s", ui->state.gm_drum_mode ? "ON" : "OFF");
                        ui->needs_redraw = 1;
                        break;
                    }

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
                            send_cc(ui, cc, (uint8_t)(is_press ? 127 : 0));
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
                            send_cc(ui, cc, (uint8_t)(is_press ? 127 : 0));
                            if (!ui->has_state_sync && is_press) {
                                ui->state.held_count = (uint8_t)(row + 1);
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

        if (ui->status_frames > 0) {
            ui->status_frames--;
            if (ui->status_frames == 0) {
                ui->status_text[0] = '\0';
                ui->needs_redraw = 1;
            }
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

    ArpIsoUI *ui = (ArpIsoUI*)calloc(1, sizeof(ArpIsoUI));
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
    ui->sync_source = 0;
    ui->last_step = 0;
    ui->playhead_flash = 0;
    ui->sync_ping_countdown = 20;
    apply_default_ui_layout(ui);
    ui->state.euclid_pulses = 0;
    ui->state.euclid_offset = 0;
    ui->state.scale_index = 0;
    ui->state.root_note = 48;
    ui->state.gate_percent = 50;
    ui->state.hold_latch_mode = 0;
    ui->state.held_count = 0;
    ui->state.gm_drum_mode = 0;
    ui->state.motion_mode = 0;
    ui->state.clock_division_index = 0;
    ui->state.cycle_length_index = 2;
    ui->state.density_bias = 32;
    ui->state.phase_bias = 0;
    ui->state.gravity_strength = 64;
    ui->state.travel_scale = 64;
    ui->state.velocity_curve = 32;
    ui->state.humanize = 0;

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

    XStoreName(ui->display, ui->window, "ArpIso");

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
    ArpIsoUI *ui = (ArpIsoUI*)handle;
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
    ArpIsoUI *ui = (ArpIsoUI*)handle;
    if (!ui) return;

    // play_state is a plain control port (float)
    if (port_index == PORT_PLAY_STATE && buffer && buffer_size >= sizeof(float)) {
        float value = *(const float*)buffer;
        ui->state.playing = (value > 0.5f) ? 1 : 0;
        if (!ui->has_state_sync) {
            ui->state.top_buttons[8] = ui->state.playing ? 1 : 0;
        }
        request_redraw(ui);
        return;
    }
    if (port_index == PORT_LAUNCHPAD_OUT && buffer && buffer_size >= sizeof(LV2_Atom_Sequence)) {
        const LV2_Atom_Sequence* seq = (const LV2_Atom_Sequence*)buffer;
        int updated = 0;
        LV2_ATOM_SEQUENCE_FOREACH(seq, ev) {
            if (ev->body.type != ui->midi_event_urid) {
                continue;
            }
            const uint8_t *midi = (const uint8_t *)(ev + 1);
            updated |= apply_launchpad_midi_to_ui_state(ui, midi, ev->body.size);
        }
        if (updated) {
            ui->has_state_sync = 1;
            ui->sync_source = 2;
            request_redraw(ui);
        }
        return;
    }
    if (port_index == PORT_CONTROL_IN && buffer && buffer_size >= sizeof(LV2_Atom_Sequence)) {
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
                if (midi[1] == UI_CC_HOLD_MODE) {
                    ui->state.hold_latch_mode = (uint8_t)(midi[2] >= 64 ? 1 : 0);
                    updated = true;
                    continue;
                }
                uint8_t index = 0;
                if (is_side_button(midi[1], &index)) {
                    ui->state.held_count = (uint8_t)(index + 1);
                    if (!ui->has_state_sync) {
                        ui->state.side_buttons[index] = ui->state.side_buttons[index] ? 0 : 1;
                    }
                    if (midi[2] > 0) {
                        switch (index) {
                            case 0: ui->state.density_bias = (uint8_t)((ui->state.density_bias + 16) & 0x7F); break;
                            case 1: ui->state.phase_bias = (uint8_t)((ui->state.phase_bias + 16) & 0x7F); break;
                            case 2: ui->state.gravity_strength = (uint8_t)((ui->state.gravity_strength + 16) & 0x7F); break;
                            case 3: ui->state.travel_scale = (uint8_t)((ui->state.travel_scale + 16) & 0x7F); break;
                            case 5: ui->state.velocity_curve = (uint8_t)((ui->state.velocity_curve + 24) & 0x7F); break;
                            case 6: ui->state.humanize = (uint8_t)((ui->state.humanize + 16) & 0x7F); break;
                            default: break;
                        }
                    }
                    updated = true;
                } else if (is_top_button(midi[1], &index)) {
                    if (!ui->has_state_sync) {
                        ui->state.top_buttons[index] = ui->state.top_buttons[index] ? 0 : 1;
                    }
                    // Keep useful labels responsive even if notify sync is delayed/missing.
                    if (midi[2] > 0) {
                        switch (index) {
                            case 1:
                                ui->state.clock_division_index = (uint8_t)((ui->state.clock_division_index + 1) & 3);
                                break;
                            case 2:
                                ui->state.cycle_length_index = (uint8_t)((ui->state.cycle_length_index + 1) & 3);
                                break;
                            case 3:
                                ui->state.root_note = (uint8_t)(ui->state.root_note + 2);
                                if (ui->state.root_note > 72) ui->state.root_note = 36;
                                break;
                            case 4:
                                ui->state.scale_index = (uint8_t)((ui->state.scale_index + 1) & 7);
                                break;
                            case 5:
                                ui->state.motion_mode = (uint8_t)((ui->state.motion_mode + 1) % 3);
                                break;
                            case 8:
                                ui->state.gm_drum_mode = (uint8_t)(!ui->state.gm_drum_mode);
                                break;
                            default:
                                break;
                        }
                    }
                    updated = true;
                }
            }
        }
        if (updated) {
            request_redraw(ui);
        }
        return;
    }
    if (port_index == PORT_CURRENT_STEP && buffer && buffer_size >= sizeof(float)) {
        float value = *(const float*)buffer;
        if (value < 0) value = 0;
        if (value > 63) value = 63;
        ui->state.current_step = (uint8_t)value;
        ui->last_step = ui->state.current_step;
        request_redraw(ui);
        return;
    }
    if (port_index == 7 && buffer && buffer_size >= sizeof(LV2_Atom_Sequence)) {
        const LV2_Atom_Sequence* seq = (const LV2_Atom_Sequence*)buffer;
        LV2_ATOM_SEQUENCE_FOREACH(seq, ev) {
            if (ev->body.type == ui->atom_chunk_urid && ev->body.size >= sizeof(ArpIsoUiState)) {
                const ArpIsoUiState* state = (const ArpIsoUiState*)(ev + 1);
                if (state->magic == ARPISO_UI_STATE_MAGIC &&
                    (state->version == ARPISO_UI_STATE_VERSION || state->version == 1)) {
                    ui->has_state_sync = 1;
                    ui->sync_source = 1;
                    for (uint8_t r = 0; r < 8; ++r) {
                        for (uint8_t c = 0; c < 8; ++c) {
                            ui->state.grid[r][c] = state->grid[r][c];
                        }
                    }
                    for (uint8_t i = 0; i < 8; ++i) {
                        ui->state.side_buttons[i] = state->side[i];
                    }
                    for (uint8_t i = 0; i < 9; ++i) {
                        ui->state.top_buttons[i] = state->top[i];
                    }
                    ui->state.held_count = state->selected_voice;
                    ui->state.pattern = state->pattern;
                    ui->state.playing = state->playing;
                    ui->state.bpm = state->bpm;
                    ui->state.current_step = state->current_step;
                    if (state->version >= 2) {
                        ui->state.euclid_pulses = state->euclid_pulses;
                        ui->state.euclid_offset = state->euclid_offset;
                    }
                    if (state->version >= 3) {
                        ui->state.held_count = state->held_count;
                        ui->state.root_note = state->root_note;
                        ui->state.scale_index = state->scale_index;
                        ui->state.gate_percent = state->gate_percent;
                        ui->state.gm_drum_mode = state->gm_drum_mode;
                    }
                    if (state->version >= 4) {
                        ui->state.motion_mode = state->motion_mode;
                        ui->state.clock_division_index = state->clock_division_index;
                        ui->state.cycle_length_index = state->cycle_length_index;
                        ui->state.density_bias = state->density_bias;
                        ui->state.phase_bias = state->phase_bias;
                        ui->state.gravity_strength = state->gravity_strength;
                        ui->state.travel_scale = state->travel_scale;
                        ui->state.velocity_curve = state->velocity_curve;
                        ui->state.humanize = state->humanize;
                    }
                    if (state->version >= 5) {
                        ui->state.hold_latch_mode = state->hold_latch_mode;
                    }
                    request_redraw(ui);
                }
            } else if (ev->body.type == ui->atom_chunk_urid && ev->body.size >= sizeof(ArpIsoUiDelta)) {
                const ArpIsoUiDelta* delta = (const ArpIsoUiDelta*)(ev + 1);
                if (delta->magic != ARPISO_UI_DELTA_MAGIC ||
                    delta->version != ARPISO_UI_DELTA_VERSION) {
                    continue;
                }
                ui->has_state_sync = 1;
                ui->sync_source = 1;
                if (delta->target == ARPISO_UI_DELTA_GRID) {
                    if (delta->index_a < 8 && delta->index_b < 8) {
                        ui->state.grid[delta->index_a][delta->index_b] = delta->color;
                    }
                } else if (delta->target == ARPISO_UI_DELTA_SIDE) {
                    if (delta->index_a < 8) {
                        ui->state.side_buttons[delta->index_a] = delta->color;
                        set_status(ui, "Right control %u", (unsigned)(delta->index_a + 1));
                    }
                } else if (delta->target == ARPISO_UI_DELTA_TOP) {
                    if (delta->index_a < 9) {
                        ui->state.top_buttons[delta->index_a] = delta->color;
                        switch (delta->index_a) {
                            case 0:
                                set_status(ui, "Euclid pulses: %u", ui->state.euclid_pulses);
                                break;
                            case 1:
                                set_status(ui, "Euclid offset: %u", ui->state.euclid_offset);
                                break;
                            case 2:
                            case 3:
                                set_status(ui, "Active columns: %u", ui_active_columns(ui));
                                break;
                            case 4:
                                set_status(ui, "Pattern A selected");
                                break;
                            case 5:
                                set_status(ui, "Pattern B selected");
                                break;
                            case 6:
                                set_status(ui, "Cleared held wells");
                                break;
                            case 7:
                                set_status(ui, "Cleared pattern %c (Euclid reset)",
                                           (char)('A' + (ui->state.pattern & 1)));
                                break;
                            case 8:
                                set_status(ui, "Cleared pattern %c (Euclid reset)",
                                           (char)('A' + (ui->state.pattern & 1)));
                                break;
                            default:
                                break;
                        }
                    }
                }
                request_redraw(ui);
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
    ARPISO_UI_URI,
    instantiate,
    cleanup,
    port_event,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : NULL;
}
