#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include "AlgorithmOutput.hpp"
#include "AlgorithmUtils.hpp"

namespace flues::disyn {

class TrajectoryAlgorithm {
public:
    explicit TrajectoryAlgorithm(float sampleRate)
        : sampleRate(sampleRate),
          sides(6),
          startAngle(0.0f),
          startPositionAngle(0.0f),
          bounceJitter(0.0f),
          frequency(440.0f),
          speed(computeSpeed(440.0f)),
          position({0.0f, 0.0f}),
          velocity({speed, 0.0f}),
          rngState(0x12345678u) {
        rebuildPolygon();
        reset();
    }

    void reset() {
        resetPosition();
        updateVelocity();
    }

    AlgorithmOutput process(float pitch, float param1, float param2, float param3) {
        updateParams(pitch, param1, param2, param3);

        if (edges.empty()) {
            return {0.0f, 0.0f};
        }

        Vec2 current = position;
        Vec2 currentVelocity = velocity;

        for (int bounce = 0; bounce < 2; ++bounce) {
            const Vec2 next = {current.x + currentVelocity.x, current.y + currentVelocity.y};

            if (isInside(next)) {
                current = next;
                break;
            }

            const PenetrationHit hit = findPenetrationEdge(next);
            if (!hit.hit) {
                current = next;
                break;
            }

            const Vec2 reflected = reflect(currentVelocity, hit.normal);
            const Vec2 jittered = applyBounceJitter(reflected);
            const float nudge = 1e-4f;
            current = {
                next.x - hit.normal.x * (hit.distance + nudge),
                next.y - hit.normal.y * (hit.distance + nudge)
            };
            currentVelocity = jittered;
        }

        position = current;
        velocity = currentVelocity;

        return {position.x, position.y};
    }

private:
    struct Vec2 {
        float x;
        float y;
    };

    struct Edge {
        Vec2 start;
        Vec2 end;
        Vec2 normal;
    };

    struct PenetrationHit {
        bool hit;
        float distance;
        Vec2 normal;
    };

    float computeSpeed(float freq) const {
        return (freq * 4.0f) / sampleRate;
    }

    void updateParams(float pitch, float param1, float param2, float param3) {
        const int nextSides = clampInt(3 + static_cast<int>(std::round(param1 * 9.0f)), 3, 12);
        const float nextAngle = degToRad(param2 * 360.0f);
        const float nextJitter = degToRad(param3 * 10.0f);

        const bool sidesChanged = nextSides != sides;
        const bool launchChanged = std::abs(nextAngle - startAngle) > 1e-6f;
        const bool jitterChanged = std::abs(nextJitter - bounceJitter) > 1e-6f;
        const bool pitchChanged = std::abs(pitch - frequency) > 1e-6f;

        if (sidesChanged) {
            sides = nextSides;
            rebuildPolygon();
        }

        if (launchChanged) {
            startAngle = nextAngle;
            startPositionAngle = nextAngle;
        }

        if (jitterChanged) {
            bounceJitter = nextJitter;
        }

        if (pitchChanged) {
            frequency = pitch;
            speed = computeSpeed(frequency);
        }

        if (sidesChanged || launchChanged) {
            resetPosition();
            updateVelocity();
        } else if (pitchChanged) {
            updateVelocity();
        }
    }

    void rebuildPolygon() {
        vertices.clear();
        edges.clear();

        const float rotation = static_cast<float>(M_PI) / static_cast<float>(sides);
        vertices.reserve(static_cast<size_t>(sides));

        for (int i = 0; i < sides; ++i) {
            const float theta = (TWO_PI * static_cast<float>(i)) / static_cast<float>(sides) + rotation;
            vertices.push_back({std::cos(theta), std::sin(theta)});
        }

        edges.reserve(vertices.size());
        for (size_t i = 0; i < vertices.size(); ++i) {
            const Vec2 start = vertices[i];
            const Vec2 end = vertices[(i + 1) % vertices.size()];
            const Vec2 edge = {end.x - start.x, end.y - start.y};
            const Vec2 normal = normalize({edge.y, -edge.x});
            edges.push_back({start, end, normal});
        }
    }

    void resetPosition() {
        const Vec2 dir = {std::cos(startPositionAngle), std::sin(startPositionAngle)};
        const RayHit hit = findRayIntersection(dir);
        if (hit.hit) {
            position = {hit.point.x * 0.995f, hit.point.y * 0.995f};
        } else {
            position = {0.0f, 0.0f};
        }
    }

