// trajectory_module.c
// Polygon bounce oscillator based on Trajectory experiment.

#include "dsp_modules.h"
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TRAJECTORY_MAX_SIDES 12
#define TRAJECTORY_MIN_SIDES 3
#define TRAJECTORY_EPSILON 1e-6f
#define TRAJECTORY_OUTPUT_GAIN 1.0f

typedef struct {
    float x;
    float y;
} TrajVec2;

typedef struct {
    TrajVec2 start;
    TrajVec2 end;
    TrajVec2 normal;
} TrajEdge;

struct TrajectoryModule {
    float sample_rate;
    int sides;
    float start_position_angle;
    float start_angle;
    float bounce_jitter;
    float frequency;
    float speed;
    TrajVec2 position;
    TrajVec2 velocity;
    TrajVec2 vertices[TRAJECTORY_MAX_SIDES];
    TrajEdge edges[TRAJECTORY_MAX_SIDES];
    int edge_count;
    float mix_x;
    float mix_y;
};

static float traj_clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static int traj_clamp_int(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static float traj_deg_to_rad(float degrees) {
    return degrees * (float)M_PI / 180.0f;
}

static TrajVec2 traj_normalize(TrajVec2 v) {
    float mag = sqrtf(v.x * v.x + v.y * v.y);
    if (mag < TRAJECTORY_EPSILON) {
        TrajVec2 zero = {0.0f, 0.0f};
        return zero;
    }
    TrajVec2 out = {v.x / mag, v.y / mag};
    return out;
}

static float traj_cross(TrajVec2 a, TrajVec2 b) {
    return a.x * b.y - a.y * b.x;
}

static TrajVec2 traj_reflect(TrajVec2 v, TrajVec2 n) {
    float dot = v.x * n.x + v.y * n.y;
    TrajVec2 out = {v.x - 2.0f * dot * n.x, v.y - 2.0f * dot * n.y};
    return out;
}

static TrajVec2 traj_apply_jitter(const TrajectoryModule* traj, TrajVec2 v) {
    if (traj->bounce_jitter <= 0.0f) {
        return v;
    }
    float rand_unit = (float)rand() / (float)RAND_MAX;
    float angle = (rand_unit * 2.0f - 1.0f) * traj->bounce_jitter;
    float c = cosf(angle);
    float s = sinf(angle);
    TrajVec2 out = {
        v.x * c - v.y * s,
        v.x * s + v.y * c
    };
    return out;
}

static float traj_compute_speed(float frequency, float sample_rate) {
    return (frequency * 4.0f) / sample_rate;
}

static void traj_rebuild_polygon(TrajectoryModule* traj) {
    traj->edge_count = traj->sides;
    float rotation = (float)M_PI / (float)traj->sides;

    for (int i = 0; i < traj->sides; i++) {
        float theta = (2.0f * (float)M_PI * (float)i) / (float)traj->sides + rotation;
        traj->vertices[i].x = cosf(theta);
        traj->vertices[i].y = sinf(theta);
    }

    for (int i = 0; i < traj->sides; i++) {
        TrajVec2 start = traj->vertices[i];
        TrajVec2 end = traj->vertices[(i + 1) % traj->sides];
        TrajVec2 edge = {end.x - start.x, end.y - start.y};
        TrajVec2 normal = traj_normalize((TrajVec2){edge.y, -edge.x});
        traj->edges[i].start = start;
        traj->edges[i].end = end;
        traj->edges[i].normal = normal;
    }
}

static bool traj_is_inside(const TrajectoryModule* traj, TrajVec2 point) {
    for (int i = 0; i < traj->edge_count; i++) {
        TrajEdge edge = traj->edges[i];
        TrajVec2 edge_vec = {edge.end.x - edge.start.x, edge.end.y - edge.start.y};
        TrajVec2 to_point = {point.x - edge.start.x, point.y - edge.start.y};
        if (traj_cross(edge_vec, to_point) < -TRAJECTORY_EPSILON) {
            return false;
        }
    }
    return true;
}

static bool traj_find_penetration_edge(const TrajectoryModule* traj, TrajVec2 point, TrajVec2* normal_out, float* distance_out) {
    bool found = false;
    float max_distance = 0.0f;
    TrajVec2 normal = {0.0f, 0.0f};

    for (int i = 0; i < traj->edge_count; i++) {
        TrajEdge edge = traj->edges[i];
        TrajVec2 to_point = {point.x - edge.start.x, point.y - edge.start.y};
        float distance = to_point.x * edge.normal.x + to_point.y * edge.normal.y;
        if (distance > 0.0f && (!found || distance > max_distance)) {
            found = true;
            max_distance = distance;
            normal = edge.normal;
        }
    }

    if (!found) {
        return false;
    }

    *normal_out = normal;
    *distance_out = max_distance;
    return true;
}

static void traj_reset_position(TrajectoryModule* traj) {
    TrajVec2 direction = {cosf(traj->start_position_angle), sinf(traj->start_position_angle)};
    TrajVec2 best_point = {0.0f, 0.0f};
    float best_t = 1e9f;

    for (int i = 0; i < traj->edge_count; i++) {
        TrajEdge edge = traj->edges[i];
        TrajVec2 segment = {edge.end.x - edge.start.x, edge.end.y - edge.start.y};
        float denom = traj_cross(direction, segment);
        if (fabsf(denom) < TRAJECTORY_EPSILON) {
            continue;
        }

        TrajVec2 to_start = {edge.start.x, edge.start.y};
        float t = traj_cross(to_start, segment) / denom;
        float u = traj_cross(to_start, direction) / denom;

        if (t >= 0.0f && u >= 0.0f && u <= 1.0f) {
            if (t < best_t) {
                best_t = t;
                best_point.x = direction.x * t;
                best_point.y = direction.y * t;
            }
        }
    }

    float scale = 0.995f;
    traj->position.x = best_point.x * scale;
    traj->position.y = best_point.y * scale;
}

static void traj_update_velocity(TrajectoryModule* traj) {
    TrajVec2 dir = {cosf(traj->start_angle), sinf(traj->start_angle)};
    traj->velocity.x = dir.x * traj->speed;
    traj->velocity.y = dir.y * traj->speed;
}

TrajectoryModule* trajectory_create(float sample_rate) {
    TrajectoryModule* traj = (TrajectoryModule*)calloc(1, sizeof(TrajectoryModule));
    if (!traj) return NULL;

    traj->sample_rate = sample_rate;
    traj->sides = 6;
    traj->start_position_angle = 0.0f;
    traj->start_angle = traj_deg_to_rad(45.0f);
    traj->bounce_jitter = 0.0f;
    traj->frequency = 440.0f;
    traj->speed = traj_compute_speed(traj->frequency, traj->sample_rate);
    traj->mix_x = 0.0f;
    traj->mix_y = 1.0f;
    traj_rebuild_polygon(traj);
    trajectory_reset(traj);
    return traj;
}

void trajectory_destroy(TrajectoryModule* traj) {
    if (!traj) return;
    free(traj);
}

void trajectory_reset(TrajectoryModule* traj) {
    if (!traj) return;
    traj_reset_position(traj);
    traj_update_velocity(traj);
}

void trajectory_set_frequency(TrajectoryModule* traj, float frequency) {
    if (!traj) return;
    traj->frequency = frequency;
    traj->speed = traj_compute_speed(traj->frequency, traj->sample_rate);
    traj_update_velocity(traj);
}

void trajectory_set_sides(TrajectoryModule* traj, int sides) {
    if (!traj) return;
    int clamped = traj_clamp_int(sides, TRAJECTORY_MIN_SIDES, TRAJECTORY_MAX_SIDES);
    if (clamped == traj->sides) return;
    traj->sides = clamped;
    traj_rebuild_polygon(traj);
    traj_reset_position(traj);
    traj_update_velocity(traj);
}

void trajectory_set_start_position_deg(TrajectoryModule* traj, float degrees) {
    if (!traj) return;
    float deg = traj_clamp(degrees, 0.0f, 360.0f);
    traj->start_position_angle = traj_deg_to_rad(deg);
    traj_reset_position(traj);
}

void trajectory_set_start_angle_deg(TrajectoryModule* traj, float degrees) {
    if (!traj) return;
    float deg = traj_clamp(degrees, 0.0f, 360.0f);
    traj->start_angle = traj_deg_to_rad(deg);
    traj_update_velocity(traj);
}

void trajectory_set_bounce_jitter_deg(TrajectoryModule* traj, float degrees) {
    if (!traj) return;
    float deg = traj_clamp(degrees, 0.0f, 10.0f);
    traj->bounce_jitter = traj_deg_to_rad(deg);
}

void trajectory_set_mix_x(TrajectoryModule* traj, float mix) {
    if (!traj) return;
    traj->mix_x = traj_clamp(mix, 0.0f, 1.0f);
}

void trajectory_set_mix_y(TrajectoryModule* traj, float mix) {
    if (!traj) return;
    traj->mix_y = traj_clamp(mix, 0.0f, 1.0f);
}

float trajectory_process(TrajectoryModule* traj) {
    if (!traj || traj->edge_count == 0) {
        return 0.0f;
    }

    TrajVec2 position = traj->position;
    TrajVec2 velocity = traj->velocity;

    for (int bounce = 0; bounce < 2; bounce++) {
        TrajVec2 next = {position.x + velocity.x, position.y + velocity.y};
        if (traj_is_inside(traj, next)) {
            position = next;
            break;
        }

        TrajVec2 normal;
        float distance = 0.0f;
        if (!traj_find_penetration_edge(traj, next, &normal, &distance)) {
            position = next;
            break;
        }

        TrajVec2 reflected = traj_reflect(velocity, normal);
        TrajVec2 jittered = traj_apply_jitter(traj, reflected);
        float nudge = 1e-4f;
        position.x = next.x - normal.x * (distance + nudge);
        position.y = next.y - normal.y * (distance + nudge);
        velocity = jittered;
    }

    traj->position = position;
    traj->velocity = velocity;

    const float output = position.x * traj->mix_x + position.y * traj->mix_y;
    return output * TRAJECTORY_OUTPUT_GAIN;
}
