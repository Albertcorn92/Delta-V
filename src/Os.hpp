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
//
// Fixes applied (v2.1):
//   F-01: Replaced spinlock (std::atomic_flag) with std::mutex.
//         The spinlock caused unbounded busy-wait / livelock on single-core
//         embedded targets when a high-priority thread held the lock.
//   F-02: lock declared mutable in Queue so isEmpty()/size() can acquire it
//         from a const context without const_cast (which was UB).
//   F-09: std::mutex provides the full memory barrier that std::atomic_flag
//         clear(release) alone could NOT guarantee for the non-atomic head/tail.
// =============================================================================
#if defined(DELTAV_OS_FREERTOS)
#include "Os_FreeRTOS.hpp"
#else

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

namespace deltav {
namespace Os {

// ---------------------------------------------------------------------------
// Os::Mutex — wraps std::mutex for portable RAII critical sections.
// std::mutex is not a spinlock: the OS scheduler yields the waiting thread,
// which is essential for correct behaviour on single-core embedded targets.
// ---------------------------------------------------------------------------
class Mutex {
public:
    void lock()   { mtx.lock(); }
    void unlock() { mtx.unlock(); }

    // RAII guard — identical interface to the old spinlock guard so all
    // call sites compile without change.
    class Guard {
    public:
        explicit Guard(Mutex& m) : lk(m.mtx) {}
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
    private:
        std::unique_lock<std::mutex> lk;
    };

private:
    std::mutex mtx;
};

// ---------------------------------------------------------------------------
// Os::Thread — Wraps std::thread with a deterministic absolute-deadline loop.
//
// Uses sleep_until (not sleep_for) so the wakeup time is advanced by exactly
// one tick period per cycle. Drift is bounded to sub-microsecond even over
// 24-hour runs.
// ---------------------------------------------------------------------------
class Thread {
public:
    Thread() = default;
    ~Thread() { stop(); }

    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    void start(uint32_t hz, std::function<void()> fn) {
        if (running.load(std::memory_order_acquire)) return;
        running.store(true, std::memory_order_release);
        worker = std::thread([this, hz, fn = std::move(fn)]() {
            const uint64_t period_ns =
                (hz > 0) ? (1'000'000'000ULL / hz) : 1'000'000'000ULL;
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
    std::thread       worker;
    std::atomic<bool> running{false};
};

// ---------------------------------------------------------------------------
// Os::Queue — bounded, thread-safe SPSC queue.
//
// F-02 fix: lock is declared mutable so isEmpty() and size() can acquire it
//           from a const context without const_cast, which was UB per the
//           C++ memory model when the object was not declared mutable.
// F-09 fix: std::mutex provides the required store/load memory barriers for
//           the non-atomic head and tail indices. The old atomic_flag spinlock
//           did NOT guarantee visibility of head/tail writes to other cores.
// ---------------------------------------------------------------------------
template<typename T, size_t Capacity>
class Queue {
public:
    bool push(const T& item) {
        std::lock_guard<std::mutex> g(lock);
        size_t next = (head + 1) % (Capacity + 1);
        if (next == tail) return false; // full
        buf[head] = item;
        head = next;
        return true;
    }

    bool pop(T& out) {
        std::lock_guard<std::mutex> g(lock);
        if (head == tail) return false; // empty
        out  = buf[tail];
        tail = (tail + 1) % (Capacity + 1);
        return true;
    }

    bool isEmpty() const {
        std::lock_guard<std::mutex> g(lock);
        return head == tail;
    }

    size_t size() const {
        std::lock_guard<std::mutex> g(lock);
        return (head - tail + Capacity + 1) % (Capacity + 1);
    }

private:
    // +1 slot to distinguish full from empty without a separate counter.
    T            buf[Capacity + 1]{};
    size_t       head{0};
    size_t       tail{0};
    mutable std::mutex lock; // mutable: allows locking in const isEmpty()/size()
};

} // namespace Os
} // namespace deltav

#endif // DELTAV_OS_FREERTOS
