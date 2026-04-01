#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/ui/ui.h>
#include <lv2/urid/urid.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo.h>
#include <cairo-xlib.h>

#include "../include/achord_ui_state.h"
#include "../include/chord_map.h"
#include "../include/launchpad_config.h"

#define ACHORD_UI_URI "https://danja.github.io/flues/plugins/achord#ui"

#define PORT_CONTROL_IN 0
#define PORT_LAUNCHPAD_OUT 2
#define PORT_PLAY_STATE 5
#define PORT_CURRENT_STEP 6
#define PORT_NOTIFY_OUT 7

#define GRID_DIM 8
#define PAD_SIZE 38
#define PAD_GAP 4
#define MARGIN 18
#define LEFT_LABEL_W 76
#define TOP_BUTTON_SIZE 28
#define COLUMN_LABEL_H 16
#define TOP_SECTION_H (TOP_BUTTON_SIZE + COLUMN_LABEL_H + 8)
#define SIDE_BUTTON_W 40
#define INFO_HEIGHT 280

#define WINDOW_WIDTH (MARGIN * 2 + LEFT_LABEL_W + GRID_DIM * (PAD_SIZE + PAD_GAP) + SIDE_BUTTON_W + 12)
#define WINDOW_HEIGHT (MARGIN * 2 + TOP_SECTION_H + GRID_DIM * (PAD_SIZE + PAD_GAP) + INFO_HEIGHT)

typedef struct {
    double r, g, b;
} Color;

typedef struct {
    Display *display;
    Window window;
    Window parent;
    int screen;
    int width;
    int height;

    cairo_surface_t *surface;
    cairo_t *cr;
    cairo_surface_t *back_buffer;
    cairo_t *back_cr;

    pthread_t thread;
    pthread_mutex_t mutex;
    volatile int running;
    volatile int needs_redraw;

    int mouse_down_active;
    uint8_t mouse_down_row;
    uint8_t mouse_down_col;

    AchordUiState state;

    LV2UI_Write_Function write_function;
    LV2UI_Controller controller;
    LV2_URID_Map *map;
    LV2_Atom_Forge forge;
    LV2_URID atom_event_transfer;
    LV2_URID midi_event_urid;
    LV2_URID atom_chunk_urid;
} AchordUI;

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

static Color palette_color(uint8_t index) {
    switch (index) {
        case 0: return (Color){0.06, 0.06, 0.08};
        case 1: return (Color){0.28, 0.28, 0.30};
        case 2: return (Color){0.45, 0.45, 0.47};
        case 3: return (Color){0.92, 0.92, 0.94};
        case 4: return (Color){0.35, 0.10, 0.12};
        case 5: return (Color){0.78, 0.18, 0.20};
        case 6: return (Color){0.98, 0.28, 0.30};
        case 7: return (Color){0.52, 0.28, 0.08};
        case 9: return (Color){0.90, 0.44, 0.15};
        case 84: return (Color){1.00, 0.62, 0.20};
        case 13: return (Color){0.90, 0.86, 0.22};
        case 14: return (Color){1.00, 0.98, 0.34};
        case 20: return (Color){0.12, 0.46, 0.20};
        case 21: return (Color){0.18, 0.72, 0.30};
        case 22: return (Color){0.25, 0.96, 0.42};
        case 33: return (Color){0.14, 0.44, 0.48};
        case 37: return (Color){0.18, 0.72, 0.78};
        case 38: return (Color){0.28, 0.92, 0.98};
        case 44: return (Color){0.14, 0.22, 0.52};
        case 45: return (Color){0.18, 0.38, 0.86};
        case 46: return (Color){0.28, 0.56, 0.98};
        case 48: return (Color){0.30, 0.16, 0.50};
        case 53: return (Color){0.58, 0.28, 0.88};
        case 54: return (Color){0.78, 0.44, 0.98};
        case 56: return (Color){0.56, 0.22, 0.44};
        case 57: return (Color){0.88, 0.30, 0.68};
        case 58: return (Color){0.98, 0.42, 0.80};
        default: return (Color){0.42, 0.42, 0.48};
    }
}

