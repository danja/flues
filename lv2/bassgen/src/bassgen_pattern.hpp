#pragma once

#include "bassgen_schema.h"

ControlSnapshot bassgen_clamp_controls(const ControlSnapshot& raw);
bool bassgen_structural_controls_changed(const ControlSnapshot& a, const ControlSnapshot& b);
int bassgen_steps_per_beat_for_subdivision(int subdivision);
int bassgen_register_offset(int reg);
void bassgen_regenerate_pattern(PatternStateBlob* pattern,
                                const ControlSnapshot& controls,
                                bool regen_rhythm,
                                bool regen_notes);
void bassgen_partial_note_mutation(PatternStateBlob* pattern,
                                   const ControlSnapshot& controls,
                                   float strength);
