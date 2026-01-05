/*
 * grid-seq - Grid-based MIDI sequencer LV2 plugin
 *
 * Copyright (C) 2025 Danny
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE.
 */

// Raw X11 + Cairo UI implementation for proper event handling in Reaper

#include "grid_seq/common.h"
#include "state.h"

#include <lv2/core/lv2.h>
#include <lv2/ui/ui.h>
#include <lv2/urid/urid.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/midi/midi.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define UI_URI PLUGIN_URI "#ui"
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480
#define GRID_MARGIN 20
#define GRID_SPACING 2

typedef struct {
    Display* display;
    Window window;
    Window settings_window;
    Visual* visual;
    int screen;
    cairo_surface_t* surface;
    cairo_surface_t* settings_surface;
    cairo_t* cr;

    // Double buffering
    cairo_surface_t* back_buffer;
    cairo_t* back_cr;
    int width;
    int height;
    pthread_t thread;
    pthread_mutex_t mutex;
    bool running;

    LV2UI_Write_Function write_function;
    LV2UI_Controller controller;

    GridSeqState state;

    int cell_size;
    bool needs_redraw;
    bool settings_open;

    // Settings values
    uint8_t pending_length;
    bool pending_filter;
    bool midi_filter_enabled;
    uint8_t pending_channel;
    uint8_t midi_channel;

    // Hover tooltip
    bool hover_active;
    char hover_text[64];

    LV2_URID_Map* map;
    const LV2UI_Port_Subscribe* port_subscribe;

    // URIDs for atom forge
    LV2_URID midi_MidiEvent;
    LV2_Atom_Forge forge;
} GridSeqX11UI;

// Xlib threading guard (match padseq stability pattern)
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

static void recreate_surfaces(GridSeqX11UI* ui, int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }

    ui->width = width;
    ui->height = height;

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

    ui->surface = cairo_xlib_surface_create(
        ui->display, ui->window, ui->visual,
        ui->width, ui->height
    );
    ui->cr = cairo_create(ui->surface);
    ui->back_buffer = cairo_image_surface_create(CAIRO_FORMAT_RGB24, ui->width, ui->height);
    ui->back_cr = cairo_create(ui->back_buffer);
    ui->needs_redraw = true;
}

