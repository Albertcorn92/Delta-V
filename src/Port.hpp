#pragma once
// =============================================================================
// Port.hpp — DELTA-V Typed Port & Internal Queue System
// =============================================================================
// Design principles:
//   1. FlightData concept enforces trivially_copyable + standard_layout at
//      compile time — misuse is a build error, never a runtime surprise.
//   2. Single RingBuffer definition (RingBuffer.hpp is deleted).
//   3. pop() is always safe: returns false and leaves `out` untouched if empty.
//   4. Overflow is counted and readable via overflowCount() for FDIR monitoring.
//   5. OutputPort supports 1:1 connections (extend to fan-out via TelemHub).
// =============================================================================
#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include "Os.hpp"

namespace deltav {

// ---------------------------------------------------------------------------
// Concept: FlightData
// Every type carried over a port must be trivially copyable (safe to memcpy)
// and standard layout (predictable memory representation for serialization).
// ---------------------------------------------------------------------------
template<typename T>
concept FlightData =
    std::is_trivially_copyable_v<T> &&
    std::is_standard_layout_v<T>;

// ---------------------------------------------------------------------------
// RingBuffer<T, Capacity>
// Thread-safe bounded queue. Uses Os::Mutex spinlock so it is safe across
// Active (threaded) and Passive (scheduler-called) component boundaries.
//
// pop() returns bool — callers MUST check the return value.
// Overflow is tracked atomically and readable without locking.
// ---------------------------------------------------------------------------
template<FlightData T, size_t Capacity>
class RingBuffer {
public:
    static_assert(Capacity >= 2, "RingBuffer capacity must be at least 2");

    // Returns false and discards item if full. Increments overflow_count.
    bool push(const T& item) {
        Os::Mutex::Guard g(lock);
        size_t next = (head + 1) % (Capacity + 1);
        if (next == tail) {
            overflow_count.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        buf[head] = item;
        head = next;
        return true;
    }

    // Returns true and writes item to `out` if data is available.
    // Returns false and leaves `out` untouched if empty — NO UB.
    bool pop(T& out) {
        Os::Mutex::Guard g(lock);
        if (head == tail) return false;
        out = buf[tail];
        tail = (tail + 1) % (Capacity + 1);
        return true;
    }

    bool isEmpty() const {
        Os::Mutex::Guard g(const_cast<Os::Mutex&>(lock));
        return head == tail;
    }

    size_t size() const {
        Os::Mutex::Guard g(const_cast<Os::Mutex&>(lock));
        return (head - tail + Capacity + 1) % (Capacity + 1);
    }

    // FDIR: read and reset the overflow counter
    uint32_t drainOverflowCount() {
        return overflow_count.exchange(0, std::memory_order_acq_rel);
    }

    uint32_t peekOverflowCount() const {
        return overflow_count.load(std::memory_order_acquire);
    }

private:
    T      buf[Capacity + 1]{};
    size_t head{0};
    size_t tail{0};
    Os::Mutex lock;
    std::atomic<uint32_t> overflow_count{0};
};

// ---------------------------------------------------------------------------
// IInputPort<T> — Type-erased interface so OutputPort doesn't need to know
// the queue depth of the InputPort it connects to.
// ---------------------------------------------------------------------------
template<FlightData T>
class IInputPort {
public:
    virtual ~IInputPort() = default;
    virtual void receive(const T& data) = 0;
};

// ---------------------------------------------------------------------------
// InputPort<T, QUEUE_DEPTH>
// Wraps a RingBuffer. Components call hasNew() / consume() in their step().
// consume() returns a default-constructed T and logs nothing if empty —
// always call hasNew() first, or check the bool return of tryConsume().
// ---------------------------------------------------------------------------
template<FlightData T, size_t QUEUE_DEPTH = 16>
class InputPort : public IInputPort<T> {
public:
    void receive(const T& data) override {
        queue.push(data); // overflow counted inside RingBuffer
    }

    bool hasNew() const { return !queue.isEmpty(); }

    // Safe consume: returns true and writes to `out` if data was available.
    bool tryConsume(T& out) { return queue.pop(out); }

    // Convenience: returns default T{} if empty. Use hasNew() first.
    T consume() {
        T out{};
        queue.pop(out);
        return out;
    }

    uint32_t overflowCount() const { return queue.peekOverflowCount(); }
    uint32_t drainOverflowCount()  { return queue.drainOverflowCount(); }

    // Exposed for unit testing
    RingBuffer<T, QUEUE_DEPTH>& getQueue() { return queue; }

private:
    RingBuffer<T, QUEUE_DEPTH> queue;
};

// ---------------------------------------------------------------------------
// OutputPort<T>
// Sends data to exactly one connected InputPort. Unconnected sends are safe
// (silent no-op). Returns false if no port is connected.
// ---------------------------------------------------------------------------
template<FlightData T>
class OutputPort {
public:
    void connect(IInputPort<T>* input) { connected = input; }

    bool send(const T& data) {
        if (connected) {
            connected->receive(data);
            return true;
        }
        return false;
    }

    bool isConnected() const { return connected != nullptr; }

private:
    IInputPort<T>* connected{nullptr};
};

} // namespace deltav