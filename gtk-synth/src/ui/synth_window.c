// synth_window.c
// Main GTK4 synthesizer window with controls
// Physical modeling synthesizer UI

#include "pm_synth.h"
#include "audio_backend.h"
#include <gtk/gtk.h>
#include <stdio.h>

typedef struct {
    GtkApplication *app;
    GtkWidget *window;
    PMSynthEngine *synth;
    AudioBackend *audio;

    // Control widgets
    GtkWidget *interface_selector;
    GtkScale *dc_scale;
    GtkScale *noise_scale;
    GtkScale *tone_scale;
    GtkScale *attack_scale;
    GtkScale *release_scale;
    GtkScale *intensity_scale;
    GtkScale *tuning_scale;
    GtkScale *ratio_scale;
    GtkScale *delay1_fb_scale;
    GtkScale *delay2_fb_scale;
    GtkScale *filter_fb_scale;
    GtkScale *filter_freq_scale;
    GtkScale *filter_q_scale;
    GtkScale *filter_shape_scale;
    GtkScale *lfo_freq_scale;
    GtkScale *mod_depth_scale;
    GtkScale *reverb_size_scale;
    GtkScale *reverb_level_scale;

    bool audio_running;
    int current_note;
} SynthWindow;

// Audio callback
static void audio_process_callback(float* output, int num_samples, void* user_data) {
    SynthWindow* win = (SynthWindow*)user_data;
    pm_synth_process(win->synth, output, num_samples);
}

// Control callbacks
static void on_interface_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    guint selected = gtk_drop_down_get_selected(dropdown);
    pm_synth_set_interface_type(win->synth, (InterfaceType)selected);
    printf("Interface changed to: %s\n", pm_synth_interface_name((InterfaceType)selected));
    (void)pspec; // Unused
}

static void on_dc_changed(GtkRange *range, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_set_dc_level(win->synth, gtk_range_get_value(range));
}

static void on_noise_changed(GtkRange *range, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_set_noise_level(win->synth, gtk_range_get_value(range));
}

static void on_attack_changed(GtkRange *range, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_set_attack(win->synth, gtk_range_get_value(range));
}

static void on_release_changed(GtkRange *range, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_set_release(win->synth, gtk_range_get_value(range));
}

static void on_intensity_changed(GtkRange *range, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_set_interface_intensity(win->synth, gtk_range_get_value(range));
}

static void on_delay1_fb_changed(GtkRange *range, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_set_delay1_feedback(win->synth, gtk_range_get_value(range));
}

static void on_delay2_fb_changed(GtkRange *range, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_set_delay2_feedback(win->synth, gtk_range_get_value(range));
}

static void on_tone_changed(GtkRange *range, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_set_tone_level(win->synth, gtk_range_get_value(range));
}

static void on_tuning_changed(GtkRange *range, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_set_tuning(win->synth, gtk_range_get_value(range));
}

static void on_ratio_changed(GtkRange *range, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_set_ratio(win->synth, gtk_range_get_value(range));
}

static void on_filter_fb_changed(GtkRange *range, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_set_filter_feedback(win->synth, gtk_range_get_value(range));
}

static void on_filter_freq_changed(GtkRange *range, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_set_filter_frequency(win->synth, gtk_range_get_value(range));
}

static void on_filter_q_changed(GtkRange *range, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_set_filter_q(win->synth, gtk_range_get_value(range));
}

static void on_filter_shape_changed(GtkRange *range, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_set_filter_shape(win->synth, gtk_range_get_value(range));
}

static void on_lfo_freq_changed(GtkRange *range, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_set_lfo_frequency(win->synth, gtk_range_get_value(range));
}

static void on_mod_depth_changed(GtkRange *range, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_set_modulation_depth(win->synth, gtk_range_get_value(range));
}

static void on_reverb_size_changed(GtkRange *range, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_set_reverb_size(win->synth, gtk_range_get_value(range));
}

static void on_reverb_level_changed(GtkRange *range, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_set_reverb_level(win->synth, gtk_range_get_value(range));
}

