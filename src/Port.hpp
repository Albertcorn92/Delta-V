#pragma once
// =============================================================================
// Port.hpp — DELTA-V Typesafe Messaging (Lock-free RingBuffer)  v3.0
// =============================================================================
// DO-178C Compliant:
// - std::array replaces C-style buffer
// - Rule of 5 enforced on interfaces
// - Auto return types used throughout
// =============================================================================
#include "Types.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <type_traits>

namespace deltav {

template <typename T>
concept FlightData =
    std::is_trivially_copyable_v<T> &&
    std::is_standard_layout_v<T> &&
    sizeof(T) <= 256;

template <FlightData T, size_t Capacity>
class RingBuffer {
public:
    RingBuffer() = default;

    auto push(const T& item) -> bool {
        uint32_t current_tail = tail.load(std::memory_order_relaxed);
        uint32_t next_tail    = (current_tail + 1) % (Capacity + 1);

        if (next_tail != head.load(std::memory_order_acquire)) {
            buf.at(current_tail) = item;
            tail.store(next_tail, std::memory_order_release);
            return true;
        }
        overflow_count.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    auto pop(T& out) -> bool {
        uint32_t current_head = head.load(std::memory_order_relaxed);
        if (current_head == tail.load(std::memory_order_acquire)) {
            return false;
        }
        out = buf.at(current_head);
        head.store((current_head + 1) % (Capacity + 1), std::memory_order_release);
        return true;
    }

    [[nodiscard]] auto isEmpty() const -> bool {
        return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
    }

    [[nodiscard]] auto isFull() const -> bool {
        uint32_t next_tail = (tail.load(std::memory_order_acquire) + 1) % (Capacity + 1);
        return next_tail == head.load(std::memory_order_acquire);
    }

    [[nodiscard]] auto size() const -> size_t {
        const uint32_t current_head = head.load(std::memory_order_acquire);
        const uint32_t current_tail = tail.load(std::memory_order_acquire);
        if (current_tail >= current_head) {
            return static_cast<size_t>(current_tail - current_head);
        }
        return static_cast<size_t>((Capacity + 1) - (current_head - current_tail));
    }

    auto drainOverflowCount() -> uint32_t {
        return overflow_count.exchange(0, std::memory_order_relaxed);
    }

    [[nodiscard]] auto peekOverflowCount() const -> uint32_t {
        return overflow_count.load(std::memory_order_relaxed);
    }

private:
    std::array<T, Capacity + 1> buf{};
    std::atomic<uint32_t> head{0};
    std::atomic<uint32_t> tail{0};
    std::atomic<uint32_t> overflow_count{0};
};

template <FlightData T>
class IInputPort {
public:
    IInputPort() = default;
    virtual ~IInputPort() = default;
    [[nodiscard]] virtual auto receive(const T& data) -> bool = 0;
    
    // DO-178C: Rule of 5 for interfaces
    IInputPort(const IInputPort&) = delete;
    auto operator=(const IInputPort&) -> IInputPort& = delete;
    IInputPort(IInputPort&&) = delete;
    auto operator=(IInputPort&&) -> IInputPort& = delete;
};

// DO-178C: Use explicitly named constant instead of magic number
constexpr size_t DEFAULT_QUEUE_DEPTH = 16;

template <FlightData T, size_t QUEUE_DEPTH = DEFAULT_QUEUE_DEPTH>
class InputPort : public IInputPort<T> {
public:
    InputPort() = default;

    [[nodiscard]] auto hasNew() const -> bool { return !queue.isEmpty(); }

    auto tryConsume(T& out) -> bool { return queue.pop(out); }

    auto consume() -> T {
        T data{};
        queue.pop(data);
        return data;
    }

    [[nodiscard]] auto overflowCount() const -> uint32_t { return queue.peekOverflowCount(); }
    auto drainOverflowCount() -> uint32_t { return queue.drainOverflowCount(); }
    [[nodiscard]] auto size() const -> size_t { return queue.size(); }

    auto getQueue() -> RingBuffer<T, QUEUE_DEPTH>& { return queue; }
    [[nodiscard]] auto receive(const T& data) -> bool override { return queue.push(data); }

private:
    RingBuffer<T, QUEUE_DEPTH> queue{};
};

template <FlightData T>
class OutputPort {
public:
    OutputPort() = default;

    template <size_t QUEUE_DEPTH>
    auto connect(InputPort<T, QUEUE_DEPTH>* dest) -> void {
        connected = dest;
    }

    auto send(const T& data) -> bool {
        if (connected != nullptr) {
            return connected->receive(data);
        }
        return false;
    }

    [[nodiscard]] auto isConnected() const -> bool { return connected != nullptr; }

private:
    IInputPort<T>* connected{nullptr};
};

} // namespace deltav
