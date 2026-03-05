#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "Types.hpp"
#include "ParamDb.hpp"
#include "TimeService.hpp"
#include <cmath>

namespace deltav {

class SensorComponent : public Component {
public:
    deltav::OutputPort<Serializer::ByteArray> telemetry_out_ground;
    deltav::OutputPort<EventPacket> event_out;
    deltav::InputPort<CommandPacket> cmd_in;

    SensorComponent(std::string_view name, uint32_t id) : Component(name, id) {}

    void init() override {
        ParamDb::getInstance().getParam(1001, 10.0f);
    }

    void step() override {
        float amplitude = ParamDb::getInstance().getParam(1001, 10.0f);
        static float angle = 0.0f;
        angle += 0.1f;
        float reading = std::sin(angle) * amplitude;

        // 🚀 UPGRADE: Fetch globally synchronized Mission Elapsed Time (MET)
        uint32_t current_met = TimeService::getMET();

        TelemetryPacket p = { current_met, getId(), reading };
        telemetry_out_ground.send(Serializer::pack(p));
    }

private:
};

} // namespace deltav