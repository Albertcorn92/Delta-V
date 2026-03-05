#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "Types.hpp"
#include "ParamDb.hpp"
#include <ctime>

namespace deltav {

class PowerComponent : public Component {
public:
    // Infrastructure Ports
    deltav::OutputPort<Serializer::ByteArray> telemetry_out;
    deltav::OutputPort<EventPacket> event_out;
    deltav::InputPort<CommandPacket> cmd_in;

    PowerComponent(std::string_view name, uint32_t id) : Component(name, id) {}

    void init() override {
        // Initialize default parameter if not already set
        // Param ID 2001: Battery Drain Rate (Default 0.1)
        ParamDb::getInstance().getParam(2001, 0.1f);
    }

    void step() override {
        // 1. Process Incoming Commands
        if (cmd_in.hasNew()) {
            CommandPacket cmd = cmd_in.consume();
            
            if (cmd.opcode == 1) { // RESET_BATTERY
                battery_level = 100.0f;
                event_out.send(EventPacket::create(1, "CMD: Battery Recharged"));
            } 
            else if (cmd.opcode == 2) { // SET_DRAIN_RATE
                ParamDb::getInstance().setParam(2001, cmd.argument);
                event_out.send(EventPacket::create(1, "PARAM: Drain Rate Updated"));
            }
        }

        // 2. State Logic
        float drain_rate = ParamDb::getInstance().getParam(2001, 0.1f);
        battery_level -= drain_rate;
        
        if (battery_level < 0) {
            battery_level = 0.0f;
        }

        // 3. FDIR (Fault Detection, Isolation, and Recovery)
        if (battery_level < 15.0f && battery_level > (15.0f - drain_rate)) {
            event_out.send(EventPacket::create(2, "WARN: Critically Low Power"));
        }

        // 4. Send Telemetry
        TelemetryPacket p = { 
            static_cast<uint32_t>(std::time(nullptr)), 
            getId(), 
            battery_level 
        };
        
        telemetry_out.send(Serializer::pack(p));
    }

private:
    float battery_level = 100.0f;
};

} // namespace deltav