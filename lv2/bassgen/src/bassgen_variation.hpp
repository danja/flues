#pragma once

#include "bassgen_schema.h"

void bassgen_reset_variation_progress(VariationStateBlob* variation);
bool bassgen_apply_loop_variation(PatternStateBlob* pattern,
                                  VariationStateBlob* variation,
                                  const ControlSnapshot& controls,
                                  double beats_per_bar);
