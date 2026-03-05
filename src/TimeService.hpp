#pragma once
#include <chrono>
#include <cstdint>

namespace deltav {

class TimeService {
public:
    // Phase 1: Called once during boot sequence to synchronize all clocks
    static void initEpoch() {
        epoch = std::chrono::steady_clock::now();
    }

    // Phase 2: Called by Active and Passive components to timestamp telemetry
    static uint32_t getMET() {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - epoch);
        return static_cast<uint32_t>(duration.count());
    }

private:
    // C++17 inline static ensures only one global epoch exists in memory
    static inline std::chrono::time_point<std::chrono::steady_clock> epoch = std::chrono::steady_clock::now();
};

} // namespace deltav