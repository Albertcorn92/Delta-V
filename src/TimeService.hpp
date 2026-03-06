#pragma once
// =============================================================================
// TimeService.hpp — DELTA-V Mission Elapsed Time (MET)
// =============================================================================
// Single global clock synchronised at boot. All components call getMET() to
// timestamp telemetry — never call chrono directly in component code.
//
// Thread-safety: epoch is written once at boot (initEpoch) before any thread
// starts, so all subsequent reads are safely unsynchronised.
//
// Fixes applied (v2.1):
//   F-13: getMET() emits a one-time warning log when MET exceeds 0xF0000000
//         (~46 days), giving the ground team advance notice before the uint32_t
//         wraps at ~49.7 days. A static flag prevents log spam. Long-duration
//         missions should migrate the return type to uint64_t.
// =============================================================================
#include <chrono>
#include <cstdint>
#include <iostream>

namespace deltav {

class TimeService {
public:
    // Called exactly once during main() before Scheduler::initAll().
    static void initEpoch() {
        epoch       = std::chrono::steady_clock::now();
        initialized = true;
        overflow_warned = false;
    }

    // Returns milliseconds since initEpoch(). Safe to call from any thread.
    // F-13: Logs a one-time warning when approaching uint32_t wrap (~46 days).
    static uint32_t getMET() {
        auto now      = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - epoch);
        uint64_t ms   = static_cast<uint64_t>(duration.count());

        // Warn once when approaching uint32_t overflow (~46-day mark).
        if (!overflow_warned && ms > 0xF000'0000ULL) {
            overflow_warned = true;
            std::cerr << "[TimeService] WARNING: MET approaching uint32_t overflow "
                         "(~46 days). Upgrade to uint64_t for long-duration missions.\n";
        }

        return static_cast<uint32_t>(ms & 0xFFFF'FFFFUL);
    }

    static bool isReady() { return initialized; }

private:
    static inline std::chrono::steady_clock::time_point epoch =
        std::chrono::steady_clock::now();
    static inline bool initialized      = false;
    static inline bool overflow_warned  = false;
};

} // namespace deltav