static void draw_grid(GridSeqX11UI* ui) {
    if (!ui->surface || !ui->back_cr || !ui->cr) return;

    cairo_t* cr = ui->back_cr;

    // Clear background
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_paint(cr);

    // Calculate cell size based on sequence length
    int visible_cols = ui->state.sequence_length;
    int available_width = ui->width - 2 * GRID_MARGIN;
    int available_height = ui->height - 2 * GRID_MARGIN;

    int cell_width = (available_width - (visible_cols - 1) * GRID_SPACING) / visible_cols;
    int cell_height = (available_height - (GRID_VISIBLE_ROWS - 1) * GRID_SPACING) / GRID_VISIBLE_ROWS;
    ui->cell_size = (cell_width < cell_height) ? cell_width : cell_height;

    // Draw grid cells
    for (int x = 0; x < visible_cols; x++) {
        for (int y = 0; y < GRID_VISIBLE_ROWS; y++) {
            int px = GRID_MARGIN + x * (ui->cell_size + GRID_SPACING);
            int py = GRID_MARGIN + y * (ui->cell_size + GRID_SPACING);

            // Flip Y coordinate for display
            int grid_y = GRID_VISIBLE_ROWS - 1 - y;
            bool active = ui->state.grid[x][grid_y];

            // Highlight current step
            if (x == ui->state.current_step) {
                cairo_set_source_rgb(cr, 0.3, 0.3, 0.5);
            } else if (active) {
                cairo_set_source_rgb(cr, 0.8, 0.8, 0.2);
            } else {
                cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
            }

            cairo_rectangle(cr, px, py, ui->cell_size, ui->cell_size);
            cairo_fill(cr);

            // Border
            cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
            cairo_set_line_width(cr, 1.0);
            cairo_rectangle(cr, px, py, ui->cell_size, ui->cell_size);
            cairo_stroke(cr);
        }
    }

    // Beats per bar readout
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 14);
    char beats_text[32];
    snprintf(beats_text, sizeof(beats_text), "Beats/Bar: %d", ui->state.beats_per_bar);
    cairo_move_to(cr, GRID_MARGIN, GRID_MARGIN - 6);
    cairo_show_text(cr, beats_text);

    if (ui->hover_active && ui->hover_text[0] != '\0') {
        cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        cairo_set_font_size(cr, 12);
        cairo_move_to(cr, GRID_MARGIN, ui->height - GRID_MARGIN + 14);
        cairo_show_text(cr, ui->hover_text);
    }

    // Draw buttons in vertical column on the right
    int button_size = 30;
    int button_spacing = 5;
    int buttons_x = ui->width - button_size - 10;
    int buttons_start_y = 10;

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 18);

    int current_y = buttons_start_y;

    // Settings button (S)
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.4);
    cairo_rectangle(cr, buttons_x, current_y, button_size, button_size);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_move_to(cr, buttons_x + 10, current_y + 21);
    cairo_show_text(cr, "S");
    current_y += button_size + button_spacing;

    // Reset button (R)
    cairo_set_source_rgb(cr, 0.5, 0.2, 0.2);
    cairo_rectangle(cr, buttons_x, current_y, button_size, button_size);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 1.0, 0.6, 0.6);
    cairo_move_to(cr, buttons_x + 10, current_y + 21);
    cairo_show_text(cr, "R");
    current_y += button_size + button_spacing;

    // Query button (?)
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.5);
    cairo_rectangle(cr, buttons_x, current_y, button_size, button_size);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.6, 0.6, 1.0);
    cairo_move_to(cr, buttons_x + 10, current_y + 21);
    cairo_show_text(cr, "?");
    current_y += button_size + button_spacing;

    // Clear button (C)
    cairo_set_source_rgb(cr, 0.6, 0.4, 0.1);
    cairo_rectangle(cr, buttons_x, current_y, button_size, button_size);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 1.0, 0.8, 0.4);
    cairo_move_to(cr, buttons_x + 10, current_y + 21);
    cairo_show_text(cr, "C");
    current_y += button_size + button_spacing;

    // Re-center button (H for Home)
    cairo_set_source_rgb(cr, 0.2, 0.4, 0.2);
    cairo_rectangle(cr, buttons_x, current_y, button_size, button_size);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.6, 1.0, 0.6);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 18);
    cairo_move_to(cr, buttons_x + 9, current_y + 21);
    cairo_show_text(cr, "H");
    current_y += button_size + button_spacing;

    // Up button (pitch shift up) - using + symbol
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.5);
    cairo_rectangle(cr, buttons_x, current_y, button_size, button_size);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.8, 0.8, 1.0);
    cairo_set_font_size(cr, 22);
    cairo_move_to(cr, buttons_x + 8, current_y + 22);
    cairo_show_text(cr, "+");
    current_y += button_size + button_spacing;

    // Down button (pitch shift down) - using - symbol
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.5);
    cairo_rectangle(cr, buttons_x, current_y, button_size, button_size);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.8, 0.8, 1.0);
    cairo_set_font_size(cr, 22);
    cairo_move_to(cr, buttons_x + 10, current_y + 20);
    cairo_show_text(cr, "-");

    cairo_set_source_surface(ui->cr, ui->back_buffer, 0, 0);
    cairo_paint(ui->cr);
    cairo_surface_flush(ui->surface);
    XFlush(ui->display);
}

