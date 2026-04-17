#pragma once

#include "drumgen_schema.h"

void drumgen_reset_variation_progress(VariationStateBlob* variation);
bool drumgen_apply_loop_variation(PatternStateBlob* pattern,
                                  VariationStateBlob* variation,
                                  const ControlSnapshot& controls);
