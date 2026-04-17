#pragma once

#include <cstdint>

struct BassGenRng {
    uint32_t state = 0x12345678u;

    void seed(uint32_t seed_value) {
        state = seed_value ? seed_value : 0x12345678u;
    }

    uint32_t next_u32() {
        state = state * 1664525u + 1013904223u;
        return state;
    }

    float next_float() {
        return (float)(next_u32() & 0x00FFFFFFu) / 16777215.0f;
    }

    int next_int(int min_value, int max_value) {
        if (max_value <= min_value) {
            return min_value;
        }
        const uint32_t span = (uint32_t)(max_value - min_value + 1);
        return min_value + (int)(next_u32() % span);
    }
};