static void draw_settings_dialog(GridSeqX11UI* ui) {
    if (!ui->settings_surface) return;

    cairo_t* cr = cairo_create(ui->settings_surface);

    // Background
    cairo_set_source_rgb(cr, 0.15, 0.15, 0.15);
    cairo_paint(cr);

    // Title
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_font_size(cr, 18);
    cairo_move_to(cr, 20, 30);
    cairo_show_text(cr, "Settings");

    // Sequence Length label
    cairo_set_font_size(cr, 14);
    cairo_move_to(cr, 20, 70);
    cairo_show_text(cr, "Sequence Length:");

    // Draw slider track
    int slider_x = 20;
    int slider_y = 85;
    int slider_width = 260;
    int slider_height = 10;

    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_rectangle(cr, slider_x, slider_y, slider_width, slider_height);
    cairo_fill(cr);

    // Draw slider thumb
    float thumb_pos = (float)(ui->pending_length - MIN_SEQUENCE_LENGTH) /
                      (float)(MAX_SEQUENCE_LENGTH - MIN_SEQUENCE_LENGTH);
    int thumb_x = slider_x + (int)(thumb_pos * slider_width) - 5;

    cairo_set_source_rgb(cr, 0.7, 0.7, 0.9);
    cairo_rectangle(cr, thumb_x, slider_y - 5, 10, slider_height + 10);
    cairo_fill(cr);

    // Display value
    char value_text[16];
    snprintf(value_text, sizeof(value_text), "%d steps", ui->pending_length);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, slider_x + slider_width + 15, slider_y + 10);
    cairo_show_text(cr, value_text);

    // MIDI Channel label
    cairo_move_to(cr, 20, 120);
    cairo_show_text(cr, "MIDI Channel:");

    int chan_x = 20;
    int chan_y = 135;
    int chan_width = 260;
    int chan_height = 10;

    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_rectangle(cr, chan_x, chan_y, chan_width, chan_height);
    cairo_fill(cr);

    float chan_pos = (float)(ui->pending_channel - 1) / 15.0f;
    int chan_thumb_x = chan_x + (int)(chan_pos * chan_width) - 5;

    cairo_set_source_rgb(cr, 0.7, 0.7, 0.9);
    cairo_rectangle(cr, chan_thumb_x, chan_y - 5, 10, chan_height + 10);
    cairo_fill(cr);

    char chan_text[16];
    snprintf(chan_text, sizeof(chan_text), "Ch %d", ui->pending_channel);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, chan_x + chan_width + 15, chan_y + 10);
    cairo_show_text(cr, chan_text);

    // MIDI Filter checkbox label
    cairo_move_to(cr, 20, 170);
    cairo_show_text(cr, "MIDI Filter (Note-Ons Only):");

    // Draw checkbox
    int checkbox_x = 310;
    int checkbox_y = 155;
    int checkbox_size = 20;

    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_rectangle(cr, checkbox_x, checkbox_y, checkbox_size, checkbox_size);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, checkbox_x, checkbox_y, checkbox_size, checkbox_size);
    cairo_stroke(cr);

    // Draw checkmark if enabled
    if (ui->pending_filter) {
        cairo_set_source_rgb(cr, 0.2, 0.8, 0.2);
        cairo_set_line_width(cr, 2.0);
        cairo_move_to(cr, checkbox_x + 4, checkbox_y + 10);
        cairo_line_to(cr, checkbox_x + 8, checkbox_y + 16);
        cairo_line_to(cr, checkbox_x + 16, checkbox_y + 4);
        cairo_stroke(cr);
    }

    // OK button
    int ok_x = 60;
    int ok_y = 210;
    int button_width = 80;
    int button_height = 30;

    cairo_set_source_rgb(cr, 0.3, 0.5, 0.3);
    cairo_rectangle(cr, ok_x, ok_y, button_width, button_height);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, ok_x + 25, ok_y + 20);
    cairo_show_text(cr, "OK");

    // Cancel button
    int cancel_x = 160;

    cairo_set_source_rgb(cr, 0.5, 0.3, 0.3);
    cairo_rectangle(cr, cancel_x, ok_y, button_width, button_height);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, cancel_x + 15, ok_y + 20);
    cairo_show_text(cr, "Cancel");

    cairo_destroy(cr);
}

static void open_settings_dialog(GridSeqX11UI* ui) {
    if (ui->settings_open) return;

    // Store current values
    ui->pending_length = ui->state.sequence_length;
    ui->pending_filter = ui->midi_filter_enabled;
    ui->pending_channel = ui->midi_channel;

    // Create settings window
    XSetWindowAttributes attrs;
    attrs.background_pixel = BlackPixel(ui->display, ui->screen);
    attrs.event_mask = ButtonPressMask | ExposureMask;
    attrs.override_redirect = True;  // Modal-like behavior

    ui->settings_window = XCreateWindow(
        ui->display, ui->window,
        (WINDOW_WIDTH - 360) / 2, (WINDOW_HEIGHT - 260) / 2,
        360, 260, 2,
        CopyFromParent, InputOutput, CopyFromParent,
        CWBackPixel | CWEventMask | CWOverrideRedirect, &attrs
    );

    ui->settings_surface = cairo_xlib_surface_create(
        ui->display, ui->settings_window, ui->visual, 360, 260
    );

    XMapWindow(ui->display, ui->settings_window);
    XRaiseWindow(ui->display, ui->settings_window);

    ui->settings_open = true;
    draw_settings_dialog(ui);
    XFlush(ui->display);
}

