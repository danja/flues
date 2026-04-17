#include "bassgen_transport.hpp"

#include <cmath>

namespace {

inline int clampi(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

bool event_active_at(const NoteEventData& ev, double local_step) {
    const double start = (double)ev.start_step;
    const double end = (double)(ev.start_step + ev.duration_steps);
    return local_step >= start && local_step < end;
}

}  // namespace

bool bassgen_transport_restart_detected(bool was_playing,
                                        int64_t last_transport_step,
                                        int64_t start_step_floor) {
    return !was_playing || (last_transport_step >= 0 && start_step_floor < last_transport_step);
}

double bassgen_local_step_from_absolute(const PatternStateBlob& pattern, double abs_steps) {
    const double local_step = fmod(abs_steps, (double)pattern.pattern_steps);
    return local_step < 0.0 ? local_step + pattern.pattern_steps : local_step;
}

const NoteEventData* bassgen_find_active_event(const PatternStateBlob& pattern, double local_step) {
    for (int i = 0; i < pattern.event_count; ++i) {
        if (event_active_at(pattern.events[i], local_step)) {
            return &pattern.events[i];
        }
    }
    return nullptr;
}

const NoteEventData* bassgen_find_event_starting_at(const PatternStateBlob& pattern, int local_step) {
    for (int i = 0; i < pattern.event_count; ++i) {
        if (pattern.events[i].start_step == local_step) {
            return &pattern.events[i];
        }
    }
    return nullptr;
}

bool bassgen_any_event_ends_at(const PatternStateBlob& pattern, int local_step) {
    for (int i = 0; i < pattern.event_count; ++i) {
        const int end_step = (pattern.events[i].start_step + pattern.events[i].duration_steps) % pattern.pattern_steps;
        if (pattern.events[i].duration_steps < pattern.pattern_steps && end_step == local_step) {
            return true;
        }
    }
    return false;
}

uint32_t bassgen_frame_for_boundary(double abs_steps_start,
                                    double abs_steps_end,
                                    uint32_t nframes,
                                    int64_t boundary) {
    const double rel_steps = (double)boundary - abs_steps_start;
    const double t = rel_steps / (abs_steps_end - abs_steps_start + 1e-12);
    return (uint32_t)clampi((int)floor(t * (double)nframes), 0, (int)nframes - 1);
}

int bassgen_local_step_for_boundary(const PatternStateBlob& pattern, int64_t boundary) {
    const double local_step = fmod((double)boundary, (double)pattern.pattern_steps);
    return (int)(local_step < 0.0 ? local_step + pattern.pattern_steps : local_step);
}