static double palette_brightness(uint8_t index) {
    if (index == 0) return 0.18;
    if (index == 3) return 1.0;
    if (index == 6 || index == 14 || index == 22 || index == 38 || index == 46 || index == 54 || index == 58 || index == 84) {
        return 0.98;
    }
    if (index == 1 || index == 4 || index == 7 || index == 20 || index == 33 || index == 44 || index == 48 || index == 56) {
        return 0.66;
    }
    return 0.78;
}

static void draw_text_centered(cairo_t *cr, double center_x, double baseline_y, const char *text) {
    cairo_text_extents_t extents;
    cairo_text_extents(cr, text, &extents);
    cairo_move_to(cr, center_x - (extents.width * 0.5 + extents.x_bearing), baseline_y);
    cairo_show_text(cr, text);
}

static void draw_pad(cairo_t *cr, double x, double y, double size, Color color, double brightness) {
    cairo_pattern_t *glow = cairo_pattern_create_radial(
        x + size * 0.5, y + size * 0.5, 0.0,
        x + size * 0.5, y + size * 0.5, size * 0.7
    );
    cairo_pattern_add_color_stop_rgba(glow, 0.0, color.r, color.g, color.b, brightness * 0.28);
    cairo_pattern_add_color_stop_rgba(glow, 1.0, color.r, color.g, color.b, 0.0);
    cairo_set_source(cr, glow);
    cairo_rectangle(cr, x - 3, y - 3, size + 6, size + 6);
    cairo_fill(cr);
    cairo_pattern_destroy(glow);

    cairo_rectangle(cr, x, y, size, size);
    cairo_set_source_rgb(cr, color.r * 0.25, color.g * 0.25, color.b * 0.25);
    cairo_fill(cr);

    cairo_pattern_t *fill = cairo_pattern_create_linear(x, y, x, y + size);
    cairo_pattern_add_color_stop_rgb(fill, 0.0,
                                     color.r * brightness,
                                     color.g * brightness,
                                     color.b * brightness);
    cairo_pattern_add_color_stop_rgb(fill, 1.0,
                                     color.r * brightness * 0.45,
                                     color.g * brightness * 0.45,
                                     color.b * brightness * 0.45);
    cairo_set_source(cr, fill);
    cairo_rectangle(cr, x, y, size, size);
    cairo_fill(cr);
    cairo_pattern_destroy(fill);

    cairo_rectangle(cr, x, y, size, size);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.12);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
}

static void send_midi3(AchordUI *ui, uint8_t a, uint8_t b, uint8_t c) {
    if (!ui->write_function || !ui->atom_event_transfer || !ui->midi_event_urid) {
        return;
    }

    uint8_t buf[16];
    uint8_t msg[3] = {a, b, c};
    lv2_atom_forge_set_buffer(&ui->forge, buf, sizeof(buf));
    lv2_atom_forge_atom(&ui->forge, 3, ui->midi_event_urid);
    lv2_atom_forge_write(&ui->forge, msg, 3);
    LV2_Atom *atom = (LV2_Atom *)buf;
    ui->write_function(ui->controller, PORT_CONTROL_IN,
                       lv2_atom_total_size(atom),
                       ui->atom_event_transfer,
                       atom);
}

static void request_redraw(AchordUI *ui) {
    ui->needs_redraw = 1;
}

static const char *inversion_name(int inversion) {
    switch (inversion) {
        case 1: return "1st";
        case 2: return "2nd";
        default: return "Root";
    }
}

static const char *clock_source_name(uint8_t source) {
    switch (source) {
        case 1: return "Host";
        case 2: return "Internal";
        default: return "Off";
    }
}

static int apply_launchpad_led_triplet(AchordUI *ui, uint8_t key, uint8_t color) {
    uint8_t row = 0;
    uint8_t col = 0;
    uint8_t index = 0;
    if (note_to_grid(key, &row, &col)) {
        ui->state.grid[row][col] = color;
        return 1;
    }
    if (is_side_button(key, &index)) {
        ui->state.side[index] = color;
        return 1;
    }
    if (is_top_button(key, &index)) {
        ui->state.top[index] = color;
        return 1;
    }
    return 0;
}

