#include <gtk/gtk.h>
#include <lv2/ui/ui.h>
#include <string.h>

#include "FluesKnob.h"

#define PMSYNTH_URI "https://danja.github.io/flues/plugins/pm-synth"
#define PMSYNTH_UI_URI PMSYNTH_URI "#ui"

typedef enum {
    PORT_AUDIO_OUT = 0,
    PORT_MIDI_IN,
    PORT_DC_LEVEL,
    PORT_NOISE_LEVEL,
    PORT_TONE_LEVEL,
    PORT_ATTACK,
    PORT_RELEASE,
    PORT_INTERFACE_TYPE,
    PORT_INTERFACE_INTENSITY,
    PORT_TUNING,
    PORT_RATIO,
    PORT_DELAY1_FEEDBACK,
    PORT_DELAY2_FEEDBACK,
    PORT_FILTER_FEEDBACK,
    PORT_FILTER_FREQUENCY,
    PORT_FILTER_Q,
    PORT_FILTER_SHAPE,
    PORT_LFO_FREQUENCY,
    PORT_MOD_TYPE_LEVEL,
    PORT_REVERB_SIZE,
    PORT_REVERB_LEVEL,
    PORT_TOTAL_COUNT
} PortIndex;

typedef enum {
    GROUP_STEAM = 0,
    GROUP_INTERFACE,
    GROUP_ENVELOPE,
    GROUP_PIPE,
    GROUP_FILTER,
    GROUP_MODULATION,
    GROUP_REVERB,
    GROUP_COUNT
} GroupIndex;

typedef struct {
    GroupIndex group;
    const gchar* label;
    guint port;
    float min;
    float max;
    float def;
    guint steps;
} ControlDesc;

static const struct {
    const gchar* title;
    guint columns;
} kGroupInfo[GROUP_COUNT] = {
    [GROUP_STEAM] = { "Steam", 3 },
    [GROUP_INTERFACE] = { "Interface", 2 },
    [GROUP_ENVELOPE] = { "Envelope", 2 },
    [GROUP_PIPE] = { "Pipe & Delay", 4 },
    [GROUP_FILTER] = { "Feedback & Filter", 4 },
    [GROUP_MODULATION] = { "Modulation", 2 },
    [GROUP_REVERB] = { "Reverb", 2 },
};

static const ControlDesc kControlInfo[] = {
    { GROUP_STEAM, "DC LEVEL", PORT_DC_LEVEL, 0.0f, 1.0f, 0.5f, 0 },
    { GROUP_STEAM, "NOISE", PORT_NOISE_LEVEL, 0.0f, 1.0f, 0.15f, 0 },
    { GROUP_STEAM, "TONE", PORT_TONE_LEVEL, 0.0f, 1.0f, 0.0f, 0 },

    { GROUP_ENVELOPE, "ATTACK", PORT_ATTACK, 0.0f, 1.0f, 0.33333334f, 0 },
    { GROUP_ENVELOPE, "RELEASE", PORT_RELEASE, 0.0f, 1.0f, 0.2821703f, 0 },

    { GROUP_INTERFACE, "TYPE", PORT_INTERFACE_TYPE, 0.0f, 11.0f, 2.0f, 12 },
    { GROUP_INTERFACE, "INTENSITY", PORT_INTERFACE_INTENSITY, 0.0f, 1.0f, 0.5f, 0 },

    { GROUP_PIPE, "TUNING", PORT_TUNING, 0.0f, 1.0f, 0.5f, 0 },
    { GROUP_PIPE, "RATIO", PORT_RATIO, 0.0f, 1.0f, 0.5f, 0 },
    { GROUP_PIPE, "DELAY 1 FB", PORT_DELAY1_FEEDBACK, 0.0f, 1.0f, 0.95959598f, 0 },
    { GROUP_PIPE, "DELAY 2 FB", PORT_DELAY2_FEEDBACK, 0.0f, 1.0f, 0.95959598f, 0 },

    { GROUP_FILTER, "FILTER FB", PORT_FILTER_FEEDBACK, 0.0f, 1.0f, 0.0f, 0 },
    { GROUP_FILTER, "FILTER FREQ", PORT_FILTER_FREQUENCY, 0.0f, 1.0f, 0.56632334f, 0 },
    { GROUP_FILTER, "FILTER Q", PORT_FILTER_Q, 0.0f, 1.0f, 0.18790182f, 0 },
    { GROUP_FILTER, "FILTER SHAPE", PORT_FILTER_SHAPE, 0.0f, 1.0f, 0.0f, 0 },

    { GROUP_MODULATION, "LFO RATE", PORT_LFO_FREQUENCY, 0.0f, 1.0f, 0.73835194f, 0 },
    { GROUP_MODULATION, "AM ↔ FM", PORT_MOD_TYPE_LEVEL, 0.0f, 1.0f, 0.5f, 0 },

    { GROUP_REVERB, "SIZE", PORT_REVERB_SIZE, 0.0f, 1.0f, 0.5f, 0 },
    { GROUP_REVERB, "LEVEL", PORT_REVERB_LEVEL, 0.0f, 1.0f, 0.3f, 0 },
};

