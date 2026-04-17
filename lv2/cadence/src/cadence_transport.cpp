#include "cadence_transport.hpp"

#include <lv2/atom/util.h>

#include <algorithm>
#include <cmath>

namespace {

inline int clampi(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

inline double clampd(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

bool atom_to_double(const LV2_Atom* atom, const CadenceURIDs& urids, double* out) {
    if (!atom || !out) {
        return false;
    }
    if (atom->type == urids.atom_Float) {
        *out = ((const LV2_Atom_Float*)atom)->body;
        return true;
    }
    if (atom->type == urids.atom_Double) {
        *out = ((const LV2_Atom_Double*)atom)->body;
        return true;
    }
    if (atom->type == urids.atom_Int) {
        *out = ((const LV2_Atom_Int*)atom)->body;
        return true;
    }
    if (atom->type == urids.atom_Long) {
        *out = (double)((const LV2_Atom_Long*)atom)->body;
        return true;
    }
    return false;
}

}  // namespace

TimeInfo cadence_read_time_info(const LV2_Atom_Sequence* control, const CadenceURIDs& urids) {
    TimeInfo info{};
    if (!control) {
        return info;
    }

    LV2_ATOM_SEQUENCE_FOREACH(control, ev) {
        const LV2_Atom_Object* obj = nullptr;
        if (ev->body.type == urids.time_Position) {
            obj = (const LV2_Atom_Object*)&ev->body;
        } else if (ev->body.type == urids.atom_Object || ev->body.type == urids.atom_Blank) {
            const LV2_Atom_Object* candidate = (const LV2_Atom_Object*)&ev->body;
            if (candidate->body.otype == urids.time_Position) {
                obj = candidate;
            }
        }

        if (!obj) {
            continue;
        }

        info.valid = true;

        const LV2_Atom* bar_atom = nullptr;
        const LV2_Atom* bar_beat_atom = nullptr;
        const LV2_Atom* beats_per_bar_atom = nullptr;
        const LV2_Atom* bpm_atom = nullptr;
        const LV2_Atom* speed_atom = nullptr;

        lv2_atom_object_get(obj,
                            urids.time_bar, &bar_atom,
                            urids.time_barBeat, &bar_beat_atom,
                            urids.time_beatsPerBar, &beats_per_bar_atom,
                            urids.time_beatsPerMinute, &bpm_atom,
                            urids.time_speed, &speed_atom,
                            0);

        double value = 0.0;
        if (atom_to_double(bar_atom, urids, &value)) {
            info.bar = value;
        }
        if (atom_to_double(bar_beat_atom, urids, &value)) {
            info.barBeat = value;
        }
        if (atom_to_double(beats_per_bar_atom, urids, &value)) {
            info.beatsPerBar = value > 0.0 ? value : 4.0;
        }
        if (atom_to_double(bpm_atom, urids, &value)) {
            info.bpm = value > 1.0 ? value : 120.0;
        }
        if (atom_to_double(speed_atom, urids, &value)) {
            info.playing = value > 0.0;
        } else {
            info.playing = true;
        }
    }

    return info;
}

double cadence_note_length_fraction(const ControlSnapshot& controls) {
    return clampd((double)controls.note_length, 0.10, 1.0);
}

double cadence_cycle_beats_for_controls(const ControlSnapshot& controls, double beats_per_bar) {
    return clampd((double)controls.cycle_bars * beats_per_bar, 1.0, (double)CADENCE_MAX_SEGMENTS * beats_per_bar);
}

double cadence_segment_beats_for_controls(const ControlSnapshot& controls, double beats_per_bar) {
    switch (controls.granularity) {
        case GRANULARITY_BEAT:
            return 1.0;
        case GRANULARITY_HALF_BAR:
            return std::max(0.5, beats_per_bar * 0.5);
        case GRANULARITY_BAR:
        default:
            return std::max(1.0, beats_per_bar);
    }
}

int cadence_segment_count_for_controls(const ControlSnapshot& controls, double beats_per_bar) {
    const double cycle_beats = cadence_cycle_beats_for_controls(controls, beats_per_bar);
    const double segment_beats = cadence_segment_beats_for_controls(controls, beats_per_bar);
    return clampi((int)lround(cycle_beats / segment_beats), 1, CADENCE_MAX_SEGMENTS);
}

double cadence_wrapped_cycle_position(double abs_beats, const ControlSnapshot& controls, double beats_per_bar) {
    const double cycle_beats = cadence_cycle_beats_for_controls(controls, beats_per_bar);
    double local = std::fmod(abs_beats, cycle_beats);
    if (local < 0.0) {
        local += cycle_beats;
    }
    return local;
}

int cadence_segment_index_for_time(const ControlSnapshot& controls,
                                   double beats_per_bar,
                                   int segment_count,
                                   double abs_beats) {
    const double cycle_pos = cadence_wrapped_cycle_position(abs_beats, controls, beats_per_bar);
    const double segment_beats = cadence_segment_beats_for_controls(controls, beats_per_bar);
    int index = (int)std::floor((cycle_pos + CADENCE_BEAT_EPSILON) / segment_beats);
    if (index >= segment_count) {
        index = segment_count - 1;
    }
    return clampi(index, 0, segment_count - 1);
}

uint32_t cadence_frame_for_beat(double abs_beats_start,
                                double abs_beats_end,
                                uint32_t nframes,
                                double target_beat) {
    if (nframes == 0 || abs_beats_end <= abs_beats_start + 1e-12) {
        return 0;
    }
    const double t = clampd((target_beat - abs_beats_start) / (abs_beats_end - abs_beats_start), 0.0, 1.0);
    return (uint32_t)clampi((int)lround(t * (double)nframes), 0, (int)nframes);
}