static int apply_launchpad_midi_to_ui_state(AchordUI *ui, const uint8_t *midi, uint32_t size) {
    if (!ui || !midi || size < 1) {
        return 0;
    }

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
                if (midi[i] == 0xF7) break;
                updated |= apply_launchpad_led_triplet(ui, midi[i + 1], midi[i + 2]);
                i += 3;
            }
            return updated;
        }
        return 0;
    }

    if (size >= 3) {
        const uint8_t status = midi[0] & 0xF0;
        if (status == 0x90 || status == 0xB0) {
            return apply_launchpad_led_triplet(ui, midi[1], midi[2]);
        }
    }
    return 0;
}

static int grid_hit_test(int x, int y, uint8_t *out_row, uint8_t *out_col) {
    const int grid_x = x - (MARGIN + LEFT_LABEL_W);
    const int grid_y = y - (MARGIN + TOP_SECTION_H);
    if (grid_x < 0 || grid_y < 0) return 0;

    const int cell = PAD_SIZE + PAD_GAP;
    const int col = grid_x / cell;
    const int row_from_top = grid_y / cell;
    if (col < 0 || col >= GRID_DIM || row_from_top < 0 || row_from_top >= GRID_DIM) return 0;
    if ((grid_x % cell) >= PAD_SIZE || (grid_y % cell) >= PAD_SIZE) return 0;

    *out_col = (uint8_t)col;
    *out_row = (uint8_t)((GRID_DIM - 1) - row_from_top);
    return 1;
}

static int top_hit_test(int x, int y, uint8_t *out_index) {
    const int local_x = x - (MARGIN + LEFT_LABEL_W);
    const int local_y = y - MARGIN;
    if (local_x < 0 || local_y < 0 || local_y >= TOP_BUTTON_SIZE) return 0;

    const int cell = PAD_SIZE + PAD_GAP;
    const int col = local_x / cell;
    if (col >= 0 && col < 8) {
        if ((local_x % cell) >= TOP_BUTTON_SIZE) return 0;
        *out_index = (uint8_t)col;
        return 1;
    }
    return 0;
}

static int side_hit_test(int x, int y, uint8_t *out_index) {
    const int local_x = x - (MARGIN + LEFT_LABEL_W + GRID_DIM * (PAD_SIZE + PAD_GAP) + 4);
    const int local_y = y - (MARGIN + TOP_SECTION_H);
    if (local_x < 0 || local_y < 0 || local_x >= SIDE_BUTTON_W) return 0;

    const int cell = PAD_SIZE + PAD_GAP;
    const int row_from_top = local_y / cell;
    if (row_from_top < 0 || row_from_top >= GRID_DIM) return 0;
    if ((local_y % cell) >= PAD_SIZE) return 0;
    *out_index = (uint8_t)((GRID_DIM - 1) - row_from_top);
    return 1;
}