typedef struct {
    LV2UI_Write_Function write;
    LV2UI_Controller controller;
    GtkWidget* container;
    FluesKnob* knobs[PORT_TOTAL_COUNT];
} PMSynthUI;

static void apply_css(GtkWidget* root) {
    static const gchar* css =
        ".flues-root {"
        "  background-color: #13161c;"
        "  padding: 16px;"
        "}"
        ".flues-group {"
        "  border: 1px solid #3b3f48;"
        "  border-radius: 6px;"
        "  background-image: linear-gradient(180deg, rgba(24,27,33,0.95), rgba(17,19,23,0.95));"
        "}"
        ".flues-group > label {"
        "  color: #f2d6a2;"
        "  font-weight: 600;"
        "  padding: 0px 8px;"
        "  text-transform: uppercase;"
        "  font-size: 11px;"
        "}"
        ".flues-group-inner {"
        "  margin: 12px;"
        "}"
        ".flues-frame-label {"
        "  color: #f0c364;"
        "}"
        ".flues-grid-spacing {"
        "  margin-bottom: 12px;"
        "}"
        ;

    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css, -1, NULL);

    GtkStyleContext* context = gtk_widget_get_style_context(root);
    gtk_style_context_add_class(context, "flues-root");

    GdkScreen* screen = gdk_screen_get_default();
    if (screen) {
        gtk_style_context_add_provider_for_screen(
            screen,
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    g_object_unref(provider);
}

typedef struct {
    GtkWidget* frame;
    GtkWidget* grid;
    guint count;
    guint columns;
} GroupWidgets;

static GtkWidget* create_group_frame(const gchar* title, guint columns, GroupWidgets* out) {
    GtkWidget* frame = gtk_frame_new(NULL);
    gtk_widget_set_hexpand(frame, TRUE);
    GtkWidget* label = gtk_label_new(NULL);
    gchar* markup = g_markup_printf_escaped("<span font_desc=\"12\" weight=\"bold\">%s</span>", title);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    g_free(markup);
    gtk_frame_set_label_widget(GTK_FRAME(frame), label);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_widget_set_margin_top(grid, 12);
    gtk_widget_set_margin_bottom(grid, 12);
    gtk_widget_set_margin_start(grid, 12);
    gtk_widget_set_margin_end(grid, 12);
    GtkStyleContext* inner_ctx = gtk_widget_get_style_context(grid);
    gtk_style_context_add_class(inner_ctx, "flues-group-inner");
    gtk_container_add(GTK_CONTAINER(frame), grid);

    GtkStyleContext* frame_context = gtk_widget_get_style_context(frame);
    gtk_style_context_add_class(frame_context, "flues-group");

    out->frame = frame;
    out->grid = grid;
    out->count = 0;
    out->columns = columns > 0 ? columns : 3;
    return frame;
}

static void on_knob_value_changed(FluesKnob* knob, float value, gpointer user_data) {
    PMSynthUI* ui = (PMSynthUI*)user_data;
    guint port = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(knob), "lv2-port"));
    if (port >= PORT_TOTAL_COUNT) {
        return;
    }
    if (ui->write) {
        ui->write(ui->controller, port, sizeof(float), 0, &value);
    }
}

static void add_control_to_group(PMSynthUI* ui,
                                 GroupWidgets* groups,
                                 const ControlDesc* desc) {
    FluesKnob* knob = flues_knob_new(desc->label, desc->min, desc->max, desc->def, desc->steps);
    GtkWidget* knob_widget = GTK_WIDGET(knob);
    g_object_set_data(G_OBJECT(knob), "lv2-port", GUINT_TO_POINTER(desc->port));
    g_signal_connect(knob, "value-changed", G_CALLBACK(on_knob_value_changed), ui);

    guint index = groups[desc->group].count++;
    guint col = index % groups[desc->group].columns;
    guint row = index / groups[desc->group].columns;
    gtk_grid_attach(GTK_GRID(groups[desc->group].grid), knob_widget, col, row, 1, 1);

    if (desc->port < PORT_TOTAL_COUNT) {
        ui->knobs[desc->port] = knob;
    }
}

