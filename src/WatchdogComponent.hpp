#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Types.hpp"

namespace deltav {

class WatchdogComponent : public Component {
public:
    WatchdogComponent(std::string_view name, uint32_t id) : Component(name, id) {}

    OutputPort<EventPacket> event_out;

    void init() override {
        event_out.send(EventPacket::create(1, "WATCHDOG: Initialization Complete"));
    }

    void step() override {
        static uint32_t cycles = 0;
        cycles++;

        // Send a heartbeat event every 10 seconds (at 1Hz)
        if (cycles % 10 == 0) {
            event_out.send(EventPacket::create(1, "HEARTBEAT: System Nominal"));
        }
    }
};

} // namespace deltav