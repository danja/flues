#pragma once

#include <lv2/atom/atom.h>

#include "cadence_schema.h"
#include "cadence_urid.hpp"

constexpr double CADENCE_BEAT_EPSILON = 1e-6;

struct TimeInfo {
    bool valid = false;
    bool playing = false;
    double bar = 0.0;
    double barBeat = 0.0;
    double beatsPerBar = 4.0;
    double bpm = 120.0;
};

TimeInfo cadence_read_time_info(const LV2_Atom_Sequence* control, const CadenceURIDs& urids);
double cadence_note_length_fraction(const ControlSnapshot& controls);
double cadence_cycle_beats_for_controls(const ControlSnapshot& controls, double beats_per_bar);
double cadence_segment_beats_for_controls(const ControlSnapshot& controls, double beats_per_bar);
int cadence_segment_count_for_controls(const ControlSnapshot& controls, double beats_per_bar);
double cadence_wrapped_cycle_position(double abs_beats, const ControlSnapshot& controls, double beats_per_bar);
int cadence_segment_index_for_time(const ControlSnapshot& controls,
                                   double beats_per_bar,
                                   int segment_count,
                                   double abs_beats);
uint32_t cadence_frame_for_beat(double abs_beats_start,
                                double abs_beats_end,
                                uint32_t nframes,
                                double target_beat);
