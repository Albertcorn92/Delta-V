#pragma once
#include <vector>
#include <chrono>
#include <thread>
#include <iostream>
#include "Component.hpp"

namespace deltav {

class Scheduler {
public:
    // Adds a component to the execution timeline
    void registerComponent(Component* comp) {
        schedule.push_back(comp);
        std::cout << "[Scheduler] Locked into execution timeline: " << comp->getName() << std::endl;
    }

    // The Heartbeat: Runs the system infinitely at a specific frequency (Hz)
    void executeLoop(uint32_t target_hz) {
        std::cout << "\n[Scheduler] Ignition. Entering real-time loop at " << target_hz << " Hz." << std::endl;
        std::cout << "[Scheduler] >> Press Ctrl+C in the terminal to abort simulation. <<\n" << std::endl;

        // Calculate exactly how long one cycle should take in milliseconds
        auto tick_duration = std::chrono::milliseconds(1000 / target_hz);

        while (true) {
            auto start_time = std::chrono::steady_clock::now();

            // Fire every registered component in exact order
            for (Component* comp : schedule) {
                comp->step();
            }

            auto end_time = std::chrono::steady_clock::now();
            auto elapsed = end_time - start_time;
            
            // Deterministic Sleep: Only wait for the REMAINING time in the tick
            if (elapsed < tick_duration) {
                std::this_thread::sleep_for(tick_duration - elapsed);
            } else {
                std::cout << "[WARNING] Frame drop detected! Execution exceeded tick duration." << std::endl;
            }
        }
    }

private:
    std::vector<Component*> schedule;
};

} // namespace deltav