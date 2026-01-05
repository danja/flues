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

#include "grid_seq/common.h"
#include "state.h"

#include <lv2/core/lv2.h>
#include <lv2/ui/ui.h>
#include <lv2/urid/urid.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>

#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <gdk/gdkx.h>
#include <cairo/cairo.h>
#include <X11/Xlib.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define UI_URI PLUGIN_URI "#ui"
#define MENU_HEIGHT 30
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT (480 + MENU_HEIGHT)
#define GRID_MARGIN 20
#define GRID_SPACING 2

// Forward declarations
static gboolean on_button_press(GtkWidget* widget, GdkEventButton* event, gpointer data);

typedef struct {
    GtkWidget* window;
    GtkWidget* vbox;
    GtkWidget* menu_bar;
    GtkWidget* drawing_area;
    GtkWidget* settings_dialog;
    GtkWidget* length_scale;
    GtkWidget* length_label;

    cairo_surface_t* surface;

    LV2UI_Write_Function write_function;
    LV2UI_Controller controller;

    GridSeqState state;

    int cell_size;
    uint8_t prev_step;
    float prev_grid_changed;
    uint8_t pending_length;  // Slider value before Apply
    bool needs_redraw;  // Flag to force immediate redraw

    // URIDs for atom messages
    LV2_URID_Map* map;
    LV2_URID gridState;
    LV2_URID atom_eventTransfer;

    // Port subscription
    const LV2UI_Port_Subscribe* port_subscribe;
} GridSeqUI;

// Settings dialog - apply button clicked
static void on_settings_apply(GtkWidget* button, gpointer data) {
    GridSeqUI* ui = (GridSeqUI*)data;
    (void)button;

    // Write new length to plugin port
    float length_value = (float)ui->pending_length;
    ui->write_function(ui->controller, 24, sizeof(float), 0, &length_value);  // PORT_SEQUENCE_LENGTH = 24

    // Close dialog
    gtk_widget_destroy(ui->settings_dialog);
    ui->settings_dialog = NULL;
}

// Settings dialog - cancel button clicked
static void on_settings_cancel(GtkWidget* button, gpointer data) {
    GridSeqUI* ui = (GridSeqUI*)data;
    (void)button;

    // Close dialog without applying
    gtk_widget_destroy(ui->settings_dialog);
    ui->settings_dialog = NULL;
}

// Length slider value changed
static void on_length_changed(GtkRange* range, gpointer data) {
    GridSeqUI* ui = (GridSeqUI*)data;

    ui->pending_length = (uint8_t)gtk_range_get_value(range);

    // Update label
    char label_text[32];
    snprintf(label_text, sizeof(label_text), "Length: %d steps", ui->pending_length);
    gtk_label_set_text(GTK_LABEL(ui->length_label), label_text);
}

// Open settings dialog
static void on_settings_activate(GtkMenuItem* item, gpointer data) {
    GridSeqUI* ui = (GridSeqUI*)data;
    (void)item;

    // Don't open multiple dialogs
    if (ui->settings_dialog) return;

    // Create dialog (no parent window in embedded UI)
    ui->settings_dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(ui->settings_dialog), "Settings");
    gtk_window_set_modal(GTK_WINDOW(ui->settings_dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(ui->settings_dialog), 400, 150);

    // Get content area
    GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(ui->settings_dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content_area), 12);

    // Create vbox for content
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_add(GTK_CONTAINER(content_area), vbox);

    // Label
    ui->length_label = gtk_label_new(NULL);
    char label_text[32];
    snprintf(label_text, sizeof(label_text), "Length: %d steps", ui->state.sequence_length);
    gtk_label_set_text(GTK_LABEL(ui->length_label), label_text);
    gtk_box_pack_start(GTK_BOX(vbox), ui->length_label, FALSE, FALSE, 0);

    // Slider
    ui->length_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                  MIN_SEQUENCE_LENGTH,
                                                  MAX_SEQUENCE_LENGTH,
                                                  1.0);
    gtk_range_set_value(GTK_RANGE(ui->length_scale), ui->state.sequence_length);
    gtk_scale_set_digits(GTK_SCALE(ui->length_scale), 0);
    gtk_scale_set_value_pos(GTK_SCALE(ui->length_scale), GTK_POS_RIGHT);
    gtk_box_pack_start(GTK_BOX(vbox), ui->length_scale, FALSE, FALSE, 0);

    ui->pending_length = ui->state.sequence_length;
    g_signal_connect(ui->length_scale, "value-changed", G_CALLBACK(on_length_changed), ui);

    // Button box
    GtkWidget* button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(button_box), 6);
    gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 0);

    // Cancel button
    GtkWidget* cancel_button = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_settings_cancel), ui);
    gtk_container_add(GTK_CONTAINER(button_box), cancel_button);

    // OK button
    GtkWidget* ok_button = gtk_button_new_with_label("OK");
    g_signal_connect(ok_button, "clicked", G_CALLBACK(on_settings_apply), ui);
    gtk_container_add(GTK_CONTAINER(button_box), ok_button);

    gtk_widget_show_all(ui->settings_dialog);
}