static void close_settings_dialog(GridSeqX11UI* ui, bool apply) {
    if (!ui->settings_open) return;

    if (apply) {
        // Write new length to plugin
        if (ui->pending_length != ui->state.sequence_length) {
            float length_value = (float)ui->pending_length;
            ui->write_function(ui->controller, 24, sizeof(float), 0, &length_value);
        }

        if (ui->pending_channel != ui->midi_channel) {
            float channel_value = (float)ui->pending_channel;
            ui->write_function(ui->controller, 27, sizeof(float), 0, &channel_value);
            ui->midi_channel = ui->pending_channel;
        }

        // Write MIDI filter setting
        if (ui->pending_filter != ui->midi_filter_enabled) {
            float filter_value = ui->pending_filter ? 1.0f : 0.0f;
            ui->write_function(ui->controller, 25, sizeof(float), 0, &filter_value);
            ui->midi_filter_enabled = ui->pending_filter;
        }
    }

    if (ui->settings_surface) {
        cairo_surface_destroy(ui->settings_surface);
        ui->settings_surface = NULL;
    }

    if (ui->settings_window) {
        XDestroyWindow(ui->display, ui->settings_window);
        ui->settings_window = 0;
    }

    ui->settings_open = false;
}

static void handle_settings_click(GridSeqX11UI* ui, int mx, int my) {
    // Slider interaction
    int slider_x = 20;
    int slider_y = 85;
    int slider_width = 260;
    int slider_height = 10;

    if (mx >= slider_x && mx <= slider_x + slider_width &&
        my >= slider_y - 5 && my <= slider_y + slider_height + 5) {
        float pos = (float)(mx - slider_x) / (float)slider_width;
        if (pos < 0.0f) pos = 0.0f;
        if (pos > 1.0f) pos = 1.0f;

        // Use rounding to ensure we can reach 16
        ui->pending_length = MIN_SEQUENCE_LENGTH +
                            (uint8_t)(pos * (MAX_SEQUENCE_LENGTH - MIN_SEQUENCE_LENGTH) + 0.5f);

        // Clamp to valid range
        if (ui->pending_length > MAX_SEQUENCE_LENGTH) {
            ui->pending_length = MAX_SEQUENCE_LENGTH;
        }

        draw_settings_dialog(ui);
        XFlush(ui->display);
        return;
    }

    // MIDI channel slider
    int chan_x = 20;
    int chan_y = 135;
    int chan_width = 260;
    int chan_height = 10;

    if (mx >= chan_x && mx <= chan_x + chan_width &&
        my >= chan_y - 5 && my <= chan_y + chan_height + 5) {
        float pos = (float)(mx - chan_x) / (float)chan_width;
        if (pos < 0.0f) pos = 0.0f;
        if (pos > 1.0f) pos = 1.0f;

        ui->pending_channel = 1 + (uint8_t)(pos * 15.0f + 0.5f);
        if (ui->pending_channel < 1) ui->pending_channel = 1;
        if (ui->pending_channel > 16) ui->pending_channel = 16;

        draw_settings_dialog(ui);
        XFlush(ui->display);
        return;
    }

    // Checkbox interaction
    int checkbox_x = 310;
    int checkbox_y = 155;
    int checkbox_size = 20;

    if (mx >= checkbox_x && mx <= checkbox_x + checkbox_size &&
        my >= checkbox_y && my <= checkbox_y + checkbox_size) {
        ui->pending_filter = !ui->pending_filter;
        draw_settings_dialog(ui);
        XFlush(ui->display);
        return;
    }

    // OK button
    int ok_x = 60;
    int ok_y = 210;
    int button_width = 80;
    int button_height = 30;

    if (mx >= ok_x && mx <= ok_x + button_width &&
        my >= ok_y && my <= ok_y + button_height) {
        close_settings_dialog(ui, true);
        return;
    }

    // Cancel button
    int cancel_x = 160;

    if (mx >= cancel_x && mx <= cancel_x + button_width &&
        my >= ok_y && my <= ok_y + button_height) {
        close_settings_dialog(ui, false);
        return;
    }
}

