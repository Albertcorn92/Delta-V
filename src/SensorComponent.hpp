#pragma once
// =============================================================================
// SensorComponent.hpp — DELTA-V Star Tracker / Generic Sensor
// =============================================================================
// Fixes over previous version:
//   - cmd_in is now actually drained in step() (was silently ignored before)
//   - recordError() called on any unrecognized opcode for FDIR visibility
//   - Uses TimeService::getMET() for all timestamps (was always correct here)
//   - ParamDb uses FNV-1a ID via constexpr helper
// =============================================================================
#include "Component.hpp"
#include "ParamDb.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <cmath>
#include <iostream>

namespace deltav {

// Compile-time param ID — no string hashing at runtime
constexpr uint32_t PARAM_STAR_AMPLITUDE = ParamDb::fnv1a("star_amplitude");

class SensorComponent : public Component {
public:
    OutputPort<Serializer::ByteArray> telemetry_out_ground;
    OutputPort<EventPacket>           event_out;
    InputPort<CommandPacket>          cmd_in;

    SensorComponent(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    void init() override {
        // Register param with default — will load from ParamDb if already saved
        ParamDb::getInstance().getParam(PARAM_STAR_AMPLITUDE, 10.0f);
    }

    void step() override {
        // 1. Drain all pending commands first
        CommandPacket cmd{};
        while (cmd_in.tryConsume(cmd)) {
            handleCommand(cmd);
        }

        // 2. Sensor simulation
        float amplitude = ParamDb::getInstance().getParam(PARAM_STAR_AMPLITUDE, 10.0f);
        angle += 0.1f;
        float reading = std::sin(angle) * amplitude;

        TelemetryPacket p{ TimeService::getMET(), getId(), reading };
        telemetry_out_ground.send(Serializer::pack(p));
    }

private:
    float angle{0.0f};

    void handleCommand(const CommandPacket& cmd) {
        switch (cmd.opcode) {
            case 1: // SET_AMPLITUDE
                ParamDb::getInstance().setParam(PARAM_STAR_AMPLITUDE, cmd.argument);
                event_out.send(EventPacket::create(Severity::INFO, getId(),
                    "STAR: Amplitude updated"));
                std::cout << "[" << getName() << "] Amplitude set to "
                          << cmd.argument << "\n";
                break;
            default:
                recordError(); // Unknown opcode — FDIR visible
                event_out.send(EventPacket::create(Severity::WARNING, getId(),
                    "STAR: Unknown opcode"));
                break;
        }
    }
};

} // namespace deltav