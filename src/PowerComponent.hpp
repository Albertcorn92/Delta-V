#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "Types.hpp"
#include "ParamDb.hpp"
#include "TimeService.hpp"

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

        // 🚀 UPGRADE: Fetch globally synchronized Mission Elapsed Time (MET)
        uint32_t current_met = TimeService::getMET(); 

        TelemetryPacket p = { current_met, getId(), battery_level };
        telemetry_out.send(Serializer::pack(p));
        battery_out_internal.send(battery_level);
    }

private:
    float battery_level = 100.0f;
};

} // namespace deltav