// Keyboard event handler
static gboolean on_key_pressed(GtkEventControllerKey *controller,
                               guint keyval, guint keycode,
                               GdkModifierType state, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;

    if (!win->audio_running) return FALSE;

    // Simple keyboard mapping (A-K = C4-C5)
    int note = -1;
    switch (keyval) {
        case GDK_KEY_a: note = 60; break; // C4
        case GDK_KEY_w: note = 61; break; // C#4
        case GDK_KEY_s: note = 62; break; // D4
        case GDK_KEY_e: note = 63; break; // D#4
        case GDK_KEY_d: note = 64; break; // E4
        case GDK_KEY_f: note = 65; break; // F4
        case GDK_KEY_t: note = 66; break; // F#4
        case GDK_KEY_g: note = 67; break; // G4
        case GDK_KEY_y: note = 68; break; // G#4
        case GDK_KEY_h: note = 69; break; // A4
        case GDK_KEY_u: note = 70; break; // A#4
        case GDK_KEY_j: note = 71; break; // B4
        case GDK_KEY_k: note = 72; break; // C5
    }

    if (note >= 0) {
        win->current_note = note;
        float freq = pm_synth_midi_to_frequency(note);
        pm_synth_note_on(win->synth, freq);
        return TRUE;
    }

    return FALSE;
}

static gboolean on_key_released(GtkEventControllerKey *controller,
                                guint keyval, guint keycode,
                                GdkModifierType state, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;
    pm_synth_note_off(win->synth);
    return TRUE;
}

// Callback to update value label when slider changes
static void update_value_label(GtkRange *range, gpointer user_data) {
    GtkWidget *label = g_object_get_data(G_OBJECT(range), "value-label");
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f", gtk_range_get_value(range));
    gtk_label_set_text(GTK_LABEL(label), buf);
    (void)user_data; // Unused
}

// Create a vertical knob (slider) with label and value display
static GtkWidget* create_knob(const char *label, double min, double max,
                              double value, GCallback callback, gpointer data) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_set_size_request(box, 70, 140);

    // Label at top
    GtkWidget *name_label = gtk_label_new(label);
    gtk_widget_set_halign(name_label, GTK_ALIGN_CENTER);

    // Vertical slider (inverted so top = max)
    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, min, max, 1.0);
    gtk_range_set_value(GTK_RANGE(scale), value);
    gtk_range_set_inverted(GTK_RANGE(scale), TRUE);
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_widget_set_vexpand(scale, TRUE);
    g_signal_connect(scale, "value-changed", callback, data);

    // Value display at bottom
    GtkWidget *value_label = gtk_label_new("0");
    gtk_widget_set_halign(value_label, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(value_label, 50, 20);

    // Update value label when slider changes
    g_object_set_data(G_OBJECT(scale), "value-label", value_label);
    g_signal_connect(scale, "value-changed", G_CALLBACK(update_value_label), NULL);

    // Set initial value display
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f", value);
    gtk_label_set_text(GTK_LABEL(value_label), buf);

    gtk_box_append(GTK_BOX(box), name_label);
    gtk_box_append(GTK_BOX(box), scale);
    gtk_box_append(GTK_BOX(box), value_label);

    return box;
}

// Create a module frame
static GtkWidget* create_module_frame(const char *title) {
    GtkWidget *frame = gtk_frame_new(title);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_frame_set_child(GTK_FRAME(frame), box);
    return frame;
}

