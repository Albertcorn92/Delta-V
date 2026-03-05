#pragma once
#include <array>
#include <atomic>
#include <optional>

namespace deltav {

template <typename T, size_t Size>
class RingBuffer {
public:
    RingBuffer() : head(0), tail(0) {}

    bool push(const T& item) {
        size_t next_head = (head + 1) % Size;
        if (next_head == tail.load(std::memory_order_acquire)) {
            return false; // Buffer Full
        }
        data[head] = item;
        head.store(next_head, std::memory_order_release);
        return true;
    }

    T pop() {
        size_t current_tail = tail.load(std::memory_order_relaxed);
        T item = data[current_tail];
        tail.store((current_tail + 1) % Size, std::memory_order_release);
        return item;
    }

    bool isEmpty() const {
        return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
    }

private:
    std::array<T, Size> data;
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
};

} // namespace deltav