// Mouse button press handler
static gboolean on_button_press(GtkWidget* widget, GdkEventButton* event, gpointer data) {
    GridSeqUI* ui = (GridSeqUI*)data;

    fprintf(stderr, "grid-seq: Button press event received! widget=%p button=%d x=%f y=%f\n",
            (void*)widget, event->button, event->x, event->y);

    if (event->button != 1) {
        fprintf(stderr, "  -> Ignoring non-left button\n");
        return FALSE;  // Only left button
    }

    int mx = (int)event->x;
    int my = (int)event->y;

    // Get widget allocation for button position
    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);

    // Check if settings button was clicked (top-right corner)
    int button_size = 30;
    int settings_x = alloc.width - button_size - 10;
    int settings_y = 10;

    if (mx >= settings_x && mx <= settings_x + button_size &&
        my >= settings_y && my <= settings_y + button_size) {
        // Settings button clicked - open dialog
        on_settings_activate(NULL, ui);
        return TRUE;
    }

    // Check if reset button was clicked (next to settings)
    int reset_x = settings_x - button_size - 5;
    int reset_y = 10;

    if (mx >= reset_x && mx <= reset_x + button_size &&
        my >= reset_y && my <= reset_y + button_size) {
        // Reset button clicked - send hardware reset command
        fprintf(stderr, "grid-seq: Hardware reset button clicked!\n");

        // Send a reset trigger to the plugin via a control port
        // We'll use PORT_GRID_X with a special value (-100) to indicate reset
        float reset_signal = -100.0f;
        ui->write_function(ui->controller, 3, sizeof(float), 0, &reset_signal);

        fprintf(stderr, "grid-seq: Sent hardware reset signal to plugin\n");
        return TRUE;
    }

    // Check if query button was clicked (next to reset)
    int query_x = reset_x - button_size - 5;
    int query_y = 10;

    if (mx >= query_x && mx <= query_x + button_size &&
        my >= query_y && my <= query_y + button_size) {
        // Query button clicked - just send device inquiry
        fprintf(stderr, "grid-seq: Query button clicked!\n");

        // Send a query trigger to the plugin via a control port
        // We'll use PORT_GRID_X with a special value (-200) to indicate query
        float query_signal = -200.0f;
        ui->write_function(ui->controller, 3, sizeof(float), 0, &query_signal);

        fprintf(stderr, "grid-seq: Sent device query signal to plugin\n");
        return TRUE;
    }

    // Check if clear button was clicked (next to query)
    int clear_x = query_x - button_size - 5;
    int clear_y = 10;

    if (mx >= clear_x && mx <= clear_x + button_size &&
        my >= clear_y && my <= clear_y + button_size) {
        // Clear button clicked - clear all grid cells
        fprintf(stderr, "grid-seq: Clear button clicked!\n");

        // Clear local state for immediate visual feedback
        for (int i = 0; i < MAX_GRID_SIZE; i++) {
            for (int j = 0; j < GRID_PITCH_RANGE; j++) {
                ui->state.grid[i][j] = false;
            }
        }
        ui->needs_redraw = true;

        // Send a clear trigger to the plugin via a control port
        // We'll use PORT_GRID_X with a special value (-300) to indicate clear
        float clear_signal = -300.0f;
        ui->write_function(ui->controller, 3, sizeof(float), 0, &clear_signal);

        fprintf(stderr, "grid-seq: Sent clear signal to plugin\n");
        return TRUE;
    }

    // Calculate which grid cell was clicked
    int x = (mx - GRID_MARGIN) / (ui->cell_size + GRID_SPACING);
    int y = (my - GRID_MARGIN) / (ui->cell_size + GRID_SPACING);

    if (x >= 0 && x < ui->state.sequence_length && y >= 0 && y < GRID_VISIBLE_ROWS) {
        // Flip Y coordinate (grid is drawn top to bottom) and map to actual MIDI note
        int grid_y = ui->state.pitch_offset + (GRID_VISIBLE_ROWS - 1 - y);

        // Toggle in local state for immediate visual feedback
        state_toggle_step(&ui->state, x, grid_y);
        ui->needs_redraw = true;  // Force immediate redraw

        // Send to plugin via control ports
        float fx = (float)x;
        float fy = (float)grid_y;
        ui->write_function(ui->controller, 3, sizeof(float), 0, &fx);  // PORT_GRID_X
        ui->write_function(ui->controller, 4, sizeof(float), 0, &fy);  // PORT_GRID_Y

        fprintf(stderr, "grid-seq: GUI toggled cell [%d,%d] (MIDI note %d)\n", x, y, grid_y);
    }

    return TRUE;
}