static void draw_ui(AchordUI *ui) {
    cairo_t *cr = ui->back_cr;
    cairo_set_source_rgb(cr, 0.09, 0.09, 0.11);
    cairo_paint(cr);

    const char *top_labels[8] = {"Bank-", "Bank+", "Oct-", "Oct+", "Scale", "Reg", "Trig", "Hold"};
    const char *side_labels[8] = {"Bass", "Add9", "Sus", "Inv", "Spread", "Accent", "Lead", "Panic"};
    const double grid_top = MARGIN + TOP_SECTION_H;

    cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12);
    cairo_set_source_rgba(cr, 0.92, 0.92, 0.94, 0.9);
    cairo_move_to(cr, MARGIN, MARGIN + 16);
    cairo_show_text(cr, "Achord");

    for (uint8_t c = 0; c < GRID_DIM; ++c) {
        const double x = MARGIN + LEFT_LABEL_W + c * (PAD_SIZE + PAD_GAP);
        draw_pad(cr, x, MARGIN, TOP_BUTTON_SIZE, palette_color(ui->state.top[c]), palette_brightness(ui->state.top[c]));
        cairo_set_source_rgba(cr, 0.98, 0.98, 0.98, 0.92);
        cairo_set_font_size(cr, 8.5);
        draw_text_centered(cr, x + TOP_BUTTON_SIZE * 0.5, MARGIN + TOP_BUTTON_SIZE - 8, top_labels[c]);
    }

    cairo_set_source_rgba(cr, 0.84, 0.84, 0.88, 0.82);
    cairo_set_font_size(cr, 10.0);
    for (uint8_t c = 0; c < GRID_DIM; ++c) {
        char root_name[8];
        const double x = MARGIN + LEFT_LABEL_W + c * (PAD_SIZE + PAD_GAP);
        achord_root_name_for_column(ui->state.tonic_note, ui->state.bank_offset, c, root_name, sizeof(root_name));
        draw_text_centered(cr, x + PAD_SIZE * 0.5, MARGIN + TOP_BUTTON_SIZE + COLUMN_LABEL_H - 2, root_name);
    }

    for (uint8_t r = 0; r < GRID_DIM; ++r) {
        const double label_y = grid_top + (GRID_DIM - 1 - r) * (PAD_SIZE + PAD_GAP) + PAD_SIZE * 0.62;
        cairo_set_source_rgba(cr, 0.88, 0.88, 0.90, 0.82);
        cairo_set_font_size(cr, 10.5);
        cairo_move_to(cr, MARGIN, label_y);
        cairo_show_text(cr, achord_row_name(r));
    }

    for (uint8_t r = 0; r < GRID_DIM; ++r) {
        for (uint8_t c = 0; c < GRID_DIM; ++c) {
            const double x = MARGIN + LEFT_LABEL_W + c * (PAD_SIZE + PAD_GAP);
            const double y = grid_top + (GRID_DIM - 1 - r) * (PAD_SIZE + PAD_GAP);
            const uint8_t palette = ui->state.grid[r][c];
            draw_pad(cr, x, y, PAD_SIZE, palette_color(palette), palette_brightness(palette));
        }
    }

    for (uint8_t idx = 0; idx < GRID_DIM; ++idx) {
        const double x = MARGIN + LEFT_LABEL_W + GRID_DIM * (PAD_SIZE + PAD_GAP) + 4;
        const double y = grid_top + (GRID_DIM - 1 - idx) * (PAD_SIZE + PAD_GAP);
        draw_pad(cr, x, y, SIDE_BUTTON_W - 6,
                 palette_color(ui->state.side[idx]),
                 palette_brightness(ui->state.side[idx]));
        cairo_set_source_rgba(cr, 0.98, 0.98, 0.98, 0.88);
        cairo_set_font_size(cr, 9.0);
        cairo_move_to(cr, x + 4, y + PAD_SIZE - 10);
        cairo_show_text(cr, side_labels[idx]);
    }

    const double info_y = grid_top + GRID_DIM * (PAD_SIZE + PAD_GAP) + 10;
    cairo_set_source_rgba(cr, 0.96, 0.96, 0.98, 0.95);
    cairo_set_font_size(cr, 12);

    char line[256];
    char tonic_name[16];
    achord_note_name(ui->state.tonic_note, tonic_name, sizeof(tonic_name));
    snprintf(line, sizeof(line),
             "Host %s | Clock %s | %u active | BPM %u | Step %u",
             ui->state.host_playing ? "Playing" : "Stopped",
             clock_source_name(ui->state.clock_source),
             (unsigned)ui->state.active_chord_count,
             (unsigned)ui->state.bpm,
             (unsigned)ui->state.current_step16);
    cairo_move_to(cr, MARGIN, info_y + 18);
    cairo_show_text(cr, line);

    snprintf(line, sizeof(line),
             "Scale: %s   Register: %s   Trigger: %s   Hold: %s",
             achord_scale_name(ui->state.scale_index),
             achord_register_name(ui->state.register_mode),
             achord_trigger_name(ui->state.trigger_mode),
             achord_hold_name(ui->state.hold_mode));
    cairo_move_to(cr, MARGIN, info_y + 42);
    cairo_show_text(cr, line);

    snprintf(line, sizeof(line),
             "Bass %s  Add9 %s  Sus %s  Inversion %s  Spread %s",
             ui->state.bass_enabled ? "On" : "Off",
             ui->state.add9_enabled ? "On" : "Off",
             achord_sus_name(ui->state.sus_mode),
             inversion_name(ui->state.inversion_offset),
             achord_spread_name(ui->state.spread_mode));
    cairo_move_to(cr, MARGIN, info_y + 66);
    cairo_show_text(cr, line);

    snprintf(line, sizeof(line),
             "Accent %s  Voice Lead %s  Tonic %u  Bank %+d",
             ui->state.accent_enabled ? "On" : "Off",
             ui->state.voice_lead_enabled ? "On" : "Off",
             (unsigned)ui->state.tonic_note,
             (int)ui->state.bank_offset);
    cairo_move_to(cr, MARGIN, info_y + 90);
    cairo_show_text(cr, line);

    cairo_set_source_rgba(cr, 0.84, 0.84, 0.88, 0.74);
    cairo_set_font_size(cr, 10);
    cairo_move_to(cr, MARGIN, info_y + 116);
    cairo_show_text(cr, "Control keys:");

    cairo_set_source_rgba(cr, 0.90, 0.90, 0.94, 0.82);
    cairo_move_to(cr, MARGIN, info_y + 136);
    snprintf(line, sizeof(line), "91/92 Bank %+d fifths   93/94 Octave %s", (int)ui->state.bank_offset, tonic_name);
    cairo_show_text(cr, line);

    cairo_move_to(cr, MARGIN, info_y + 154);
    snprintf(line, sizeof(line), "95 Scale %s   96 Reg %s", achord_scale_name(ui->state.scale_index), achord_register_name(ui->state.register_mode));
    cairo_show_text(cr, line);

    cairo_move_to(cr, MARGIN, info_y + 172);
    snprintf(line, sizeof(line), "97 Trig %s   98 Hold %s", achord_trigger_name(ui->state.trigger_mode), achord_hold_name(ui->state.hold_mode));
    cairo_show_text(cr, line);

    cairo_move_to(cr, MARGIN, info_y + 190);
    snprintf(line, sizeof(line), "19 Bass %s   29 Add9 %s   39 Sus %s   49 Inv %s",
             ui->state.bass_enabled ? "On" : "Off",
             ui->state.add9_enabled ? "On" : "Off",
             achord_sus_name(ui->state.sus_mode),
             inversion_name(ui->state.inversion_offset));
    cairo_show_text(cr, line);

    cairo_move_to(cr, MARGIN, info_y + 208);
    snprintf(line, sizeof(line), "59 Spread %s   69 Accent %s   79 Lead %s   89 Panic Ready",
             achord_spread_name(ui->state.spread_mode),
             ui->state.accent_enabled ? "On" : "Off",
             ui->state.voice_lead_enabled ? "On" : "Off");
    cairo_show_text(cr, line);

    cairo_set_source_rgba(cr, 0.84, 0.84, 0.88, 0.74);
    cairo_move_to(cr, MARGIN, info_y + 236);
    cairo_show_text(cr, "Grid: white flash = quantized pending, pulse = repeat/strum, bright flash = latched.");

    cairo_move_to(cr, MARGIN, info_y + 254);
    cairo_show_text(cr, "Rows bottom->top: Bass Shell, Counterbass, Major, Minor, Dom7, Diminished, Maj7, Min7.");
}

