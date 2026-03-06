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
#include <cstdio>
#include <string_view>

namespace deltav {

// Compile-time param ID — no string hashing at runtime
constexpr uint32_t PARAM_STAR_AMPLITUDE = ParamDb::fnv1a("star_amplitude");

// DO-178C: Named constants for simulation parameters
constexpr float DEFAULT_STAR_AMPLITUDE = 10.0f;
constexpr float ANGLE_STEP_RADS        = 0.1f;

class SensorComponent : public Component {
public:
    // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
    OutputPort<Serializer::ByteArray> telemetry_out{};
    OutputPort<EventPacket>           event_out{};
    InputPort<CommandPacket>          cmd_in{};
    // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

    /* NOLINTNEXTLINE(bugprone-easily-swappable-parameters) */
    SensorComponent(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    auto init() -> void override {
        // Register param with default — will load from ParamDb if already saved
        ParamDb::getInstance().getParam(PARAM_STAR_AMPLITUDE, DEFAULT_STAR_AMPLITUDE);
    }

    auto step() -> void override {
        // 1. Drain all pending commands first
        CommandPacket cmd{};
        while (cmd_in.tryConsume(cmd)) {
            handleCommand(cmd);
        }

        // 2. Sensor simulation
        float amplitude = ParamDb::getInstance().getParam(PARAM_STAR_AMPLITUDE, DEFAULT_STAR_AMPLITUDE);
        angle += ANGLE_STEP_RADS;
        
        // DO-178C: Zero-initialized at declaration
        const float reading = std::sin(angle) * amplitude;

        TelemetryPacket p{ TimeService::getMET(), getId(), reading };
        telemetry_out.send(Serializer::pack(p));
    }

private:
    float angle{0.0f};

    auto handleCommand(const CommandPacket& cmd) -> void {
        switch (cmd.opcode) {
            case 1: // SET_AMPLITUDE
                ParamDb::getInstance().setParam(PARAM_STAR_AMPLITUDE, cmd.argument);
                event_out.send(EventPacket::create(Severity::INFO, getId(),
                    "STAR: Amplitude updated"));
                {
                    const auto name = getName();
                    std::printf("[%.*s] Amplitude set to %.3f\n",
                        static_cast<int>(name.size()), name.data(),
                        static_cast<double>(cmd.argument));
                }
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
