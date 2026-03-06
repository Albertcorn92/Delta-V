#pragma once
// =============================================================================
// CommandHub.hpp — DELTA-V Command Router  v3.0
// =============================================================================
// Changes from v2.x:
//   - MissionFsm gating: commands blocked by current MissionState (DV-SEC-02)
//   - setMissionStateSource(): CommandHub holds a pointer to the Watchdog so
//     it can check getMissionStateRaw() before dispatching each command.
//   - If the FSM blocks a command, a NACK with "FSM_BLOCKED" is returned and
//     recordError() is called for FDIR visibility.
//
// Fixes retained from v2.1:
//   F-07: ack_out.send() return value checked.
// =============================================================================
#include "Component.hpp"
#include "MissionFsm.hpp"
#include "Port.hpp"
#include "Types.hpp"
#include "WatchdogComponent.hpp"
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
    CommandHub(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    InputPort<CommandPacket>  cmd_input;
    OutputPort<EventPacket>   ack_out;

    void init() override {}

    void step() override {
        CommandPacket cmd{};
        while (cmd_input.tryConsume(cmd)) {
            dispatchCommand(cmd);
        }
    }

    void registerRoute(uint32_t comp_id, OutputPort<CommandPacket>* port) {
        if (port == nullptr) {
            recordError();
            return;
        }

        for (size_t i = 0; i < route_count; ++i) {
            if (routes.at(i).comp_id == comp_id) {
                recordError();
                return;
            }
        }

        if (route_count < MAX_COMMAND_ROUTES) {
            routes.at(route_count++) = { comp_id, port };
        } else {
            recordError();
        }
    }

    // Wire in the Watchdog so FSM state can be checked per-command (DV-SEC-02).
    // Call this from TopologyManager::wire() after constructing both objects.
    void setMissionStateSource(WatchdogComponent* wd) { watchdog = wd; }

    [[nodiscard]] auto routeCount() const -> size_t { return route_count; }
    [[nodiscard]] auto hasMissionStateSource() const -> bool { return watchdog != nullptr; }

private:
    std::array<RouteEntry, MAX_COMMAND_ROUTES> routes{};
    size_t              route_count{0};
    WatchdogComponent*  watchdog{nullptr};

    void dispatchCommand(const CommandPacket& cmd) {
        // --- DV-SEC-02: FSM gating ---
        if (watchdog != nullptr) {
            uint8_t state_raw = watchdog->getMissionStateRaw();
            if (!MissionFsm::isAllowed(state_raw, cmd.opcode)) {
                char msg[28];
                (void)std::snprintf(msg, sizeof(msg), "FSM_BLOCKED: OP%u in %s",
                    cmd.opcode, MissionFsm::stateName(state_raw));
                if (!ack_out.send(EventPacket::create(Severity::WARNING, getId(), msg))) {
                    recordError();
                }
                recordError(); // FDIR: blocked command is a notable event
                return;
            }
        }

        for (size_t i = 0; i < route_count; ++i) {
            if (routes.at(i).comp_id == cmd.target_id) {
                routes.at(i).port->send(cmd);
                char msg[28];
                (void)std::snprintf(msg, sizeof(msg), "ACK: OP%u->ID%u",
                    cmd.opcode, cmd.target_id);
                if (!ack_out.send(EventPacket::create(Severity::INFO, getId(), msg))) {
                    recordError();
                }
                return;
            }
        }
        // Unknown target
        char msg[28];
        (void)std::snprintf(msg, sizeof(msg), "NACK: ID%u not found", cmd.target_id);
        if (!ack_out.send(EventPacket::create(Severity::WARNING, getId(), msg))) {
            recordError();
        }
        recordError(); // DV-FDIR-08
    }

};

} // namespace deltav