static void update_hover(GridSeqX11UI* ui, int mx, int my) {
    const int button_size = 30;
    const int button_spacing = 5;
    const int buttons_x = ui->width - button_size - 10;
    const int buttons_start_y = 10;

    ui->hover_active = false;
    ui->hover_text[0] = '\0';

    if (mx < buttons_x || mx > buttons_x + button_size) {
        return;
    }

    int current_y = buttons_start_y;
    const char* labels[] = {
        "Settings", "Reset Launchpad", "Device Query",
        "Clear Pattern", "Recenter Pitch", "Pitch Up", "Pitch Down"
    };

    for (int i = 0; i < 7; i++) {
        if (my >= current_y && my <= current_y + button_size) {
            ui->hover_active = true;
            snprintf(ui->hover_text, sizeof(ui->hover_text), "%s", labels[i]);
            ui->needs_redraw = true;
            return;
        }
        current_y += button_size + button_spacing;
    }
}
static void handle_button_press(GridSeqX11UI* ui, int mx, int my) {
    fprintf(stderr, "grid-seq: X11 button press at (%d, %d)\n", mx, my);

    // Buttons are in vertical column on the right
    int button_size = 30;
    int button_spacing = 5;
    int buttons_x = ui->width - button_size - 10;
    int buttons_start_y = 10;

    // Check if click is in button column
    if (mx >= buttons_x && mx <= buttons_x + button_size) {
        int current_y = buttons_start_y;

        // Settings button (S)
        if (my >= current_y && my <= current_y + button_size) {
            fprintf(stderr, "grid-seq: Settings button clicked\n");
            open_settings_dialog(ui);
            return;
        }
        current_y += button_size + button_spacing;

        // Reset button (R)
        if (my >= current_y && my <= current_y + button_size) {
            fprintf(stderr, "grid-seq: Reset button clicked\n");
            float reset_signal = -100.0f;
            ui->write_function(ui->controller, 3, sizeof(float), 0, &reset_signal);
            return;
        }
        current_y += button_size + button_spacing;

        // Query button (?)
        if (my >= current_y && my <= current_y + button_size) {
            fprintf(stderr, "grid-seq: Query button clicked\n");
            float query_signal = -200.0f;
            ui->write_function(ui->controller, 3, sizeof(float), 0, &query_signal);
            return;
        }
        current_y += button_size + button_spacing;

        // Clear button (C)
        if (my >= current_y && my <= current_y + button_size) {
            fprintf(stderr, "grid-seq: Clear button clicked\n");

            // Clear local state for immediate visual feedback
            pthread_mutex_lock(&ui->mutex);
            for (int i = 0; i < MAX_GRID_SIZE; i++) {
                for (int j = 0; j < GRID_PITCH_RANGE; j++) {
                    ui->state.grid[i][j] = false;
                }
            }
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);

            float clear_signal = -300.0f;
            ui->write_function(ui->controller, 3, sizeof(float), 0, &clear_signal);
            return;
        }
        current_y += button_size + button_spacing;

        // Re-center button (⌂)
        if (my >= current_y && my <= current_y + button_size) {
            fprintf(stderr, "grid-seq: Re-center button clicked\n");
            float recenter_signal = -400.0f;
            ui->write_function(ui->controller, 3, sizeof(float), 0, &recenter_signal);
            return;
        }
        current_y += button_size + button_spacing;

        // Up button (+) - pitch shift up
        // Send MIDI CC 92 message (same as Launchpad up button)
        if (my >= current_y && my <= current_y + button_size) {
            fprintf(stderr, "grid-seq: Pitch up button clicked - sending CC 92\n");

            // Create a buffer for the atom sequence
            uint8_t buf[128];
            lv2_atom_forge_set_buffer(&ui->forge, buf, sizeof(buf));

            // Forge the MIDI CC message
            LV2_Atom_Forge_Frame frame;
            lv2_atom_forge_sequence_head(&ui->forge, &frame, 0);
            lv2_atom_forge_frame_time(&ui->forge, 0);

            uint8_t midi_msg[3] = {0xB0, 92, 127};  // CC 92, value 127
            lv2_atom_forge_atom(&ui->forge, 3, ui->midi_MidiEvent);
            lv2_atom_forge_write(&ui->forge, midi_msg, 3);

            lv2_atom_forge_pop(&ui->forge, &frame);

            // Write to MIDI input port (port 0)
            ui->write_function(ui->controller, 0, lv2_atom_total_size(&((LV2_Atom_Sequence*)buf)->atom),
                             ui->map->map(ui->map->handle, LV2_ATOM__Sequence),
                             buf);
            return;
        }
        current_y += button_size + button_spacing;

        // Down button (-) - pitch shift down
        // Send MIDI CC 91 message (same as Launchpad down button)
        if (my >= current_y && my <= current_y + button_size) {
            fprintf(stderr, "grid-seq: Pitch down button clicked - sending CC 91\n");

            // Create a buffer for the atom sequence
            uint8_t buf[128];
            lv2_atom_forge_set_buffer(&ui->forge, buf, sizeof(buf));

            // Forge the MIDI CC message
            LV2_Atom_Forge_Frame frame;
            lv2_atom_forge_sequence_head(&ui->forge, &frame, 0);
            lv2_atom_forge_frame_time(&ui->forge, 0);

            uint8_t midi_msg[3] = {0xB0, 91, 127};  // CC 91, value 127
            lv2_atom_forge_atom(&ui->forge, 3, ui->midi_MidiEvent);
            lv2_atom_forge_write(&ui->forge, midi_msg, 3);

            lv2_atom_forge_pop(&ui->forge, &frame);

            // Write to MIDI input port (port 0)
            ui->write_function(ui->controller, 0, lv2_atom_total_size(&((LV2_Atom_Sequence*)buf)->atom),
                             ui->map->map(ui->map->handle, LV2_ATOM__Sequence),
                             buf);
            return;
        }
    }

    // Calculate grid cell click
    int x = (mx - GRID_MARGIN) / (ui->cell_size + GRID_SPACING);
    int y = (my - GRID_MARGIN) / (ui->cell_size + GRID_SPACING);

    if (x >= 0 && x < ui->state.sequence_length && y >= 0 && y < GRID_VISIBLE_ROWS) {
        // Flip Y coordinate - send window-relative row (0-7)
        // Plugin will add pitch_offset to get absolute MIDI note
        int grid_y = GRID_VISIBLE_ROWS - 1 - y;

        // Send to plugin - DON'T toggle locally, wait for port update from plugin
        float fx = (float)x;
        float fy = (float)grid_y;
        ui->write_function(ui->controller, 3, sizeof(float), 0, &fx);  // PORT_GRID_X
        ui->write_function(ui->controller, 4, sizeof(float), 0, &fy);  // PORT_GRID_Y

        fprintf(stderr, "grid-seq: Sent toggle request for cell [%d,%d] (window-relative row)\n", x, grid_y);
    }
}

