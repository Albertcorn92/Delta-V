#pragma once
// =============================================================================
// TimeService.hpp — DELTA-V Mission Elapsed Time (MET)
// =============================================================================
// Single global clock synchronized at boot. All components call getMET() to
// timestamp telemetry — never call chrono directly in component code.
//
// Thread-safety: epoch is written once at boot (initEpoch) before any thread
// starts, so all subsequent reads are safely unsynchronized.
// =============================================================================
#include <chrono>
#include <cstdint>

namespace deltav {

class TimeService {
public:
    // Called exactly once during main() before Scheduler::initAll().
    static void initEpoch() {
        epoch = std::chrono::steady_clock::now();
        initialized = true;
    }

    // Returns milliseconds since initEpoch(). Safe to call from any thread.
    // Wraps at ~49.7 days (uint32_t max). Missions longer than that need uint64_t.
    static uint32_t getMET() {
        auto now      = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - epoch);
        return static_cast<uint32_t>(duration.count());
    }

    // Returns true after initEpoch() has been called.
    static bool isReady() { return initialized; }

private:
    static inline std::chrono::steady_clock::time_point epoch =
        std::chrono::steady_clock::now();
    static inline bool initialized = false;
};

} // namespace deltav