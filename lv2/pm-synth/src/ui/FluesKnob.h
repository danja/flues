#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define FLUES_TYPE_KNOB (flues_knob_get_type())
G_DECLARE_FINAL_TYPE(FluesKnob, flues_knob, FLUES, KNOB, GtkDrawingArea)

FluesKnob* flues_knob_new(const gchar* label,
                          float min_value,
                          float max_value,
                          float default_value,
                          guint steps);

void flues_knob_set_value(FluesKnob* knob, float value, gboolean emit_signal);
float flues_knob_get_value(FluesKnob* knob);
const gchar* flues_knob_get_label(FluesKnob* knob);

G_END_DECLS
