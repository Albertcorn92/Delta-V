#pragma once
#include <concepts>
#include <type_traits>
#include <array>
#include <atomic>

namespace deltav {

// Restrict port data to mathematically predictable, memory-safe types
template<typename T>
concept FlightData = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

/**
 * @brief A thread-safe, lock-based Ring Buffer.
 * This satisfies the 'RingBuffer' type required by the unit tests.
 */
template <FlightData T, size_t Size>
class RingBuffer {
public:
    RingBuffer() : head(0), tail(0) {}

    bool push(const T& data) {
        while (lock.test_and_set(std::memory_order_acquire)) { /* spin */ }
        
        size_t next_head = (head + 1) % Size;
        bool success = false;
        if (next_head != tail) {
            buffer[head] = data;
            head = next_head;
            success = true;
        }
        
        lock.clear(std::memory_order_release);
        return success;
    }

    T pop() {
        T data{};
        while (lock.test_and_set(std::memory_order_acquire)) { /* spin */ }
        
        if (head != tail) {
            data = buffer[tail];
            tail = (tail + 1) % Size;
        }
        
        lock.clear(std::memory_order_release);
        return data;
    }

    bool isEmpty() const {
        return head == tail;
    }

private:
    std::array<T, Size> buffer;
    size_t head = 0;
    size_t tail = 0;
    std::atomic_flag lock = ATOMIC_FLAG_INIT;
};

// Interface class allows OutputPorts to connect without knowing Input Queue Depth
template<FlightData T>
class IInputPort {
public:
    virtual ~IInputPort() = default;
    virtual void receive(const T& data) = 0;
};

// Upgraded InputPort using the RingBuffer logic
template<FlightData T, size_t QUEUE_DEPTH = 10>
class InputPort : public IInputPort<T> {
public:
    void receive(const T& data) override {
        queue.push(data);
    }

    bool hasNew() const {
        return !queue.isEmpty();
    }

    T consume() {
        return queue.pop();
    }

    // Exposed for testing
    RingBuffer<T, QUEUE_DEPTH> queue;
};

template<FlightData T>
class OutputPort {
public:
    void connect(IInputPort<T>* input) {
        _connected_input = input;
    }

    void send(const T& data) {
        if (_connected_input) {
            _connected_input->receive(data);
        }
    }

private:
    IInputPort<T>* _connected_input = nullptr;
};

} // namespace deltav