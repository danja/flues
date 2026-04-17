#pragma once

#include "bassgen_schema.h"

#include <cstdint>

bool bassgen_transport_restart_detected(bool was_playing,
                                        int64_t last_transport_step,
                                        int64_t start_step_floor);
double bassgen_local_step_from_absolute(const PatternStateBlob& pattern, double abs_steps);
const NoteEventData* bassgen_find_active_event(const PatternStateBlob& pattern, double local_step);
const NoteEventData* bassgen_find_event_starting_at(const PatternStateBlob& pattern, int local_step);
bool bassgen_any_event_ends_at(const PatternStateBlob& pattern, int local_step);
uint32_t bassgen_frame_for_boundary(double abs_steps_start,
                                    double abs_steps_end,
                                    uint32_t nframes,
                                    int64_t boundary);
int bassgen_local_step_for_boundary(const PatternStateBlob& pattern, int64_t boundary);
