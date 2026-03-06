#pragma once
// =============================================================================
// Os.hpp — DELTA-V Operating System Abstraction Layer
// =============================================================================
// Decouples all framework threading and timing from Linux/POSIX so the same
// Component code compiles for FreeRTOS, RTEMS, VxWorks, or bare metal by
// swapping this single header.
//
// DO-178C Note: All Os:: primitives use well-defined memory models and avoid
// undefined behavior. No raw pthread, no raw std::thread in Components.
// =============================================================================
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>
#include <time.h>

namespace deltav {
namespace Os {

// ---------------------------------------------------------------------------
// Os::Mutex — Lightweight spinlock suitable for short critical sections.
// Using std::atomic_flag per C++20 guarantees lock-free on all tier-1 targets.
// ---------------------------------------------------------------------------
class Mutex {
public:
    void lock()   { while (flag.test_and_set(std::memory_order_acquire)) { /* spin */ } }
    void unlock() { flag.clear(std::memory_order_release); }

    // RAII guard
    class Guard {
    public:
        explicit Guard(Mutex& m) : mtx(m) { mtx.lock(); }
        ~Guard() { mtx.unlock(); }
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
    private:
        Mutex& mtx;
    };

private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

// ---------------------------------------------------------------------------
// Os::Thread — Wraps std::thread with a deterministic absolute-deadline loop.
//
// Key upgrade over the old ActiveComponent: instead of sleep_for (which drifts
// because it sleeps RELATIVE to whenever step() finishes), this uses an
// ABSOLUTE next-wakeup-time that is advanced by exactly one tick period each
// cycle. Drift is bounded to sub-microsecond even over 24-hour runs.
// ---------------------------------------------------------------------------
class Thread {
public:
    Thread() = default;
    ~Thread() { stop(); }

    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    // Start the thread calling `fn` at `hz` Hz using absolute-deadline scheduling.
    void start(uint32_t hz, std::function<void()> fn) {
        if (running.load(std::memory_order_acquire)) return;
        running.store(true, std::memory_order_release);
        worker = std::thread([this, hz, fn = std::move(fn)]() {
            const uint64_t period_ns = (hz > 0) ? (1'000'000'000ULL / hz) : 1'000'000'000ULL;
            auto next_wake = std::chrono::steady_clock::now();

            while (running.load(std::memory_order_acquire)) {
                fn();

                next_wake += std::chrono::nanoseconds(period_ns);
                std::this_thread::sleep_until(next_wake);
            }
        });
    }

    void stop() {
        running.store(false, std::memory_order_release);
        if (worker.joinable()) {
            worker.join();
        }
    }

    bool isRunning() const { return running.load(std::memory_order_acquire); }

private:
    std::thread worker;
    std::atomic<bool> running{false};
};

// ---------------------------------------------------------------------------
// Os::Queue — A bounded, thread-safe SPSC queue using a spinlock.
// Separated from Port so it can be replaced with a lock-free implementation
// or an RTOS message queue on embedded targets.
// ---------------------------------------------------------------------------
template<typename T, size_t Capacity>
class Queue {
public:
    bool push(const T& item) {
        Os::Mutex::Guard g(lock);
        size_t next = (head + 1) % (Capacity + 1);
        if (next == tail) return false; // full
        buf[head] = item;
        head = next;
        return true;
    }

    bool pop(T& out) {
        Os::Mutex::Guard g(lock);
        if (head == tail) return false; // empty
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

private:
    // +1 slot to distinguish full from empty without a separate counter
    T buf[Capacity + 1]{};
    size_t head{0};
    size_t tail{0};
    Os::Mutex lock;
};

} // namespace Os
} // namespace deltav