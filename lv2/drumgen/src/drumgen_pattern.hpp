#pragma once

#include "drumgen_schema.h"

ControlSnapshot drumgen_clamp_controls(const ControlSnapshot& raw);
bool drumgen_structural_controls_changed(const ControlSnapshot& a, const ControlSnapshot& b);
int drumgen_steps_per_beat_for_resolution(int resolution);
void drumgen_regenerate_pattern(PatternStateBlob* pattern,
                                const ControlSnapshot& controls,
                                bool fill_only_refresh);
void drumgen_refresh_bar(PatternStateBlob* pattern,
                         const ControlSnapshot& controls,
                         int bar_index);
