#pragma once

#include <lv2/state/state.h>

#include "cadence_schema.h"
#include "cadence_urid.hpp"

LV2_State_Status cadence_save_state(LV2_State_Store_Function store,
                                    LV2_State_Handle handle,
                                    const CadenceURIDs& urids,
                                    const ChordSlot* playback,
                                    int playback_segment_count,
                                    bool ready,
                                    const VariationStateBlob& variation);

LV2_State_Status cadence_restore_state(LV2_State_Retrieve_Function retrieve,
                                       LV2_State_Handle handle,
                                       const CadenceURIDs& urids,
                                       ChordSlot* playback,
                                       int* playback_segment_count,
                                       bool* ready,
                                       VariationStateBlob* variation);