static void handle_configure(GridSeqX11UI* ui, const XConfigureEvent* cfg) {
    if (!cfg) return;
    if (cfg->width == ui->width && cfg->height == ui->height) {
        return;
    }
    pthread_mutex_lock(&ui->mutex);
    recreate_surfaces(ui, cfg->width, cfg->height);
    pthread_mutex_unlock(&ui->mutex);
}

static void* event_thread_main(void* arg) {
    GridSeqX11UI* ui = (GridSeqX11UI*)arg;

    while (ui->running) {
        while (XPending(ui->display) > 0) {
            XEvent event;
            XNextEvent(ui->display, &event);

            switch (event.type) {
                case ButtonPress:
                    if (ui->settings_open && event.xbutton.window == ui->settings_window) {
                        handle_settings_click(ui, event.xbutton.x, event.xbutton.y);
                    } else {
                        handle_button_press(ui, event.xbutton.x, event.xbutton.y);
                    }
                    break;
                case Expose:
                    if (ui->settings_open && event.xexpose.window == ui->settings_window) {
                        draw_settings_dialog(ui);
                        XFlush(ui->display);
                    } else {
                        ui->needs_redraw = true;
                    }
                    break;
                case MotionNotify:
                    if (!ui->settings_open) {
                        update_hover(ui, event.xmotion.x, event.xmotion.y);
                    }
                    break;
                case ConfigureNotify:
                    if (event.xconfigure.window == ui->window) {
                        handle_configure(ui, &event.xconfigure);
                    }
                    break;
                default:
                    break;
            }
        }

        if (ui->needs_redraw) {
            pthread_mutex_lock(&ui->mutex);
            draw_grid(ui);
            ui->needs_redraw = false;
            pthread_mutex_unlock(&ui->mutex);
        }

        struct timespec sleep_time = {0};
        sleep_time.tv_nsec = 16000000;
        nanosleep(&sleep_time, NULL);
    }

    return NULL;
}