    void updateVelocity() {
        const Vec2 dir = {std::cos(startAngle), std::sin(startAngle)};
        velocity = {dir.x * speed, dir.y * speed};
    }

    struct RayHit {
        bool hit;
        float t;
        Vec2 point;
    };

    RayHit findRayIntersection(const Vec2& direction) const {
        RayHit closest{false, 0.0f, {0.0f, 0.0f}};
        for (const Edge& edge : edges) {
            RayHit hit = intersectRaySegment({0.0f, 0.0f}, direction, edge.start, edge.end);
            if (!hit.hit) {
                continue;
            }
            if (!closest.hit || hit.t < closest.t) {
                closest = hit;
            }
        }
        return closest;
    }

    RayHit intersectRaySegment(const Vec2& origin, const Vec2& direction, const Vec2& start,
                               const Vec2& end) const {
        const Vec2 segment = {end.x - start.x, end.y - start.y};
        const float denom = cross(direction, segment);
        if (std::abs(denom) < 1e-6f) {
            return {false, 0.0f, {0.0f, 0.0f}};
        }

        const Vec2 toStart = {start.x - origin.x, start.y - origin.y};
        const float t = cross(toStart, segment) / denom;
        const float u = cross(toStart, direction) / denom;

        if (t >= 0.0f && u >= 0.0f && u <= 1.0f) {
            return {true, t, {origin.x + direction.x * t, origin.y + direction.y * t}};
        }

        return {false, 0.0f, {0.0f, 0.0f}};
    }

    PenetrationHit findPenetrationEdge(const Vec2& point) const {
        PenetrationHit worst{false, 0.0f, {0.0f, 0.0f}};

        for (const Edge& edge : edges) {
            const Vec2 toPoint = {point.x - edge.start.x, point.y - edge.start.y};
            const float distance = toPoint.x * edge.normal.x + toPoint.y * edge.normal.y;
            if (distance > 0.0f && (!worst.hit || distance > worst.distance)) {
                worst = {true, distance, edge.normal};
            }
        }

        return worst;
    }

    bool isInside(const Vec2& point) const {
        for (const Edge& edge : edges) {
            const Vec2 edgeVector = {edge.end.x - edge.start.x, edge.end.y - edge.start.y};
            const Vec2 toPoint = {point.x - edge.start.x, point.y - edge.start.y};
            if (cross(edgeVector, toPoint) < -1e-6f) {
                return false;
            }
        }
        return true;
    }

    Vec2 reflect(const Vec2& vector, const Vec2& normal) const {
        const float dot = vector.x * normal.x + vector.y * normal.y;
        return {vector.x - 2.0f * dot * normal.x, vector.y - 2.0f * dot * normal.y};
    }

    Vec2 applyBounceJitter(const Vec2& vector) {
        if (bounceJitter <= 0.0f) {
            return vector;
        }
        const float randValue = randomUnit();
        const float angle = (randValue * 2.0f - 1.0f) * bounceJitter;
        const float cosAngle = std::cos(angle);
        const float sinAngle = std::sin(angle);
        return {
            vector.x * cosAngle - vector.y * sinAngle,
            vector.x * sinAngle + vector.y * cosAngle
        };
    }

    Vec2 normalize(const Vec2& vector) const {
        const float magnitude = std::hypot(vector.x, vector.y);
        if (magnitude < 1e-6f) {
            return {0.0f, 0.0f};
        }
        return {vector.x / magnitude, vector.y / magnitude};
    }

    int clampInt(int value, int minValue, int maxValue) const {
        if (value < minValue) {
            return minValue;
        }
        if (value > maxValue) {
            return maxValue;
        }
        return value;
    }

    float degToRad(float degrees) const {
        return (degrees * static_cast<float>(M_PI)) / 180.0f;
    }

    float cross(const Vec2& a, const Vec2& b) const {
        return a.x * b.y - a.y * b.x;
    }

    float randomUnit() {
        rngState = rngState * 1664525u + 1013904223u;
        return static_cast<float>((rngState >> 8) & 0xFFFFFF) / 16777216.0f;
    }

    float sampleRate;
    int sides;
    float startAngle;
    float startPositionAngle;
    float bounceJitter;
    float frequency;
    float speed;

    std::vector<Vec2> vertices;
    std::vector<Edge> edges;

    Vec2 position;
    Vec2 velocity;
    uint32_t rngState;
};

} // namespace flues::disyn
