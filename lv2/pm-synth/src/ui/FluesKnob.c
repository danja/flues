#include "FluesKnob.h"

#include <math.h>
#include <gdk/gdkkeysyms.h>

struct _FluesKnob {
    GtkDrawingArea parent_instance;

    float min_value;
    float max_value;
    float value;
    float default_value;
    guint steps; // if > 1, quantise to steps

    gchar* label;

    gboolean dragging;
    gdouble drag_y;
    float drag_value;
    gboolean hover;
};

G_DEFINE_TYPE(FluesKnob, flues_knob, GTK_TYPE_DRAWING_AREA)

enum {
    PROP_0,
    PROP_LABEL,
    PROP_MIN,
    PROP_MAX,
    PROP_DEFAULT,
    PROP_STEPS,
    N_PROPS
};

enum {
    SIGNAL_VALUE_CHANGED,
    N_SIGNALS
};

static GParamSpec* properties[N_PROPS];
static guint signals[N_SIGNALS];

static float clamp_value(const FluesKnob* knob, float value) {
    if (value < knob->min_value) {
        value = knob->min_value;
    } else if (value > knob->max_value) {
        value = knob->max_value;
    }
    if (knob->steps > 1) {
        const float step = (knob->max_value - knob->min_value) / (float)(knob->steps - 1);
        value = knob->min_value + roundf((value - knob->min_value) / step) * step;
    }
    return value;
}

static gboolean flues_knob_draw(GtkWidget* widget, cairo_t* cr) {
    FluesKnob* knob = FLUES_KNOB(widget);
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);

    const double width = allocation.width;
    const double height = allocation.height;
    const double padding = 8.0;
    const double diameter = MIN(width, height) - padding * 2.0;
    const double radius = diameter / 2.0;
    const double cx = width / 2.0;
    const double cy = height / 2.0 - 4.0; // slight shift up, leave space for label

    // Background
    cairo_save(cr);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_set_source_rgb(cr, 0.10, 0.11, 0.13);
    cairo_fill(cr);
    cairo_restore(cr);

    // Outer ring
    cairo_save(cr);
    cairo_arc(cr, cx, cy, radius, 0, 2 * G_PI);
    cairo_set_source_rgb(cr, 0.13, 0.15, 0.18);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, 2.0);
    cairo_set_source_rgb(cr, 0.80, 0.48, 0.16);
    cairo_stroke(cr);
    cairo_restore(cr);

    // Inner circle
    cairo_save(cr);
    cairo_arc(cr, cx, cy, radius * 0.72, 0, 2 * G_PI);
    if (knob->hover || knob->dragging) {
        cairo_set_source_rgb(cr, 0.24, 0.26, 0.31);
    } else {
        cairo_set_source_rgb(cr, 0.18, 0.20, 0.24);
    }
    cairo_fill(cr);
    cairo_restore(cr);

    // Ticks
    cairo_save(cr);
    const guint ticks = knob->steps > 1 ? knob->steps : 11;
    for (guint i = 0; i < ticks; ++i) {
        const double t = (double)i / (double)(ticks - 1);
        const double angle = (G_PI * 1.5 * t) + (G_PI * 0.75);
        const double r_inner = radius * 0.82;
        const double r_outer = radius * 0.92;
        const double x1 = cx + cos(angle) * r_inner;
        const double y1 = cy + sin(angle) * r_inner;
        const double x2 = cx + cos(angle) * r_outer;
        const double y2 = cy + sin(angle) * r_outer;
        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
    }
    cairo_set_line_width(cr, 1.5);
    cairo_set_source_rgba(cr, 0.84, 0.64, 0.36, 0.5);
    cairo_stroke(cr);
    cairo_restore(cr);

    // Indicator
    const double norm = (knob->value - knob->min_value) / (knob->max_value - knob->min_value);
    const double angle = (norm * 1.5 * G_PI) + (G_PI * 0.75);
    const double indicator_outer = radius * 0.88;
    const double indicator_inner = radius * 0.20;

    cairo_save(cr);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_width(cr, 4.0);
    cairo_set_source_rgb(cr, 0.96, 0.63, 0.24);
    cairo_move_to(cr, cx + cos(angle) * indicator_inner, cy + sin(angle) * indicator_inner);
    cairo_line_to(cr, cx + cos(angle) * indicator_outer, cy + sin(angle) * indicator_outer);
    cairo_stroke(cr);
    cairo_restore(cr);

    // Value display
    cairo_save(cr);
    cairo_set_source_rgb(cr, 0.89, 0.85, 0.72);
    cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    char value_str[32];
    if (knob->steps > 1 && (knob->max_value - knob->min_value) <= 12.0f) {
        g_snprintf(value_str, sizeof value_str, "%.0f", knob->value);
    } else {
        g_snprintf(value_str, sizeof value_str, "%.2f", knob->value);
    }
    cairo_text_extents_t extents;
    cairo_text_extents(cr, value_str, &extents);
    cairo_move_to(cr, cx - extents.width / 2.0, cy + radius * 0.45);
    cairo_show_text(cr, value_str);
    cairo_restore(cr);

    // Label at bottom
    if (knob->label) {
        cairo_save(cr);
        cairo_set_source_rgb(cr, 0.72, 0.68, 0.58);
        cairo_select_font_face(cr, "Fira Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 10.0);
        cairo_text_extents(cr, knob->label, &extents);
        cairo_move_to(cr, cx - extents.width / 2.0, height - 6.0);
        cairo_show_text(cr, knob->label);
        cairo_restore(cr);
    }

    return FALSE;
}