static LV2UI_Handle instantiate(
    const LV2UI_Descriptor* descriptor,
    const char* plugin_uri,
    const char* bundle_path,
    LV2UI_Write_Function write_function,
    LV2UI_Controller controller,
    LV2UI_Widget* widget,
    const LV2_Feature* const* features
) {
    (void)descriptor;
    (void)plugin_uri;
    (void)bundle_path;

    GridSeqX11UI* ui = (GridSeqX11UI*)calloc(1, sizeof(GridSeqX11UI));
    if (!ui) return NULL;

    ui->write_function = write_function;
    ui->controller = controller;

    // Get features
    void* parent = NULL;
    for (int i = 0; features[i]; i++) {
        if (strcmp(features[i]->URI, LV2_URID__map) == 0) {
            ui->map = (LV2_URID_Map*)features[i]->data;
        } else if (strcmp(features[i]->URI, LV2_UI__portSubscribe) == 0) {
            ui->port_subscribe = (const LV2UI_Port_Subscribe*)features[i]->data;
        } else if (strcmp(features[i]->URI, LV2_UI__parent) == 0) {
            parent = features[i]->data;
        }
    }

    if (!ui->map) {
        free(ui);
        return NULL;
    }

    // Map URIDs for MIDI messages
    ui->midi_MidiEvent = ui->map->map(ui->map->handle, LV2_MIDI__MidiEvent);

    // Initialize atom forge
    lv2_atom_forge_init(&ui->forge, ui->map);

    // Initialize state
    state_init(&ui->state, 48000.0);
    ui->needs_redraw = true;
    ui->settings_open = false;
    ui->settings_window = 0;
    ui->settings_surface = NULL;
    ui->midi_filter_enabled = false;
    ui->midi_channel = 1;
    ui->pending_channel = ui->midi_channel;
    ui->hover_active = false;
    ui->hover_text[0] = '\0';

    ensure_xlib_threads();

    // Open X11 display
    ui->display = XOpenDisplay(NULL);
    if (!ui->display) {
        fprintf(stderr, "grid-seq: Failed to open X11 display\n");
        free(ui);
        return NULL;
    }

    ui->screen = DefaultScreen(ui->display);
    ui->visual = DefaultVisual(ui->display, ui->screen);
    Window root = DefaultRootWindow(ui->display);
    ui->width = WINDOW_WIDTH;
    ui->height = WINDOW_HEIGHT;
    pthread_mutex_init(&ui->mutex, NULL);
    ui->running = true;

    // Create window
    XSetWindowAttributes attrs;
    attrs.background_pixel = BlackPixel(ui->display, ui->screen);
    attrs.event_mask = ButtonPressMask | ButtonReleaseMask | ExposureMask | StructureNotifyMask | PointerMotionMask;

    ui->window = XCreateWindow(
        ui->display, parent ? (Window)(uintptr_t)parent : root,
        0, 0, ui->width, ui->height, 0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWBackPixel | CWEventMask, &attrs
    );

    if (!ui->window) {
        fprintf(stderr, "grid-seq: Failed to create X11 window\n");
        XCloseDisplay(ui->display);
        free(ui);
        return NULL;
    }

    // Create Cairo surface
    recreate_surfaces(ui, ui->width, ui->height);

    XMapWindow(ui->display, ui->window);
    XFlush(ui->display);

    // Subscribe to ports
    if (ui->port_subscribe) {
        // Subscribe to current_step (port 5)
        ui->port_subscribe->subscribe(ui->port_subscribe->handle, 5, 0, NULL);

        // Subscribe to grid row ports (8-23)
        for (uint32_t i = 8; i <= 23; i++) {
            ui->port_subscribe->subscribe(ui->port_subscribe->handle, i, 0, NULL);
        }

        // Subscribe to sequence_length (port 24)
        ui->port_subscribe->subscribe(ui->port_subscribe->handle, 24, 0, NULL);

        // Subscribe to midi_filter (port 25)
        ui->port_subscribe->subscribe(ui->port_subscribe->handle, 25, 0, NULL);

        // Subscribe to beats_per_bar (port 26)
        ui->port_subscribe->subscribe(ui->port_subscribe->handle, 26, 0, NULL);

        // Subscribe to midi_channel (port 27)
        ui->port_subscribe->subscribe(ui->port_subscribe->handle, 27, 0, NULL);
    }

    *widget = (LV2UI_Widget)(uintptr_t)ui->window;

    fprintf(stderr, "grid-seq: X11 UI created, window=0x%lx parent=%p\n", ui->window, parent);

    pthread_create(&ui->thread, NULL, event_thread_main, ui);

    return (LV2UI_Handle)ui;
}