static LV2UI_Handle ui_instantiate(const LV2UI_Descriptor* descriptor,
                                   const char* plugin_uri,
                                   const char* bundle_path,
                                   LV2UI_Write_Function write_function,
                                   LV2UI_Controller controller,
                                   LV2UI_Widget* widget,
                                   const LV2_Feature* const* features) {
    if (strcmp(plugin_uri, PMSYNTH_URI) != 0) {
        return NULL;
    }

    PMSynthUI* ui = g_new0(PMSynthUI, 1);
    ui->write = write_function;
    ui->controller = controller;

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_name(root, "flues-ui-root");
    apply_css(root);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 18);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 18);
    gtk_box_pack_start(GTK_BOX(root), grid, TRUE, TRUE, 0);

    GroupWidgets group_widgets[GROUP_COUNT];
    create_group_frame(kGroupInfo[GROUP_STEAM].title, kGroupInfo[GROUP_STEAM].columns, &group_widgets[GROUP_STEAM]);
    create_group_frame(kGroupInfo[GROUP_INTERFACE].title, kGroupInfo[GROUP_INTERFACE].columns, &group_widgets[GROUP_INTERFACE]);
    create_group_frame(kGroupInfo[GROUP_ENVELOPE].title, kGroupInfo[GROUP_ENVELOPE].columns, &group_widgets[GROUP_ENVELOPE]);
    create_group_frame(kGroupInfo[GROUP_PIPE].title, kGroupInfo[GROUP_PIPE].columns, &group_widgets[GROUP_PIPE]);
    create_group_frame(kGroupInfo[GROUP_FILTER].title, kGroupInfo[GROUP_FILTER].columns, &group_widgets[GROUP_FILTER]);
    create_group_frame(kGroupInfo[GROUP_MODULATION].title, kGroupInfo[GROUP_MODULATION].columns, &group_widgets[GROUP_MODULATION]);
    create_group_frame(kGroupInfo[GROUP_REVERB].title, kGroupInfo[GROUP_REVERB].columns, &group_widgets[GROUP_REVERB]);

    gtk_grid_attach(GTK_GRID(grid), group_widgets[GROUP_STEAM].frame, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), group_widgets[GROUP_INTERFACE].frame, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), group_widgets[GROUP_ENVELOPE].frame, 2, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), group_widgets[GROUP_PIPE].frame, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), group_widgets[GROUP_FILTER].frame, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), group_widgets[GROUP_MODULATION].frame, 2, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), group_widgets[GROUP_REVERB].frame, 0, 2, 3, 1);

    for (guint i = 0; i < G_N_ELEMENTS(kControlInfo); ++i) {
        add_control_to_group(ui, group_widgets, &kControlInfo[i]);
    }

    gtk_widget_show_all(root);

    ui->container = root;
    *widget = root;

    for (guint i = 0; i < PORT_TOTAL_COUNT; ++i) {
        if (ui->knobs[i]) {
            flues_knob_set_value(ui->knobs[i], flues_knob_get_value(ui->knobs[i]), FALSE);
        }
    }

    return ui;
}

static void ui_cleanup(LV2UI_Handle handle) {
    PMSynthUI* ui = (PMSynthUI*)handle;
    if (!ui) {
        return;
    }
    if (ui->container) {
        gtk_widget_destroy(ui->container);
    }
    g_free(ui);
}

static void ui_port_event(LV2UI_Handle handle,
                          uint32_t port_index,
                          uint32_t buffer_size,
                          uint32_t format,
                          const void* buffer) {
    PMSynthUI* ui = (PMSynthUI*)handle;
    if (!ui || format != 0 || buffer_size < sizeof(float)) {
        return;
    }
    if (port_index >= PORT_TOTAL_COUNT) {
        return;
    }
    FluesKnob* knob = ui->knobs[port_index];
    if (!knob) {
        return;
    }
    float value = *((const float*)buffer);
    flues_knob_set_value(knob, value, FALSE);
}

static const LV2UI_Descriptor ui_descriptor = {
    PMSYNTH_UI_URI,
    ui_instantiate,
    ui_cleanup,
    ui_port_event,
    NULL
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return index == 0 ? &ui_descriptor : NULL;
}