// GTK3 draw callback
static gboolean on_draw(GtkWidget* widget, cairo_t* cr, gpointer data) {
    GridSeqUI* ui = (GridSeqUI*)data;
    (void)widget;

    if (!cr) {
        fprintf(stderr, "grid-seq: draw callback - no cairo context!\n");
        return FALSE;
    }

    // Get actual widget size
    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);

    static gboolean first_draw = TRUE;
    if (first_draw) {
        fprintf(stderr, "grid-seq: drawing area actual size: %dx%d\n", alloc.width, alloc.height);
        first_draw = FALSE;
    }

    // Clear background
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_paint(cr);

    // Calculate cell size based on actual widget size and sequence length
    int grid_width = alloc.width - 2 * GRID_MARGIN;
    int grid_height = alloc.height - 2 * GRID_MARGIN;
    int num_cols = ui->state.sequence_length;

    // Calculate cell size to fit both dimensions
    int cell_width = (grid_width - (num_cols - 1) * GRID_SPACING) / num_cols;
    int cell_height = (grid_height - (GRID_VISIBLE_ROWS - 1) * GRID_SPACING) / GRID_VISIBLE_ROWS;
    ui->cell_size = (cell_width < cell_height) ? cell_width : cell_height;

    // Draw grid cells - use sequence_length for columns
    for (int x = 0; x < num_cols; x++) {
        for (int y = 0; y < GRID_VISIBLE_ROWS; y++) {
            int px = GRID_MARGIN + x * (ui->cell_size + GRID_SPACING);
            int py = GRID_MARGIN + y * (ui->cell_size + GRID_SPACING);

            // Check if this step is active (flip Y for bottom-to-top display)
            // Map visible row to actual MIDI note using pitch_offset
            uint8_t actual_note = ui->state.pitch_offset + (GRID_VISIBLE_ROWS - 1 - y);
            bool is_active = ui->state.grid[x][actual_note];

            // Check if this is the current step
            bool is_current = (x == ui->state.current_step);

            if (is_active) {
                // Active cell - bright green
                cairo_set_source_rgb(cr, 0.2, 0.8, 0.3);
            } else if (is_current) {
                // Current step but not active - dim highlight
                cairo_set_source_rgb(cr, 0.3, 0.3, 0.4);
            } else {
                // Inactive cell - dark gray
                cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
            }

            cairo_rectangle(cr, px, py, ui->cell_size, ui->cell_size);
            cairo_fill(cr);

            // Highlight current step with border
            if (is_current) {
                cairo_set_source_rgb(cr, 0.8, 0.8, 0.2);
                cairo_set_line_width(cr, 2);
                cairo_rectangle(cr, px, py, ui->cell_size, ui->cell_size);
                cairo_stroke(cr);
            }
        }
    }

    // Draw settings button in top-right corner
    int button_size = 30;
    int settings_x = alloc.width - button_size - 10;
    int settings_y = 10;

    // Settings button background
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_rectangle(cr, settings_x, settings_y, button_size, button_size);
    cairo_fill(cr);

    // Draw reset button (next to settings)
    int reset_x = settings_x - button_size - 5;
    int reset_y = 10;

    // Reset button background (different color - red)
    cairo_set_source_rgb(cr, 0.5, 0.2, 0.2);
    cairo_rectangle(cr, reset_x, reset_y, button_size, button_size);
    cairo_fill(cr);

    // Draw query button (next to reset)
    int query_x = reset_x - button_size - 5;
    int query_y = 10;

    // Query button background (different color - blue)
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.5);
    cairo_rectangle(cr, query_x, query_y, button_size, button_size);
    cairo_fill(cr);

    // Draw gear icon on settings button
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_set_line_width(cr, 2);
    int settings_center_x = settings_x + button_size / 2;
    int settings_center_y = settings_y + button_size / 2;
    cairo_arc(cr, settings_center_x, settings_center_y, 8, 0, 2 * M_PI);
    cairo_stroke(cr);

    // Draw "R" text on reset button
    cairo_set_source_rgb(cr, 1.0, 0.6, 0.6);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 18);
    int reset_center_x = reset_x + button_size / 2;
    int reset_center_y = reset_y + button_size / 2;
    cairo_move_to(cr, reset_center_x - 6, reset_center_y + 6);
    cairo_show_text(cr, "R");

    // Draw "?" text on query button
    cairo_set_source_rgb(cr, 0.6, 0.6, 1.0);
    int query_center_x = query_x + button_size / 2;
    int query_center_y = query_y + button_size / 2;
    cairo_move_to(cr, query_center_x - 5, query_center_y + 6);
    cairo_show_text(cr, "?");

    // Draw clear button (next to query)
    int clear_x = query_x - button_size - 5;
    int clear_y = 10;

    // Clear button background (orange/yellow)
    cairo_set_source_rgb(cr, 0.6, 0.4, 0.1);
    cairo_rectangle(cr, clear_x, clear_y, button_size, button_size);
    cairo_fill(cr);

    // Draw "C" text on clear button
    cairo_set_source_rgb(cr, 1.0, 0.8, 0.4);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 18);
    int clear_center_x = clear_x + button_size / 2;
    int clear_center_y = clear_y + button_size / 2;
    cairo_move_to(cr, clear_center_x - 6, clear_center_y + 6);
    cairo_show_text(cr, "C");

    // Draw pitch range indicator in top-left
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 14);
    char pitch_text[32];
    snprintf(pitch_text, sizeof(pitch_text), "MIDI: %d-%d",
             ui->state.pitch_offset,
             ui->state.pitch_offset + GRID_VISIBLE_ROWS - 1);
    cairo_move_to(cr, GRID_MARGIN, GRID_MARGIN - 5);
    cairo_show_text(cr, pitch_text);

    return FALSE;
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

    GridSeqUI* ui = (GridSeqUI*)calloc(1, sizeof(GridSeqUI));
    if (!ui) return NULL;

    ui->write_function = write_function;
    ui->controller = controller;

    // Get required features
    ui->map = NULL;
    ui->port_subscribe = NULL;
    void* parent = NULL;

    for (int i = 0; features[i]; i++) {
        if (strcmp(features[i]->URI, LV2_URID__map) == 0) {
            ui->map = (LV2_URID_Map*)features[i]->data;
        } else if (strcmp(features[i]->URI, LV2_UI__portSubscribe) == 0) {
            ui->port_subscribe = (const LV2UI_Port_Subscribe*)features[i]->data;
        } else if (strcmp(features[i]->URI, LV2_UI__parent) == 0) {
            parent = features[i]->data;
            fprintf(stderr, "grid-seq: Got parent window from host: %p\n", parent);
        }
    }

    if (!ui->map) {
        free(ui);
        return NULL;
    }

    if (!parent) {
        fprintf(stderr, "grid-seq: WARNING - No parent window provided by host\n");
    }

    // Map URIDs
    ui->gridState = ui->map->map(ui->map->handle, GRID_SEQ__gridState);
    ui->atom_eventTransfer = ui->map->map(ui->map->handle, LV2_ATOM__eventTransfer);

    // Initialize state
    state_init(&ui->state, 48000.0);

    ui->prev_step = 0;
    ui->prev_grid_changed = 0.0f;
    ui->settings_dialog = NULL;
    ui->needs_redraw = true;

    // Create window - X11UI uses toplevel window that host will reparent
    // Don't use GtkPlug - it expects a GtkSocket which Reaper doesn't provide
    ui->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ui->window), "Grid Sequencer");
    gtk_window_set_default_size(GTK_WINDOW(ui->window), WINDOW_WIDTH, WINDOW_HEIGHT);
    gtk_window_set_resizable(GTK_WINDOW(ui->window), FALSE);

    fprintf(stderr, "grid-seq: Created toplevel window (parent=%p will reparent)\n", parent);

    // Don't accept keyboard focus to avoid blocking host shortcuts
    gtk_window_set_accept_focus(GTK_WINDOW(ui->window), FALSE);

    // Create vbox as main container
    ui->vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(ui->window), ui->vbox);

    // GTK menu bar doesn't render in X11 embedding
    // TODO: Consider migrating to robtk for proper widget support
    // For now, draw settings button with Cairo
    ui->menu_bar = NULL;

    // Create an event box to capture mouse events (drawing areas don't receive events when embedded)
    GtkWidget* event_box = gtk_event_box_new();
    gtk_box_pack_start(GTK_BOX(ui->vbox), event_box, TRUE, TRUE, 0);

    // Create drawing area for grid inside the event box
    ui->drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(ui->drawing_area, WINDOW_WIDTH, WINDOW_HEIGHT);
    gtk_container_add(GTK_CONTAINER(event_box), ui->drawing_area);

    // Enable button press events on the event box
    gtk_widget_add_events(event_box, GDK_BUTTON_PRESS_MASK);

    // Connect drawing signal to drawing area (for rendering)
    fprintf(stderr, "grid-seq: connecting draw signal to drawing_area %p\n", (void*)ui->drawing_area);
    g_signal_connect(ui->drawing_area, "draw", G_CALLBACK(on_draw), ui);

    // Connect button-press to event box (for mouse clicks)
    // Note: This may not work when embedded in Reaper - we poll X11 events directly in idle()
    g_signal_connect(event_box, "button-press-event", G_CALLBACK(on_button_press), ui);
    fprintf(stderr, "grid-seq: connected button-press-event to event_box\n");

    // Also try connecting to the window itself
    g_signal_connect(ui->window, "button-press-event", G_CALLBACK(on_button_press), ui);
    gtk_widget_add_events(ui->window, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

    // Subscribe to ports
    if (ui->port_subscribe) {
        // Subscribe to grid row ports (0-15)
        for (uint32_t i = 8; i <= 23; i++) {
            ui->port_subscribe->subscribe(
                ui->port_subscribe->handle,
                i,
                0,  // Control port protocol
                NULL
            );
        }

        // Subscribe to sequence_length port
        ui->port_subscribe->subscribe(
            ui->port_subscribe->handle,
            24,  // PORT_SEQUENCE_LENGTH
            0,   // Control port protocol
            NULL
        );
    }

    // Realize and show the window
    gtk_widget_realize(ui->window);
    gtk_widget_show_all(ui->window);

    // Get X11 window ID
    GdkWindow* gdk_window = gtk_widget_get_window(ui->window);
    if (gdk_window) {
        Window xid = gdk_x11_window_get_xid(gdk_window);
        *widget = (LV2UI_Widget)(uintptr_t)xid;

        // Enable X11 button press events directly
        // Note: Reaper will reparent this window, so we need to re-enable events after embedding
        Display* display = GDK_WINDOW_XDISPLAY(gdk_window);

        // Request ALL events including substructure notify (for reparenting detection)
        XSelectInput(display, xid,
                    ButtonPressMask | ButtonReleaseMask |
                    PointerMotionMask |
                    SubstructureNotifyMask);

        fprintf(stderr, "grid-seq: Enabled X11 events on window 0x%lx\n", xid);

        // Check if window is actually visible
        GdkWindowState state = gdk_window_get_state(gdk_window);
        gboolean is_viewable = gdk_window_is_viewable(gdk_window);
        gboolean is_visible = gdk_window_is_visible(gdk_window);

        fprintf(stderr, "grid-seq: Returning X11 window ID: 0x%lx (viewable=%d, visible=%d, state=%d)\n",
                xid, is_viewable, is_visible, state);
    } else {
        fprintf(stderr, "grid-seq: ERROR - failed to get GdkWindow!\n");
        *widget = 0;
    }

    fprintf(stderr, "grid-seq: UI instantiated, drawing area realized=%d\n",
            gtk_widget_get_realized(ui->drawing_area));

    return (LV2UI_Handle)ui;
}

