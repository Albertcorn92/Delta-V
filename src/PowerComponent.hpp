#pragma once
// =============================================================================
// PowerComponent.hpp — DELTA-V Battery / Power Management
// =============================================================================
// Fixes over previous version:
//   - timestamp_ms was hardcoded to 0 — now uses TimeService::getMET()
//   - cmd_in fully drained per tick
//   - Opcode 2 (SET_DRAIN_RATE) implemented (was in dictionary but unhandled)
//   - recordError() used for invalid commands (FDIR visibility)
// =============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <iostream>

namespace deltav {

class PowerComponent : public Component {
public:
    PowerComponent(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    OutputPort<Serializer::ByteArray> telemetry_out;
    OutputPort<float>                 battery_out_internal;
    OutputPort<EventPacket>           event_out;
    InputPort<CommandPacket>          cmd_in;

    void init() override {
        soc       = 100.0f;
        drain_rate = 0.1f;
    }

    void step() override {
        // 1. Drain all pending commands
        CommandPacket cmd{};
        while (cmd_in.tryConsume(cmd)) {
            handleCommand(cmd);
        }

        // 2. Discharge simulation
        if (soc > 0.0f) {
            soc = std::max(0.0f, soc - drain_rate);
        }

        // 3. Telemetry — timestamp was previously hardcoded to 0, now fixed
        TelemetryPacket p{ TimeService::getMET(), getId(), soc };
        telemetry_out.send(Serializer::pack(p));
        battery_out_internal.send(soc);
    }

    float getSOC() const { return soc; }

private:
    float soc{100.0f};
    float drain_rate{0.1f};

    void handleCommand(const CommandPacket& cmd) {
        switch (cmd.opcode) {
            case 1: // RESET_BATTERY
                soc = 100.0f;
                event_out.send(EventPacket::create(Severity::INFO, getId(),
                    "BATT: SOC reset to 100%"));
                std::cout << "[" << getName() << "] SOC reset to 100%\n";
                break;

            case 2: // SET_DRAIN_RATE
                if (cmd.argument >= 0.0f && cmd.argument <= 10.0f) {
                    drain_rate = cmd.argument;
                    event_out.send(EventPacket::create(Severity::INFO, getId(),
                        "BATT: Drain rate updated"));
                    std::cout << "[" << getName() << "] Drain rate set to "
                              << cmd.argument << "%/tick\n";
                } else {
                    recordError();
                    event_out.send(EventPacket::create(Severity::WARNING, getId(),
                        "BATT: Invalid drain rate arg"));
                }
                break;

            default:
                recordError();
                event_out.send(EventPacket::create(Severity::WARNING, getId(),
                    "BATT: Unknown opcode"));
                break;
        }
    }
};

} // namespace deltav