#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace outsider_client {

template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity >= 2, "SpscQueue capacity must be at least 2");

public:
    bool push(const T& item) {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next = increment(head);
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T* out) {
        if (!out) {
            return false;
        }
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        *out = buffer_[tail];
        tail_.store(increment(tail), std::memory_order_release);
        return true;
    }

    void clear() {
        tail_.store(head_.load(std::memory_order_acquire), std::memory_order_release);
    }

private:
    static constexpr std::size_t increment(std::size_t index) {
        return (index + 1) % Capacity;
    }

    std::array<T, Capacity> buffer_{};
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};

}  // namespace outsider_client