static void cleanup(LV2UI_Handle handle) {
    GridSeqUI* ui = (GridSeqUI*)handle;

    // Unsubscribe from ports
    if (ui->port_subscribe) {
        // Unsubscribe from grid row ports (8-23)
        for (uint32_t i = 8; i <= 23; i++) {
            ui->port_subscribe->unsubscribe(
                ui->port_subscribe->handle,
                i,
                0,  // Control port protocol
                NULL
            );
        }

        // Unsubscribe from sequence_length port
        ui->port_subscribe->unsubscribe(
            ui->port_subscribe->handle,
            24,
            0,
            NULL
        );
    }

    // Destroy window and all child widgets
    if (ui->window) {
        gtk_widget_destroy(ui->window);
    }

    free(ui);
}

static void port_event(
    LV2UI_Handle handle,
    uint32_t port_index,
    uint32_t buffer_size,
    uint32_t format,
    const void* buffer
) {
    GridSeqUI* ui = (GridSeqUI*)handle;

    (void)buffer_size;
    (void)format;

    // Handle current step updates from plugin
    if (port_index == 5 && buffer) {  // PORT_CURRENT_STEP
        float step = *(const float*)buffer;
        if (step >= 0 && step < MAX_GRID_SIZE) {
            ui->state.current_step = (uint8_t)step;
            ui->needs_redraw = true;  // Force immediate redraw
        }
    }

    // Handle sequence length changes
    if (port_index == 24 && buffer) {  // PORT_SEQUENCE_LENGTH
        float length = *(const float*)buffer;
        uint8_t new_length = (uint8_t)length;
        if (new_length >= MIN_SEQUENCE_LENGTH && new_length <= MAX_SEQUENCE_LENGTH) {
            ui->state.sequence_length = new_length;
            ui->needs_redraw = true;  // Force immediate redraw
        }
    }

    // Handle grid row ports (8-23) - all 16 rows
    if (port_index >= 8 && port_index <= 23 && buffer) {
        int x = port_index - 8;
        uint8_t row_value = (uint8_t)(*(const float*)buffer);

        // Unpack bits into grid
        for (int y = 0; y < GRID_VISIBLE_ROWS; y++) {
            ui->state.grid[x][y] = (row_value & (1 << y)) != 0;
        }

        ui->needs_redraw = true;  // Force immediate redraw
    }
}

