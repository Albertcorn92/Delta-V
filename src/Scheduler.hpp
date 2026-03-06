#pragma once
// =============================================================================
// Scheduler.hpp — DELTA-V Legacy Single-Rate Executive
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
    Scheduler() = default;
    ~Scheduler() = default;

    // DO-178C: Rule of 5
    Scheduler(const Scheduler&) = delete;
    auto operator=(const Scheduler&) -> Scheduler& = delete;
    Scheduler(Scheduler&&) = delete;
    auto operator=(Scheduler&&) -> Scheduler& = delete;

    auto registerComponent(Component* comp) -> void {
        if (comp == nullptr) {
            std::cerr << "[FATAL] Scheduler: null component registration rejected\n";
            return;
        }

        if (component_count < MAX_COMPONENTS) {
            schedule.at(component_count++) = comp;
            std::cout << "[Scheduler] Registered: " << comp->getName()
                      << (comp->isActive() ? " [ACTIVE]" : " [PASSIVE]") << "\n";
        } else {
            std::cerr << "[FATAL] Scheduler: component limit reached. "
                         "Cannot register " << comp->getName() << "\n";
        }
    }

    auto initAll() -> void {
        std::cout << "[Scheduler] Initializing " << component_count << " subsystems...\n";
        for (size_t i = 0; i < component_count; ++i) {
            schedule.at(i)->init();
        }
        std::cout << "[Scheduler] All systems nominal.\n";
    }

    auto executeLoop(uint32_t target_hz) -> void {
        if (target_hz == 0) {
            std::cerr << "[FATAL] Scheduler: target_hz must be > 0\n";
            return;
        }

        std::cout << "[Scheduler] Starting active threads...\n";
        for (size_t i = 0; i < component_count; ++i) {
            schedule.at(i)->startThread();
        }

        std::cout << "[Scheduler] Master loop at " << target_hz
                  << " Hz. Press Ctrl+C to abort.\n\n";

        const auto tick = std::chrono::nanoseconds(1'000'000'000ULL / target_hz);
        auto next_wake  = std::chrono::steady_clock::now();

        while (true) {
            for (size_t i = 0; i < component_count; ++i) {
                if (!schedule.at(i)->isActive()) {
                    schedule.at(i)->step();
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
                next_wake = now; // Prevent "Death Spiral" catch-up loop
            } else {
                std::this_thread::sleep_until(next_wake);
            }
        }
    }

    // Caller-owned components may outlive or die before Scheduler;
    // shutdown must be called explicitly while pointers are known-valid.
    auto shutdown() -> void {
        for (size_t i = 0; i < component_count; ++i) {
            if (schedule.at(i) != nullptr && schedule.at(i)->isActive()) {
                schedule.at(i)->joinThread();
            }
        }
    }

    [[nodiscard]] auto getFrameDropCount() const -> uint64_t { return frame_drops; }

private:
    std::array<Component*, MAX_COMPONENTS> schedule{};
    size_t   component_count{0};
    uint64_t frame_drops{0};
};

} // namespace deltav
