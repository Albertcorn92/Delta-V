#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Types.hpp"

namespace deltav {

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

    void step() override {
        static uint32_t cycles = 0;
        cycles++;

        // 1. Hardware FDIR: Monitor Power Levels
        if (battery_level_in.hasNew()) {
            float current_battery = battery_level_in.consume();
            
            // Trigger Safe Mode if battery is critically low (e.g., under 5%)
            if (current_battery <= 5.0f) {
                event_out.send(EventPacket::create(3, "CRITICAL: Battery <= 5%. SAFE MODE ENGAGED."));
                
                // Future expansion: We could add an OutputPort<CommandPacket> here 
                // to automatically send a shutdown command to the SensorComponent.
            }
        }

        // 2. System Heartbeat (every 10 seconds at 1Hz)
        if (cycles % 10 == 0) {
            event_out.send(EventPacket::create(1, "HEARTBEAT: System Nominal"));
        }
    }
};

} // namespace deltav