static int idle(LV2UI_Handle handle) {
    GridSeqUI* ui = (GridSeqUI*)handle;

    // Process pending GTK events
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }

    // Poll X11 events directly like PUGL (GTK events blocked by Reaper embedding)
    if (ui->window && gtk_widget_get_realized(ui->window)) {
        GdkWindow* gdk_window = gtk_widget_get_window(ui->window);
        if (gdk_window) {
            Display* display = GDK_WINDOW_XDISPLAY(gdk_window);
            Window xwindow = GDK_WINDOW_XID(gdk_window);

            // Flush and sync to ensure we see all events
            XFlush(display);

            XEvent xevent;

            // Debug: Log if ANY events are pending (only first 10 times)
            static int event_check_count = 0;
            int pending = XPending(display);
            if (event_check_count < 10) {
                if (pending > 0 || event_check_count == 0) {
                    fprintf(stderr, "grid-seq: XPending=%d events for display (check #%d)\n",
                            pending, event_check_count);
                }
                event_check_count++;
            }

            // Poll all pending events like PUGL does
            while (XPending(display) > 0) {
                XNextEvent(display, &xevent);

                // Debug: Log all events for our window (first 20)
                static int our_event_count = 0;
                if (xevent.xany.window == xwindow && our_event_count < 20) {
                    fprintf(stderr, "grid-seq: X11 event type=%d for our window 0x%lx\n",
                            xevent.type, xwindow);
                    our_event_count++;
                }

                // Only process events for our window
                if (xevent.xany.window != xwindow) {
                    continue;
                }

                if (xevent.type == ButtonPress) {
                    fprintf(stderr, "grid-seq: X11 ButtonPress detected at (%d, %d) button=%d\n",
                            xevent.xbutton.x, xevent.xbutton.y, xevent.xbutton.button);

                    // Create a GdkEventButton and call our handler
                    GdkEventButton event;
                    event.type = GDK_BUTTON_PRESS;
                    event.button = xevent.xbutton.button;
                    event.x = xevent.xbutton.x;
                    event.y = xevent.xbutton.y;

                    on_button_press(ui->drawing_area, &event, ui);
                }
            }
        }
    }

    // Redraw when state changes or at 30fps
    static int frame_count = 0;
    static bool first_idle = true;
    frame_count++;

    bool should_draw = ui->needs_redraw || (frame_count % 30 == 0);

    if (should_draw) {
        // Force redraw of entire window (including menu) on first idle
        if (first_idle && ui->window && gtk_widget_get_realized(ui->window)) {
            GdkWindow* window_gdk = gtk_widget_get_window(ui->window);
            if (window_gdk) {
                gdk_window_invalidate_rect(window_gdk, NULL, TRUE);
            }
            first_idle = false;
        }

        // Draw the grid
        if (ui->drawing_area && gtk_widget_get_realized(ui->drawing_area)) {
            GdkWindow* gdk_win = gtk_widget_get_window(ui->drawing_area);
            if (gdk_win) {
                cairo_t* cr = gdk_cairo_create(gdk_win);
                if (cr) {
                    on_draw(ui->drawing_area, cr, ui);
                    cairo_destroy(cr);
                    ui->needs_redraw = false;  // Clear flag after drawing
                }
            }
        }
    }

    return 0;
}

static int show(LV2UI_Handle handle) {
    GridSeqUI* ui = (GridSeqUI*)handle;
    fprintf(stderr, "grid-seq: show() called\n");

    gtk_widget_show_all(ui->window);

    // Force a redraw after showing
    gtk_widget_queue_draw(ui->drawing_area);

    // Process pending events
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }

    return 0;
}

static int hide(LV2UI_Handle handle) {
    GridSeqUI* ui = (GridSeqUI*)handle;
    gtk_widget_hide(ui->window);
    return 0;
}

static const void* extension_data(const char* uri) {
    static const LV2UI_Idle_Interface idle_interface = { idle };
    static const LV2UI_Show_Interface show_interface = { show, hide };

    if (strcmp(uri, LV2_UI__idleInterface) == 0) {
        return &idle_interface;
    }
    if (strcmp(uri, LV2_UI__showInterface) == 0) {
        return &show_interface;
    }

    return NULL;
}

static const LV2UI_Descriptor descriptor = {
    .URI = UI_URI,
    .instantiate = instantiate,
    .cleanup = cleanup,
    .port_event = port_event,
    .extension_data = extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return (index == 0) ? &descriptor : NULL;
}