static gboolean flues_knob_button_press(GtkWidget* widget, GdkEventButton* event) {
    if (event->button != 1) {
        return FALSE;
    }
    FluesKnob* knob = FLUES_KNOB(widget);
    knob->dragging = TRUE;
    knob->drag_y = event->y_root;
    knob->drag_value = knob->value;
    gtk_widget_grab_focus(widget);
    gtk_widget_queue_draw(widget);
    return TRUE;
}

static gboolean flues_knob_button_release(GtkWidget* widget, GdkEventButton* event) {
    FluesKnob* knob = FLUES_KNOB(widget);
    if (event->button == 1 && knob->dragging) {
        knob->dragging = FALSE;
        gtk_widget_queue_draw(widget);
        return TRUE;
    }
    return FALSE;
}

static gboolean flues_knob_motion(GtkWidget* widget, GdkEventMotion* event) {
    FluesKnob* knob = FLUES_KNOB(widget);
    if (!knob->dragging) {
        return FALSE;
    }

    const double delta = knob->drag_y - event->y_root;
    const double sensitivity = (knob->max_value - knob->min_value) / 200.0;
    float new_value = clamp_value(knob, knob->drag_value + delta * sensitivity);

    if (fabsf(new_value - knob->value) > 0.0001f) {
        knob->value = new_value;
        g_signal_emit(knob, signals[SIGNAL_VALUE_CHANGED], 0, knob->value);
        gtk_widget_queue_draw(widget);
    }
    return TRUE;
}

static gboolean flues_knob_scroll(GtkWidget* widget, GdkEventScroll* event) {
    FluesKnob* knob = FLUES_KNOB(widget);
    const double step = (knob->max_value - knob->min_value) / 100.0;
    float new_value = knob->value;
    if (event->direction == GDK_SCROLL_UP) {
        new_value += step * 4.0f;
    } else if (event->direction == GDK_SCROLL_DOWN) {
        new_value -= step * 4.0f;
    } else if (event->direction == GDK_SCROLL_SMOOTH) {
        double dx, dy;
        gdk_event_get_scroll_deltas((GdkEvent*)event, &dx, &dy);
        new_value -= dy * step * 6.0f;
    }
    new_value = clamp_value(knob, new_value);
    if (fabsf(new_value - knob->value) > 0.0001f) {
        knob->value = new_value;
        g_signal_emit(knob, signals[SIGNAL_VALUE_CHANGED], 0, knob->value);
        gtk_widget_queue_draw(widget);
    }
    return TRUE;
}

static gboolean flues_knob_enter(GtkWidget* widget, GdkEventCrossing* event) {
    FluesKnob* knob = FLUES_KNOB(widget);
    knob->hover = TRUE;
    gtk_widget_queue_draw(widget);
    return FALSE;
}

static gboolean flues_knob_leave(GtkWidget* widget, GdkEventCrossing* event) {
    FluesKnob* knob = FLUES_KNOB(widget);
    knob->hover = FALSE;
    gtk_widget_queue_draw(widget);
    return FALSE;
}

static gboolean flues_knob_key_press(GtkWidget* widget, GdkEventKey* event) {
    FluesKnob* knob = FLUES_KNOB(widget);
    const double step = (knob->max_value - knob->min_value) / 50.0;
    float new_value = knob->value;
    switch (event->keyval) {
        case GDK_KEY_Up:
        case GDK_KEY_Right:
            new_value += step * 2.0f;
            break;
        case GDK_KEY_Down:
        case GDK_KEY_Left:
            new_value -= step * 2.0f;
            break;
        case GDK_KEY_Home:
            new_value = knob->default_value;
            break;
        default:
            return FALSE;
    }
    new_value = clamp_value(knob, new_value);
    if (fabsf(new_value - knob->value) > 0.0001f) {
        knob->value = new_value;
        g_signal_emit(knob, signals[SIGNAL_VALUE_CHANGED], 0, knob->value);
        gtk_widget_queue_draw(widget);
    }
    return TRUE;
}

