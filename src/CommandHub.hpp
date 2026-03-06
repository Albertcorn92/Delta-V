#pragma once
// =============================================================================
// CommandHub.hpp — DELTA-V Command Router
// =============================================================================
// Receives CommandPackets from TelemetryBridge and routes them by target_id
// to the registered component OutputPort.
//
// Fixes over previous version:
//   - Drains ALL queued commands per tick (not just one)
//   - ACK/NACK EventPackets include source_id for GDS correlation
//   - Route table is populated at boot and never modified at runtime
// =============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Types.hpp"
#include <array>
#include <cstdio>

namespace deltav {

constexpr size_t MAX_COMMAND_ROUTES = 32;

struct RouteEntry {
    uint32_t                  comp_id{0};
    OutputPort<CommandPacket>* port{nullptr};
};

class CommandHub : public Component {
public:
    CommandHub(std::string_view comp_name, uint32_t comp_id) : Component(comp_name, comp_id) {}

    InputPort<CommandPacket>  cmd_input;
    OutputPort<EventPacket>   ack_out;

    void init() override {}

    void step() override {
        // Drain ALL pending commands this tick (not just one)
        CommandPacket cmd{};
        while (cmd_input.tryConsume(cmd)) {
            dispatchCommand(cmd);
        }
    }

    void registerRoute(uint32_t comp_id, OutputPort<CommandPacket>* port) {
        if (route_count < MAX_COMMAND_ROUTES) {
            routes[route_count++] = { comp_id, port };
        }
    }

private:
    std::array<RouteEntry, MAX_COMMAND_ROUTES> routes{};
    size_t route_count{0};

    void dispatchCommand(const CommandPacket& cmd) {
        for (size_t i = 0; i < route_count; ++i) {
            if (routes[i].comp_id == cmd.target_id) {
                routes[i].port->send(cmd);
                char msg[28];
                std::snprintf(msg, sizeof(msg), "ACK: OP%u->ID%u",
                    cmd.opcode, cmd.target_id);
                ack_out.send(EventPacket::create(Severity::INFO, getId(), msg));
                return;
            }
        }
        char msg[28];
        std::snprintf(msg, sizeof(msg), "NACK: ID%u not found", cmd.target_id);
        ack_out.send(EventPacket::create(Severity::WARNING, getId(), msg));
        recordError(); // FDIR: unroutable command counts as an error
    }
};

} // namespace deltav