static void activate(GtkApplication *app, gpointer user_data) {
    SynthWindow *win = (SynthWindow*)user_data;

    win->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win->window), "PM Synth - Physical Modeling Synthesizer");
    gtk_window_set_default_size(GTK_WINDOW(win->window), 1300, 720);

    // Main container
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(main_vbox, 10);
    gtk_widget_set_margin_end(main_vbox, 10);
    gtk_widget_set_margin_top(main_vbox, 10);
    gtk_widget_set_margin_bottom(main_vbox, 10);

    // === TOP BAR ===
    GtkWidget *top_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);

    // Interface selector section
    GtkWidget *interface_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    GtkWidget *interface_label = gtk_label_new("INTERFACE TYPE");
    const char *interface_names[] = {
        "Pluck", "Hit", "Reed", "Flute", "Brass", "Bow", "Bell", "Drum",
        "Crystal", "Vapor", "Quantum", "Plasma", NULL
    };
    win->interface_selector = gtk_drop_down_new_from_strings(interface_names);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(win->interface_selector), 2); // Reed default
    gtk_widget_set_size_request(win->interface_selector, 140, 50);
    g_signal_connect(win->interface_selector, "notify::selected",
                    G_CALLBACK(on_interface_changed), win);
    gtk_box_append(GTK_BOX(interface_vbox), interface_label);
    gtk_box_append(GTK_BOX(interface_vbox), win->interface_selector);
    gtk_box_append(GTK_BOX(top_bar), interface_vbox);

    // Info text
    GtkWidget *info_label = gtk_label_new(
        "🎹 Keyboard: A W S E D F T G Y H U J K  (C4-C5)\n"
        "Physical Modeling · 8 DSP Modules · 12 Interface Types");
    gtk_widget_set_halign(info_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(top_bar), info_label);

    gtk_box_append(GTK_BOX(main_vbox), top_bar);

    // === MODULE ROWS ===

    // Row 1: SOURCES, ENVELOPE, INTERFACE
    GtkWidget *row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    // SOURCES module
    GtkWidget *sources_frame = create_module_frame("SOURCES");
    GtkWidget *sources_box = gtk_frame_get_child(GTK_FRAME(sources_frame));
    gtk_box_append(GTK_BOX(sources_box), create_knob("DC", 0, 100, 0, G_CALLBACK(on_dc_changed), win));
    gtk_box_append(GTK_BOX(sources_box), create_knob("Noise", 0, 100, 10, G_CALLBACK(on_noise_changed), win));
    gtk_box_append(GTK_BOX(sources_box), create_knob("Tone", 0, 100, 0, G_CALLBACK(on_tone_changed), win));
    gtk_box_append(GTK_BOX(row1), sources_frame);

    // ENVELOPE module
    GtkWidget *envelope_frame = create_module_frame("ENVELOPE");
    GtkWidget *envelope_box = gtk_frame_get_child(GTK_FRAME(envelope_frame));
    gtk_box_append(GTK_BOX(envelope_box), create_knob("Attack", 0, 100, 10, G_CALLBACK(on_attack_changed), win));
    gtk_box_append(GTK_BOX(envelope_box), create_knob("Release", 0, 100, 50, G_CALLBACK(on_release_changed), win));
    gtk_box_append(GTK_BOX(row1), envelope_frame);

    // INTERFACE module
    GtkWidget *interface_frame = create_module_frame("INTERFACE");
    GtkWidget *interface_box = gtk_frame_get_child(GTK_FRAME(interface_frame));
    gtk_box_append(GTK_BOX(interface_box), create_knob("Intensity", 0, 100, 50, G_CALLBACK(on_intensity_changed), win));
    gtk_box_append(GTK_BOX(row1), interface_frame);

    gtk_box_append(GTK_BOX(main_vbox), row1);

    // Row 2: DELAY LINES, FEEDBACK
    GtkWidget *row2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    // DELAY LINES module
    GtkWidget *delay_frame = create_module_frame("DELAY LINES");
    GtkWidget *delay_box = gtk_frame_get_child(GTK_FRAME(delay_frame));
    gtk_box_append(GTK_BOX(delay_box), create_knob("Tuning", 0, 100, 50, G_CALLBACK(on_tuning_changed), win));
    gtk_box_append(GTK_BOX(delay_box), create_knob("Ratio", 0, 100, 50, G_CALLBACK(on_ratio_changed), win));
    gtk_box_append(GTK_BOX(row2), delay_frame);

    // FEEDBACK module
    GtkWidget *feedback_frame = create_module_frame("FEEDBACK");
    GtkWidget *feedback_box = gtk_frame_get_child(GTK_FRAME(feedback_frame));
    gtk_box_append(GTK_BOX(feedback_box), create_knob("Delay 1", 0, 100, 95, G_CALLBACK(on_delay1_fb_changed), win));
    gtk_box_append(GTK_BOX(feedback_box), create_knob("Delay 2", 0, 100, 95, G_CALLBACK(on_delay2_fb_changed), win));
    gtk_box_append(GTK_BOX(feedback_box), create_knob("Filter", 0, 100, 0, G_CALLBACK(on_filter_fb_changed), win));
    gtk_box_append(GTK_BOX(row2), feedback_frame);

    gtk_box_append(GTK_BOX(main_vbox), row2);

    // Row 3: FILTER, MODULATION, REVERB
    GtkWidget *row3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    // FILTER module
    GtkWidget *filter_frame = create_module_frame("FILTER");
    GtkWidget *filter_box = gtk_frame_get_child(GTK_FRAME(filter_frame));
    gtk_box_append(GTK_BOX(filter_box), create_knob("Freq", 0, 100, 70, G_CALLBACK(on_filter_freq_changed), win));
    gtk_box_append(GTK_BOX(filter_box), create_knob("Q", 0, 100, 20, G_CALLBACK(on_filter_q_changed), win));
    gtk_box_append(GTK_BOX(filter_box), create_knob("Shape", 0, 100, 0, G_CALLBACK(on_filter_shape_changed), win));
    gtk_box_append(GTK_BOX(row3), filter_frame);

    // MODULATION module
    GtkWidget *mod_frame = create_module_frame("MODULATION");
    GtkWidget *mod_box = gtk_frame_get_child(GTK_FRAME(mod_frame));
    gtk_box_append(GTK_BOX(mod_box), create_knob("LFO Freq", 0, 100, 30, G_CALLBACK(on_lfo_freq_changed), win));
    gtk_box_append(GTK_BOX(mod_box), create_knob("Depth", 0, 100, 50, G_CALLBACK(on_mod_depth_changed), win));
    gtk_box_append(GTK_BOX(row3), mod_frame);

    // REVERB module
    GtkWidget *reverb_frame = create_module_frame("REVERB");
    GtkWidget *reverb_box = gtk_frame_get_child(GTK_FRAME(reverb_frame));
    gtk_box_append(GTK_BOX(reverb_box), create_knob("Size", 0, 100, 50, G_CALLBACK(on_reverb_size_changed), win));
    gtk_box_append(GTK_BOX(reverb_box), create_knob("Level", 0, 100, 30, G_CALLBACK(on_reverb_level_changed), win));
    gtk_box_append(GTK_BOX(row3), reverb_frame);

    gtk_box_append(GTK_BOX(main_vbox), row3);

    gtk_window_set_child(GTK_WINDOW(win->window), main_vbox);

    // Add keyboard controller
    GtkEventController *key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_key_pressed), win);
    g_signal_connect(key_controller, "key-released", G_CALLBACK(on_key_released), win);
    gtk_widget_add_controller(win->window, key_controller);

    // Start audio automatically
    if (audio_backend_start(win->audio)) {
        win->audio_running = true;
        printf("Audio started successfully\n");
    } else {
        fprintf(stderr, "Failed to start audio\n");
    }

    gtk_window_present(GTK_WINDOW(win->window));
}

int main(int argc, char **argv) {
    SynthWindow win = {0};

    // Create synthesizer engine
    win.synth = pm_synth_create(DEFAULT_SAMPLE_RATE);
    if (!win.synth) {
        fprintf(stderr, "Failed to create synth engine\n");
        return 1;
    }

    // Create audio backend (PulseAudio)
    win.audio = audio_backend_create(AUDIO_BACKEND_PULSEAUDIO,
                                     DEFAULT_SAMPLE_RATE, DEFAULT_BUFFER_SIZE,
                                     audio_process_callback, &win);
    if (!win.audio) {
        fprintf(stderr, "Failed to create audio backend\n");
        pm_synth_destroy(win.synth);
        return 1;
    }

    // Create GTK application
    win.app = gtk_application_new("org.flues.pmsynth", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(win.app, "activate", G_CALLBACK(activate), &win);

    int status = g_application_run(G_APPLICATION(win.app), argc, argv);

    // Cleanup
    audio_backend_destroy(win.audio);
    pm_synth_destroy(win.synth);
    g_object_unref(win.app);

    return status;
}
