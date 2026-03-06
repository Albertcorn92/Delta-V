#pragma once
// =============================================================================
// Scheduler.hpp — DELTA-V Real-Time Executive
// =============================================================================
// Manages two execution domains:
//   1. PASSIVE loop: runs all passive components in the master thread at target_hz
//   2. ACTIVE threads: each ActiveComponent owns its own Os::Thread (started here)
//
// Frame drop tracking: counts how many scheduler ticks overran their deadline.
// This is critical FDIR data — persistent frame drops indicate the passive
// component load is too high for the target rate and must be reduced.
// =============================================================================
#include "Component.hpp"
#include <array>
#include <chrono>
#include <iostream>
#include <thread>

namespace deltav {

constexpr size_t MAX_COMPONENTS = 32;

class Scheduler {
public:
    void registerComponent(Component* comp) {
        if (component_count < MAX_COMPONENTS) {
            schedule[component_count++] = comp;
            std::cout << "[Scheduler] Registered: " << comp->getName()
                      << (comp->isActive() ? " [ACTIVE]" : " [PASSIVE]") << "\n";
        } else {
            std::cerr << "[FATAL] Scheduler: component limit reached. "
                         "Cannot register " << comp->getName() << "\n";
        }
    }

    void initAll() {
        std::cout << "[Scheduler] Initializing " << component_count
                  << " subsystems...\n";
        for (size_t i = 0; i < component_count; ++i) {
            schedule[i]->init();
        }
        std::cout << "[Scheduler] All systems nominal.\n";
    }

    void executeLoop(uint32_t target_hz) {
        std::cout << "[Scheduler] Starting active threads...\n";
        for (size_t i = 0; i < component_count; ++i) {
            schedule[i]->startThread();
        }

        std::cout << "[Scheduler] Master loop at " << target_hz
                  << " Hz. Press Ctrl+C to abort.\n\n";

        const auto tick = std::chrono::nanoseconds(1'000'000'000ULL / target_hz);
        auto next_wake  = std::chrono::steady_clock::now();

        while (true) {
            for (size_t i = 0; i < component_count; ++i) {
                if (!schedule[i]->isActive()) {
                    schedule[i]->step();
                }
            }

            next_wake += tick;
            auto now = std::chrono::steady_clock::now();
            if (now > next_wake) {
                ++frame_drops;
                if (frame_drops % 10 == 1) {
                    std::cerr << "[Scheduler] WARNING: frame drop #" << frame_drops
                              << " — passive load exceeds " << target_hz << " Hz budget\n";
                }
            } else {
                std::this_thread::sleep_until(next_wake);
            }
        }
    }

    uint64_t getFrameDropCount() const { return frame_drops; }

    ~Scheduler() {
        for (size_t i = 0; i < component_count; ++i) {
            schedule[i]->joinThread();
        }
    }

private:
    std::array<Component*, MAX_COMPONENTS> schedule{};
    size_t   component_count{0};
    uint64_t frame_drops{0};
};

} // namespace deltav