static void cleanup(LV2UI_Handle handle) {
    GridSeqX11UI* ui = (GridSeqX11UI*)handle;

    ui->running = false;
    if (ui->thread) {
        pthread_join(ui->thread, NULL);
        ui->thread = 0;
    }

    if (ui->port_subscribe) {
        // Unsubscribe from current_step
        ui->port_subscribe->unsubscribe(ui->port_subscribe->handle, 5, 0, NULL);

        // Unsubscribe from grid rows
        for (uint32_t i = 8; i <= 23; i++) {
            ui->port_subscribe->unsubscribe(ui->port_subscribe->handle, i, 0, NULL);
        }

        // Unsubscribe from sequence_length
        ui->port_subscribe->unsubscribe(ui->port_subscribe->handle, 24, 0, NULL);

        // Unsubscribe from midi_filter
        ui->port_subscribe->unsubscribe(ui->port_subscribe->handle, 25, 0, NULL);

        // Unsubscribe from beats_per_bar
        ui->port_subscribe->unsubscribe(ui->port_subscribe->handle, 26, 0, NULL);

        // Unsubscribe from midi_channel
        ui->port_subscribe->unsubscribe(ui->port_subscribe->handle, 27, 0, NULL);
    }

    // Close settings dialog if open
    if (ui->settings_open) {
        close_settings_dialog(ui, false);
    }

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
        cairo_surface_destroy(ui->surface);
        ui->surface = NULL;
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

static void port_event(
    LV2UI_Handle handle,
    uint32_t port_index,
    uint32_t buffer_size,
    uint32_t format,
    const void* buffer
) {
    GridSeqX11UI* ui = (GridSeqX11UI*)handle;
    (void)buffer_size;
    (void)format;

    // Current step (port 5)
    if (port_index == 5 && buffer) {
        uint8_t new_step = (uint8_t)(*(const float*)buffer);
        if (new_step < MAX_GRID_SIZE) {
            pthread_mutex_lock(&ui->mutex);
            ui->state.current_step = new_step;
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
        }
    }

    // Sequence length (port 24)
    if (port_index == 24 && buffer) {
        uint8_t new_length = (uint8_t)(*(const float*)buffer);
        if (new_length >= MIN_SEQUENCE_LENGTH && new_length <= MAX_SEQUENCE_LENGTH) {
            pthread_mutex_lock(&ui->mutex);
            ui->state.sequence_length = new_length;
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
        }
    }

    // MIDI filter (port 25)
    if (port_index == 25 && buffer) {
        float filter_value = *(const float*)buffer;
        pthread_mutex_lock(&ui->mutex);
        ui->midi_filter_enabled = (filter_value > 0.5f);
        pthread_mutex_unlock(&ui->mutex);
    }

    // Beats per bar (port 26)
    if (port_index == 26 && buffer) {
        float beats = *(const float*)buffer;
        if (beats >= MIN_BEATS_PER_BAR && beats <= MAX_BEATS_PER_BAR) {
            pthread_mutex_lock(&ui->mutex);
            ui->state.beats_per_bar = (uint8_t)beats;
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
        }
    }

    // MIDI channel (port 27)
    if (port_index == 27 && buffer) {
        float channel = *(const float*)buffer;
        if (channel >= 1.0f && channel <= 16.0f) {
            pthread_mutex_lock(&ui->mutex);
            ui->midi_channel = (uint8_t)channel;
            if (!ui->settings_open) {
                ui->pending_channel = ui->midi_channel;
            }
            ui->needs_redraw = true;
            pthread_mutex_unlock(&ui->mutex);
        }
    }

    // Grid rows (ports 8-23)
    if (port_index >= 8 && port_index <= 23 && buffer) {
        int x = port_index - 8;
        uint8_t row_value = (uint8_t)(*(const float*)buffer);

        pthread_mutex_lock(&ui->mutex);
        for (int y = 0; y < GRID_VISIBLE_ROWS; y++) {
            ui->state.grid[x][y] = (row_value & (1 << y)) != 0;
        }

        ui->needs_redraw = true;
        pthread_mutex_unlock(&ui->mutex);
    }
}

static int idle(LV2UI_Handle handle) {
    (void)handle;
    return 0;
}

static const LV2UI_Idle_Interface idle_iface = {
    idle
};

static const void* extension_data(const char* uri) {
    if (!strcmp(uri, LV2_UI__idleInterface)) {
        return &idle_iface;
    }
    return NULL;
}

static const LV2UI_Descriptor descriptor = {
    UI_URI,
    instantiate,
    cleanup,
    port_event,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return (index == 0) ? &descriptor : NULL;
}
