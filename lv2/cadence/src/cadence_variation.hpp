#pragma once

#include "cadence_harmony.hpp"

void cadence_reset_variation_progress(VariationStateBlob* variation);
bool cadence_apply_cycle_variation(const SegmentCapture* learned_capture,
                                   int learned_segment_count,
                                   const ControlSnapshot& controls,
                                   VariationStateBlob* variation,
                                   const ChordSlot* base_slots,
                                   int base_segment_count,
                                   const ChordSlot* previous_playback,
                                   int previous_count,
                                   ChordSlot* out_slots);
