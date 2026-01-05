#pragma once

#include <vector>
#include <cstddef>

template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity)
        : buffer_(capacity), capacity_(capacity) {}

    void reset() {
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }

    void push(const T& item) {
        if (capacity_ == 0) {
            return;
        }
        buffer_[head_] = item;
        head_ = (head_ + 1) % capacity_;
        if (size_ < capacity_) {
            ++size_;
        } else {
            tail_ = head_;
        }
    }

    bool pop(T* item) {
        if (size_ == 0 || capacity_ == 0) {
            return false;
        }
        *item = buffer_[tail_];
        tail_ = (tail_ + 1) % capacity_;
        --size_;
        return true;
    }

private:
    std::vector<T> buffer_;
    size_t capacity_ = 0;
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t size_ = 0;
};
