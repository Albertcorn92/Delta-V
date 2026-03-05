#pragma once
#include <concepts>
#include <type_traits>
#include <array>
#include <atomic>

namespace deltav {

// Restrict port data to mathematically predictable, memory-safe types
template<typename T>
concept FlightData = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

// Interface class allows OutputPorts to connect without knowing the Input Queue Depth
template<FlightData T>
class IInputPort {
public:
    virtual ~IInputPort() = default;
    virtual void receive(const T& data) = 0;
};

// Upgraded InputPort with a fixed-size, thread-safe Ring Buffer Queue
template<FlightData T, size_t QUEUE_DEPTH = 10>
class InputPort : public IInputPort<T> {
public:
    // Push data into the queue (Protected by a thread-safe Micro-Spinlock)
    void receive(const T& data) override {
        while (lock.test_and_set(std::memory_order_acquire)) {
            // Spin and wait for lock (Highly efficient for RTOS environments)
        }
        
        size_t next_head = (head + 1) % QUEUE_DEPTH;
        if (next_head != tail) {
            buffer[head] = data;
            head = next_head;
        } 
        // Note: If queue is full, we intentionally drop the packet. 
        // In flight software, dropping telemetry is always safer than a memory overflow crash.
        
        lock.clear(std::memory_order_release);
    }

    bool hasNew() const {
        return head != tail;
    }

    // Pop data from the queue 
    T consume() {
        T data{};
        while (lock.test_and_set(std::memory_order_acquire)) { /* spin */ }
        
        if (head != tail) {
            data = buffer[tail];
            tail = (tail + 1) % QUEUE_DEPTH;
        }
        
        lock.clear(std::memory_order_release);
        return data;
    }

private:
    std::array<T, QUEUE_DEPTH> buffer;
    size_t head = 0;
    size_t tail = 0;
    std::atomic_flag lock = ATOMIC_FLAG_INIT;
};

template<FlightData T>
class OutputPort {
public:
    // Connects to the generic interface
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