static void flues_knob_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec) {
    FluesKnob* knob = FLUES_KNOB(object);
    switch (prop_id) {
        case PROP_LABEL:
            g_value_set_string(value, knob->label);
            break;
        case PROP_MIN:
            g_value_set_float(value, knob->min_value);
            break;
        case PROP_MAX:
            g_value_set_float(value, knob->max_value);
            break;
        case PROP_DEFAULT:
            g_value_set_float(value, knob->default_value);
            break;
        case PROP_STEPS:
            g_value_set_uint(value, knob->steps);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void flues_knob_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec) {
    FluesKnob* knob = FLUES_KNOB(object);
    switch (prop_id) {
        case PROP_LABEL:
            g_free(knob->label);
            knob->label = g_value_dup_string(value);
            break;
        case PROP_MIN:
            knob->min_value = g_value_get_float(value);
            break;
        case PROP_MAX:
            knob->max_value = g_value_get_float(value);
            break;
        case PROP_DEFAULT:
            knob->default_value = g_value_get_float(value);
            knob->value = knob->default_value;
            break;
        case PROP_STEPS:
            knob->steps = g_value_get_uint(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void flues_knob_finalize(GObject* object) {
    FluesKnob* knob = FLUES_KNOB(object);
    g_clear_pointer(&knob->label, g_free);
    G_OBJECT_CLASS(flues_knob_parent_class)->finalize(object);
}

static void flues_knob_init(FluesKnob* knob) {
    gtk_widget_set_size_request(GTK_WIDGET(knob), 96, 110);
    gtk_widget_add_events(GTK_WIDGET(knob),
                          GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK |
                          GDK_POINTER_MOTION_MASK |
                          GDK_SCROLL_MASK |
                          GDK_ENTER_NOTIFY_MASK |
                          GDK_LEAVE_NOTIFY_MASK);
    gtk_widget_set_can_focus(GTK_WIDGET(knob), TRUE);
}

static void flues_knob_class_init(FluesKnobClass* klass) {
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    widget_class->draw = flues_knob_draw;
    widget_class->button_press_event = flues_knob_button_press;
    widget_class->button_release_event = flues_knob_button_release;
    widget_class->motion_notify_event = flues_knob_motion;
    widget_class->scroll_event = flues_knob_scroll;
    widget_class->key_press_event = flues_knob_key_press;
    widget_class->enter_notify_event = flues_knob_enter;
    widget_class->leave_notify_event = flues_knob_leave;

    object_class->get_property = flues_knob_get_property;
    object_class->set_property = flues_knob_set_property;
    object_class->finalize = flues_knob_finalize;

    properties[PROP_LABEL] = g_param_spec_string("label", "Label", "Display label", NULL, G_PARAM_READWRITE);
    properties[PROP_MIN] = g_param_spec_float("min", "Min", "Minimum value", -G_MAXFLOAT, G_MAXFLOAT, 0.0f, G_PARAM_READWRITE);
    properties[PROP_MAX] = g_param_spec_float("max", "Max", "Maximum value", -G_MAXFLOAT, G_MAXFLOAT, 1.0f, G_PARAM_READWRITE);
    properties[PROP_DEFAULT] = g_param_spec_float("default", "Default", "Default value", -G_MAXFLOAT, G_MAXFLOAT, 0.0f, G_PARAM_READWRITE);
    properties[PROP_STEPS] = g_param_spec_uint("steps", "Steps", "Number of discrete steps (>=2)", 0, G_MAXUINT, 0, G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPS, properties);

    signals[SIGNAL_VALUE_CHANGED] = g_signal_new(
        "value-changed",
        G_TYPE_FROM_CLASS(object_class),
        G_SIGNAL_RUN_FIRST,
        0,
        NULL,
        NULL,
        g_cclosure_marshal_VOID__FLOAT,
        G_TYPE_NONE,
        1,
        G_TYPE_FLOAT);
}

FluesKnob* flues_knob_new(const gchar* label,
                          float min_value,
                          float max_value,
                          float default_value,
                          guint steps) {
    return g_object_new(FLUES_TYPE_KNOB,
                        "label", label,
                        "min", min_value,
                        "max", max_value,
                        "default", default_value,
                        "steps", steps,
                        NULL);
}

void flues_knob_set_value(FluesKnob* knob, float value, gboolean emit_signal) {
    g_return_if_fail(FLUES_IS_KNOB(knob));
    float clamped = clamp_value(knob, value);
    if (fabsf(clamped - knob->value) > 0.0001f) {
        knob->value = clamped;
        if (emit_signal) {
            g_signal_emit(knob, signals[SIGNAL_VALUE_CHANGED], 0, knob->value);
        }
        gtk_widget_queue_draw(GTK_WIDGET(knob));
    }
}

float flues_knob_get_value(FluesKnob* knob) {
    g_return_val_if_fail(FLUES_IS_KNOB(knob), 0.0f);
    return knob->value;
}

const gchar* flues_knob_get_label(FluesKnob* knob) {
    g_return_val_if_fail(FLUES_IS_KNOB(knob), NULL);
    return knob->label;
}
