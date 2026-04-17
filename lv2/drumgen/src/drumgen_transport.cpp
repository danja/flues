#include "drumgen_transport.hpp"

#include <cmath>

namespace {

inline int clampi(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

}  // namespace

bool drumgen_transport_restart_detected(bool was_playing,
                                        int64_t last_transport_step,
                                        int64_t start_step_floor) {
    return !was_playing || (last_transport_step >= 0 && start_step_floor < last_transport_step);
}

double drumgen_local_step_from_absolute(const PatternStateBlob& pattern, double abs_steps) {
    const double local_step = fmod(abs_steps, (double)pattern.total_steps);
    return local_step < 0.0 ? local_step + pattern.total_steps : local_step;
}

int drumgen_local_step_for_boundary(const PatternStateBlob& pattern, int64_t boundary) {
    const double local_step = fmod((double)boundary, (double)pattern.total_steps);
    return (int)(local_step < 0.0 ? local_step + pattern.total_steps : local_step);
}

uint32_t drumgen_frame_for_boundary(double abs_steps_start,
                                    double abs_steps_end,
                                    uint32_t nframes,
                                    int64_t boundary) {
    const double rel_steps = (double)boundary - abs_steps_start;
    const double t = rel_steps / (abs_steps_end - abs_steps_start + 1e-12);
    return (uint32_t)clampi((int)floor(t * (double)nframes), 0, (int)nframes - 1);
}
