#pragma once

#include "drumgen_schema.h"

#include <cstdint>

bool drumgen_transport_restart_detected(bool was_playing,
                                        int64_t last_transport_step,
                                        int64_t start_step_floor);
double drumgen_local_step_from_absolute(const PatternStateBlob& pattern, double abs_steps);
int drumgen_local_step_for_boundary(const PatternStateBlob& pattern, int64_t boundary);
uint32_t drumgen_frame_for_boundary(double abs_steps_start,
                                    double abs_steps_end,
                                    uint32_t nframes,
                                    int64_t boundary);
