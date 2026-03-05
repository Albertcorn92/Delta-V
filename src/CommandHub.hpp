#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Types.hpp"
#include <array>
#include <iostream>
#include <cstdio>

namespace deltav {

constexpr size_t MAX_COMMAND_ROUTES = 32;

struct RouteEntry {
    uint32_t comp_id = 0;
    OutputPort<CommandPacket>* port = nullptr;
};

class CommandHub : public Component {
public:
    CommandHub(std::string_view name, uint32_t id) : Component(name, id) {}

    InputPort<CommandPacket> cmd_input;
    OutputPort<EventPacket> ack_out; 

    void init() override {}

    void step() override {
        if (cmd_input.hasNew()) {
            CommandPacket cmd = cmd_input.consume();
            bool routed = false;

            for (size_t i = 0; i < route_count; ++i) {
                if (routes[i].comp_id == cmd.target_id) {
                    routes[i].port->send(cmd);
                    routed = true;
                    break;
                }
            }

            char msg[32];
            if (routed) {
                std::snprintf(msg, sizeof(msg), "ACK: CMD %u to ID %u", cmd.opcode, cmd.target_id);
                ack_out.send(EventPacket::create(1, msg));
            } else {
                std::snprintf(msg, sizeof(msg), "NACK: ID %u Not Found", cmd.target_id);
                ack_out.send(EventPacket::create(2, msg));
            }
        }
    }

    void registerRoute(uint32_t comp_id, OutputPort<CommandPacket>* port) {
        if (route_count < MAX_COMMAND_ROUTES) {
            routes[route_count].comp_id = comp_id;
            routes[route_count].port = port;
            route_count++;
        }
    }

private:
    std::array<RouteEntry, MAX_COMMAND_ROUTES> routes;
    size_t route_count = 0;
};

} // namespace deltav