static void redraw(AchordUI *ui) {
    if (!ui->back_cr || !ui->cr) return;
    draw_ui(ui);
    cairo_set_source_surface(ui->cr, ui->back_buffer, 0, 0);
    cairo_paint(ui->cr);
    cairo_surface_flush(ui->surface);
    XFlush(ui->display);
}

static void *event_thread_main(void *arg) {
    AchordUI *ui = (AchordUI *)arg;

    while (ui->running) {
        while (XPending(ui->display)) {
            XEvent event;
            XNextEvent(ui->display, &event);

            switch (event.type) {
                case Expose:
                    request_redraw(ui);
                    break;
                case ConfigureNotify: {
                    ui->width = event.xconfigure.width;
                    ui->height = event.xconfigure.height;
                    if (ui->surface) cairo_surface_destroy(ui->surface);
                    if (ui->cr) cairo_destroy(ui->cr);
                    if (ui->back_cr) cairo_destroy(ui->back_cr);
                    if (ui->back_buffer) cairo_surface_destroy(ui->back_buffer);

                    Visual *visual = DefaultVisual(ui->display, ui->screen);
                    ui->surface = cairo_xlib_surface_create(ui->display, ui->window, visual, ui->width, ui->height);
                    ui->cr = cairo_create(ui->surface);
                    ui->back_buffer = cairo_image_surface_create(CAIRO_FORMAT_RGB24, ui->width, ui->height);
                    ui->back_cr = cairo_create(ui->back_buffer);
                    request_redraw(ui);
                    break;
                }
                case ButtonPress: {
                    if (event.xbutton.button != Button1) {
                        break;
                    }
                    uint8_t row = 0;
                    uint8_t col = 0;
                    uint8_t index = 0;
                    if (grid_hit_test(event.xbutton.x, event.xbutton.y, &row, &col)) {
                        ui->mouse_down_active = 1;
                        ui->mouse_down_row = row;
                        ui->mouse_down_col = col;
                        send_midi3(ui, 0x90, grid_to_note(row, col), 96);
                    } else if (top_hit_test(event.xbutton.x, event.xbutton.y, &index)) {
                        send_midi3(ui, 0xB0, TOP_BUTTONS[index], 127);
                    } else if (side_hit_test(event.xbutton.x, event.xbutton.y, &index)) {
                        send_midi3(ui, 0xB0, SIDE_BUTTONS[index], 127);
                    }
                    break;
                }
                case ButtonRelease:
                    if (event.xbutton.button == Button1 && ui->mouse_down_active) {
                        send_midi3(ui, 0x80, grid_to_note(ui->mouse_down_row, ui->mouse_down_col), 0);
                        ui->mouse_down_active = 0;
                    }
                    break;
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

static LV2UI_Handle instantiate(const LV2UI_Descriptor *,
                                const char *,
                                const char *,
                                LV2UI_Write_Function write_function,
                                LV2UI_Controller controller,
                                LV2UI_Widget *widget,
                                const LV2_Feature *const *features) {
    AchordUI *ui = (AchordUI *)calloc(1, sizeof(AchordUI));
    if (!ui) return NULL;

    ui->write_function = write_function;
    ui->controller = controller;
    ui->width = WINDOW_WIDTH;
    ui->height = WINDOW_HEIGHT;
    ui->running = 1;
    ui->needs_redraw = 1;
    ui->state.magic = ACHORD_UI_STATE_MAGIC;
    ui->state.version = ACHORD_UI_STATE_VERSION;
    ui->state.tonic_note = 48;
    ui->state.scale_index = ACHORD_SCALE_MAJOR;
    ui->state.register_mode = ACHORD_REGISTER_8;
    ui->state.trigger_mode = ACHORD_TRIGGER_DIRECT;
    ui->state.hold_mode = ACHORD_HOLD_MOMENTARY;
    ui->state.voice_lead_enabled = 1;
    ui->state.bpm = 120;
    pthread_mutex_init(&ui->mutex, NULL);

    ensure_xlib_threads();
    ui->display = XOpenDisplay(NULL);
    if (!ui->display) {
        free(ui);
        return NULL;
    }

    ui->screen = DefaultScreen(ui->display);
    ui->parent = RootWindow(ui->display, ui->screen);

    for (int i = 0; features && features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_UI__parent)) {
            ui->parent = (Window)(uintptr_t)features[i]->data;
        } else if (!strcmp(features[i]->URI, LV2_URID__map)) {
            ui->map = (LV2_URID_Map *)features[i]->data;
        }
    }

    XSetWindowAttributes attrs;
    attrs.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask;
    attrs.background_pixel = BlackPixel(ui->display, ui->screen);

    ui->window = XCreateWindow(ui->display,
                               ui->parent,
                               0, 0,
                               ui->width, ui->height,
                               0,
                               CopyFromParent,
                               InputOutput,
                               CopyFromParent,
                               CWBackPixel | CWEventMask,
                               &attrs);

    XStoreName(ui->display, ui->window, "Achord");
    XMapWindow(ui->display, ui->window);

    Visual *visual = DefaultVisual(ui->display, ui->screen);
    ui->surface = cairo_xlib_surface_create(ui->display, ui->window, visual, ui->width, ui->height);
    ui->cr = cairo_create(ui->surface);
    ui->back_buffer = cairo_image_surface_create(CAIRO_FORMAT_RGB24, ui->width, ui->height);
    ui->back_cr = cairo_create(ui->back_buffer);

    if (ui->map) {
        lv2_atom_forge_init(&ui->forge, ui->map);
        ui->atom_event_transfer = ui->map->map(ui->map->handle, LV2_ATOM__eventTransfer);
        ui->midi_event_urid = ui->map->map(ui->map->handle, LV2_MIDI__MidiEvent);
        ui->atom_chunk_urid = ui->map->map(ui->map->handle, LV2_ATOM__Chunk);
    }

    *widget = (LV2UI_Widget)(intptr_t)ui->window;
    redraw(ui);
    pthread_create(&ui->thread, NULL, event_thread_main, ui);
    return (LV2UI_Handle)ui;
}

static void cleanup(LV2UI_Handle handle) {
    AchordUI *ui = (AchordUI *)handle;
    if (!ui) return;

    ui->running = 0;
    if (ui->thread) {
        pthread_join(ui->thread, NULL);
    }

    if (ui->back_cr) cairo_destroy(ui->back_cr);
    if (ui->back_buffer) cairo_surface_destroy(ui->back_buffer);
    if (ui->cr) cairo_destroy(ui->cr);
    if (ui->surface) cairo_surface_destroy(ui->surface);

    if (ui->display) {
        if (ui->window) {
            XUnmapWindow(ui->display, ui->window);
            XSync(ui->display, False);
            XDestroyWindow(ui->display, ui->window);
        }
        XCloseDisplay(ui->display);
    }

    free(ui);
}

static void port_event(LV2UI_Handle handle,
                       uint32_t port_index,
                       uint32_t buffer_size,
                       uint32_t,
                       const void *buffer) {
    AchordUI *ui = (AchordUI *)handle;
    if (!ui || !buffer) return;

    if (port_index == PORT_PLAY_STATE && buffer_size >= sizeof(float)) {
        const float value = *(const float *)buffer;
        ui->state.host_playing = value > 0.5f ? 1 : 0;
        request_redraw(ui);
        return;
    }

    if (port_index == PORT_CURRENT_STEP && buffer_size >= sizeof(float)) {
        float value = *(const float *)buffer;
        if (value < 0.0f) value = 0.0f;
        if (value > 15.0f) value = 15.0f;
        ui->state.current_step16 = (uint8_t)value;
        request_redraw(ui);
        return;
    }

    if (buffer_size < sizeof(LV2_Atom_Sequence)) {
        return;
    }

    const LV2_Atom_Sequence *seq = (const LV2_Atom_Sequence *)buffer;
    if (port_index == PORT_LAUNCHPAD_OUT) {
        int updated = 0;
        LV2_ATOM_SEQUENCE_FOREACH(seq, ev) {
            if (ev->body.type != ui->midi_event_urid) {
                continue;
            }
            const uint8_t *midi = (const uint8_t *)(ev + 1);
            updated |= apply_launchpad_midi_to_ui_state(ui, midi, ev->body.size);
        }
        if (updated) {
            request_redraw(ui);
        }
        return;
    }

    if (port_index != PORT_NOTIFY_OUT) {
        return;
    }

    LV2_ATOM_SEQUENCE_FOREACH(seq, ev) {
        if (ev->body.type == ui->atom_chunk_urid && ev->body.size >= sizeof(AchordUiState)) {
            const AchordUiState *state = (const AchordUiState *)(ev + 1);
            if (state->magic == ACHORD_UI_STATE_MAGIC && state->version == ACHORD_UI_STATE_VERSION) {
                memcpy(&ui->state, state, sizeof(AchordUiState));
                request_redraw(ui);
            }
        }
    }
}

static const void *extension_data(const char *uri) {
    (void)uri;
    return NULL;
}

static const LV2UI_Descriptor descriptor = {
    ACHORD_UI_URI,
    instantiate,
    cleanup,
    port_event,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor *lv2ui_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : NULL;
}
