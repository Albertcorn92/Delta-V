#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Types.hpp"
#include <array>
#include <cstdio>

namespace deltav {

// Compile-time limit for the number of subsystems the Watchdog can supervise
constexpr size_t MAX_MONITORED_SUBSYSTEMS = 32;

class WatchdogComponent : public Component {
public:
    WatchdogComponent(std::string_view name, uint32_t id) : Component(name, id) {}

    // FDIR Input: Monitor critical subsystem health
    InputPort<float> battery_level_in;
    
    // Output: System events and alerts
    OutputPort<EventPacket> event_out;

    void init() override {
        event_out.send(EventPacket::create(1, "WATCHDOG: Initialization Complete"));
    }

    // Connects a subsystem to the Watchdog's supervision list
    void registerSubsystem(Component* comp) {
        if (monitored_count < MAX_MONITORED_SUBSYSTEMS) {
            monitored_systems[monitored_count++] = comp;
        }
    }

    void step() override {
        static uint32_t cycles = 0;
        cycles++;

        // 1. Hardware FDIR: Monitor Power Levels
        if (battery_level_in.hasNew()) {
            float current_battery = battery_level_in.consume();
            
            // Trigger Safe Mode if battery is critically low (e.g., under 5%)
            if (current_battery <= 5.0f) {
                event_out.send(EventPacket::create(3, "CRITICAL: Battery <= 5%. SAFE MODE ENGAGED."));
            }
        }

        // 2. Software FDIR: Monitor Active Subsystem Health
        for (size_t i = 0; i < monitored_count; ++i) {
            if (monitored_systems[i]->reportHealth() == HealthStatus::CRITICAL_FAILURE) {
                char msg[32];
                // Safely format the failure message with the specific component's name
                std::snprintf(msg, sizeof(msg), "CRIT: %s Health Failed", monitored_systems[i]->getName().data());
                event_out.send(EventPacket::create(3, msg));
                
                // Future expansion: Automatically call schedule[i]->joinThread() and restart it
            }
        }

        // 3. System Heartbeat (every 10 seconds at 1Hz)
        if (cycles % 10 == 0) {
            event_out.send(EventPacket::create(1, "HEARTBEAT: System Nominal"));
        }
    }

private:
    std::array<Component*, MAX_MONITORED_SUBSYSTEMS> monitored_systems;
    size_t monitored_count = 0;
};

} // namespace deltav