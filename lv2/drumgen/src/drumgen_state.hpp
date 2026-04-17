#pragma once

#include "drumgen_schema.h"

#include <lv2/state/state.h>
#include <lv2/urid/urid.h>

struct DrumGenStateURIDs {
    LV2_URID atom_chunk = 0;
    LV2_URID controls = 0;
    LV2_URID pattern = 0;
    LV2_URID variation = 0;
};

LV2_State_Status drumgen_save_state(LV2_State_Store_Function store,
                                    LV2_State_Handle handle,
                                    const DrumGenStateURIDs& urids,
                                    const ControlSnapshot& controls,
                                    const PatternStateBlob& pattern,
                                    const VariationStateBlob& variation);

LV2_State_Status drumgen_restore_state(LV2_State_Retrieve_Function retrieve,
                                       LV2_State_Handle handle,
                                       const DrumGenStateURIDs& urids,
                                       ControlSnapshot* controls,
                                       PatternStateBlob* pattern,
                                       VariationStateBlob* variation,
                                       bool* pattern_valid);
