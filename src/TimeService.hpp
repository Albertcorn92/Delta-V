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
#include <atomic>
#include <cstdint>
#include <cstdio>

namespace deltav {

class TimeService {
public:
    static constexpr uint64_t MET_WARN_THRESHOLD_MS = 0xF000'0000ULL;
    static constexpr uint64_t MET_WRAP_MASK         = 0xFFFF'FFFFULL;

    // Called exactly once during main() before Scheduler::initAll().
    static void initEpoch() {
        epoch       = std::chrono::steady_clock::now();
        initialized.store(true, std::memory_order_release);
        overflow_warned.store(false, std::memory_order_release);
        utc_offset_ms.store(0, std::memory_order_release);
        utc_synced.store(false, std::memory_order_release);
        last_utc_unix_ms.store(0, std::memory_order_release);
    }

    // Returns milliseconds since initEpoch() with 64-bit range.
    static uint64_t getMET64() {
        auto now      = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - epoch);
        return static_cast<uint64_t>(duration.count());
    }

    // Returns milliseconds since initEpoch(). Safe to call from any thread.
    // F-13: Logs a one-time warning when approaching uint32_t wrap (~46 days).
    static uint32_t getMET() {
        const uint64_t ms = getMET64();

        // Warn once when approaching uint32_t overflow (~46-day mark).
        bool expected = false;
        if (isNearOverflow(ms) &&
            overflow_warned.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
            (void)std::fprintf(stderr,
                "[TimeService] WARNING: MET approaching uint32_t overflow "
                "(~46 days). Upgrade to uint64_t for long-duration missions.\n");
        }

        return static_cast<uint32_t>(ms & MET_WRAP_MASK);
    }

    // UTC time sync support for ground "time at the tone" operations.
    // Internally clamped to a monotonic non-decreasing value.
    static void setUtcOffsetMs(int64_t offset_ms) {
        utc_offset_ms.store(offset_ms, std::memory_order_release);
        utc_synced.store(true, std::memory_order_release);
    }

    static void setUtcFromUnixMs(uint64_t unix_ms) {
        const int64_t met_ms = static_cast<int64_t>(getMET64());
        const int64_t target = static_cast<int64_t>(unix_ms);
        setUtcOffsetMs(target - met_ms);
    }

    static uint64_t getUtcUnixMs() {
        const int64_t met_ms = static_cast<int64_t>(getMET64());
        const int64_t offset = utc_offset_ms.load(std::memory_order_acquire);
        const int64_t utc_ms = met_ms + offset;
        uint64_t candidate = (utc_ms < 0) ? 0u : static_cast<uint64_t>(utc_ms);

        // Keep UTC monotonic in-process so leap-second/offset corrections cannot
        // drive time-triggered logic backwards.
        uint64_t last = last_utc_unix_ms.load(std::memory_order_acquire);
        while (true) {
            if (candidate < last) {
                candidate = last;
            }
            if (last_utc_unix_ms.compare_exchange_weak(
                    last, candidate, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return candidate;
            }
        }
    }

    static int64_t getUtcOffsetMs() {
        return utc_offset_ms.load(std::memory_order_acquire);
    }

    static bool hasUtcSync() {
        return utc_synced.load(std::memory_order_acquire);
    }

    static bool isReady() { return initialized.load(std::memory_order_acquire); }
    static bool isNearOverflow(uint64_t met_ms) { return met_ms > MET_WARN_THRESHOLD_MS; }

private:
    static inline std::chrono::steady_clock::time_point epoch =
        std::chrono::steady_clock::now();
    static inline std::atomic<bool> initialized{false};
    static inline std::atomic<bool> overflow_warned{false};
    static inline std::atomic<int64_t> utc_offset_ms{0};
    static inline std::atomic<bool> utc_synced{false};
    static inline std::atomic<uint64_t> last_utc_unix_ms{0};
};

} // namespace deltav
