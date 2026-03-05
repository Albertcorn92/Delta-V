#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "Types.hpp"
#include "ParamDb.hpp"

namespace deltav {

class PowerComponent : public Component {
public:
    deltav::OutputPort<Serializer::ByteArray> telemetry_out;
    deltav::OutputPort<EventPacket> event_out;
    deltav::InputPort<CommandPacket> cmd_in;
    deltav::OutputPort<float> battery_out_internal;

    PowerComponent(std::string_view name, uint32_t id) : Component(name, id) {}

    void init() override {
        ParamDb::getInstance().getParam(2001, 0.1f);
    }

    void step() override {
        if (cmd_in.hasNew()) {
            CommandPacket cmd = cmd_in.consume();
            if (cmd.opcode == 1) { battery_level = 100.0f; }
        }

        float drain_rate = ParamDb::getInstance().getParam(2001, 0.1f);
        battery_level -= drain_rate;
        if (battery_level < 0) battery_level = 0.0f;

        // Use a 1000ms increment for the dashboard
        static uint32_t simulated_ms = 0;
        simulated_ms += 1000; 

        TelemetryPacket p = { simulated_ms, getId(), battery_level };
        telemetry_out.send(Serializer::pack(p));
        battery_out_internal.send(battery_level);
    }

private:
    float battery_level = 100.0f;
};

} // namespace deltav