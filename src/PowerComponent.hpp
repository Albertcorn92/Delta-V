#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Types.hpp"
#include <iostream>

namespace deltav {

class PowerComponent : public Component {
public:
    PowerComponent(std::string_view name, uint32_t id) : Component(name, id) {}

    OutputPort<Serializer::ByteArray> telemetry_out;
    OutputPort<float> battery_out_internal;
    OutputPort<EventPacket> event_out;
    InputPort<CommandPacket> cmd_in; // 📥 Command Input

    void init() override {
        soc = 100.0f;
    }

    void step() override {
        // 🚀 1. COMMAND PROCESSING
        // Check if a new command is waiting in the input port
        if (cmd_in.hasNew()) {
            CommandPacket cmd = cmd_in.consume();
            if (cmd.opcode == 1) { // RESET_BATTERY
                soc = 100.0f;
                std::cout << "[BatterySystem] EXEC: Resetting SOC to 100.0" << std::endl;
                event_out.send(EventPacket::create(1, "BATT: Reset Success"));
            }
        }

        // 2. DISCHARGE SIMULATION
        if (soc > 0.0f) {
            soc -= 0.1f;
        }

        // 3. TELEMETRY GENERATION
        TelemetryPacket p = {0, getId(), soc};
        telemetry_out.send(Serializer::pack(p));
        
        battery_out_internal.send(soc);
    }

private:
    float soc = 100.0f;
};

} // namespace deltav