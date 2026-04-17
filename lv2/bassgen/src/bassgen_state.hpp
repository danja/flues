#pragma once

#include "bassgen_schema.h"

#include <lv2/state/state.h>
#include <lv2/urid/urid.h>

struct BassGenStateURIDs {
    LV2_URID atom_chunk = 0;
    LV2_URID controls = 0;
    LV2_URID pattern = 0;
    LV2_URID variation = 0;
};

LV2_State_Status bassgen_save_state(LV2_State_Store_Function store,
                                    LV2_State_Handle handle,
                                    const BassGenStateURIDs& urids,
                                    const ControlSnapshot& controls,
                                    const PatternStateBlob& pattern,
                                    const VariationStateBlob& variation);

LV2_State_Status bassgen_restore_state(LV2_State_Retrieve_Function retrieve,
                                       LV2_State_Handle handle,
                                       const BassGenStateURIDs& urids,
                                       ControlSnapshot* controls,
                                       PatternStateBlob* pattern,
                                       VariationStateBlob* variation,
                                       bool* pattern_valid);
