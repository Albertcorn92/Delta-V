#pragma once
#include <array>
#include <chrono>
#include <thread>
#include <iostream>
#include "Component.hpp"

namespace deltav {

// Define the maximum number of components the flight computer can run
constexpr size_t MAX_COMPONENTS = 32;

class Scheduler {
public:
    // New Method: Triggers the init() phase for all registered hardware/software
    void initAll() {
        std::cout << "[Scheduler] Initializing all subsystems..." << std::endl;
        for (size_t i = 0; i < component_count; ++i) {
            schedule[i]->init();
        }
        std::cout << "[Scheduler] All systems Green. Ready for loop entry." << std::endl;
    }

    void registerComponent(Component* comp) {
        if (component_count < MAX_COMPONENTS) {
            schedule[component_count++] = comp;
            std::cout << "[Scheduler] Locked into execution timeline: " << comp->getName() << std::endl;
        } else {
            std::cout << "[FATAL] Scheduler component limit reached. Refusing to load: " 
                      << comp->getName() << std::endl;
        }
    }

    void executeLoop(uint32_t target_hz) {
        std::cout << "\n[Scheduler] Ignition. Entering real-time loop at " << target_hz << " Hz." << std::endl;
        std::cout << "[Scheduler] >> Press Ctrl+C in the terminal to abort simulation. <<\n" << std::endl;

        auto tick_duration = std::chrono::milliseconds(1000 / target_hz);

        while (true) {
            auto start_time = std::chrono::steady_clock::now();

            // Execute purely off the pre-allocated static array
            for (size_t i = 0; i < component_count; ++i) {
                schedule[i]->step();
            }

            auto end_time = std::chrono::steady_clock::now();
            auto elapsed = end_time - start_time;
            
            if (elapsed < tick_duration) {
                std::this_thread::sleep_for(tick_duration - elapsed);
            } else {
                std::cout << "[WARNING] Frame drop detected! Execution exceeded tick duration." << std::endl;
            }
        }
    }

private:
    std::array<Component*, MAX_COMPONENTS> schedule;
    size_t component_count = 0;
};

} // namespace deltav