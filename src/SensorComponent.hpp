#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "Types.hpp"
#include "ParamDb.hpp"
#include <ctime>
#include <cmath>

namespace deltav {

class SensorComponent : public Component {
public:
    // Infrastructure Ports
    deltav::OutputPort<Serializer::ByteArray> telemetry_out_ground;
    deltav::OutputPort<EventPacket> event_out;
    deltav::InputPort<CommandPacket> cmd_in;

    SensorComponent(std::string_view name, uint32_t id) : Component(name, id) {}

    void init() override {
        // Param ID 1001: Sensor Amplitude (Default 10.0)
        ParamDb::getInstance().getParam(1001, 10.0f);
    }

    void step() override {
        // 1. Process Incoming Commands
        if (cmd_in.hasNew()) {
            CommandPacket cmd = cmd_in.consume();
            
            if (cmd.opcode == 1) { // RESET_SENSOR
                angle = 0.0f;
                event_out.send(EventPacket::create(1, "CMD: Sensor Phase Reset"));
            }
            else if (cmd.opcode == 2) { // SET_AMPLITUDE
                ParamDb::getInstance().setParam(1001, cmd.argument);
                event_out.send(EventPacket::create(1, "PARAM: Amplitude Updated"));
            }
        }

        // 2. Sensor Logic (Sine Wave Simulation)
        float amplitude = ParamDb::getInstance().getParam(1001, 10.0f);
        angle += 0.1f;
        float reading = std::sin(angle) * amplitude;

        // 3. Send Telemetry
        TelemetryPacket p = { 
            static_cast<uint32_t>(std::time(nullptr)), 
            getId(), 
            reading 
        };
        
        telemetry_out_ground.send(Serializer::pack(p));
    }

private:
    float angle = 0.0f;
};

} // namespace deltav