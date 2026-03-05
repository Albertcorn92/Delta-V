#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Types.hpp"
#include <array>
#include <iostream>

namespace deltav {

// Define strict, compile-time memory boundaries
constexpr size_t MAX_COMMAND_ROUTES = 32;

struct RouteEntry {
    uint32_t comp_id = 0;
    OutputPort<CommandPacket>* port = nullptr;
};

class CommandHub : public Component {
public:
    CommandHub(std::string_view name, uint32_t id) : Component(name, id) {}

    // Input from the outside world (Radio/Bridge)
    InputPort<CommandPacket> cmd_input;

    void init() override {}

    void step() override {
        if (cmd_input.hasNew()) {
            CommandPacket cmd = cmd_input.consume();

            // Linear search through contiguous memory to find the designated route
            for (size_t i = 0; i < route_count; ++i) {
                if (routes[i].comp_id == cmd.target_id && routes[i].port != nullptr) {
                    routes[i].port->send(cmd);
                    break; // Command routed, stop searching
                }
            }
        }
    }

    // Helper to wire components to the hub
    void registerRoute(uint32_t comp_id, OutputPort<CommandPacket>* port) {
        if (route_count < MAX_COMMAND_ROUTES) {
            routes[route_count].comp_id = comp_id;
            routes[route_count].port = port;
            route_count++;
        } else {
            std::cerr << "[FATAL] " << getName() << " exceeded MAX_COMMAND_ROUTES boundary. Refusing route." << std::endl;
        }
    }

private:
    std::array<RouteEntry, MAX_COMMAND_ROUTES> routes;
    size_t route_count = 0;
};

} // namespace deltav