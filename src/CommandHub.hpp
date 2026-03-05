#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Types.hpp"
#include <map>

namespace deltav {

class CommandHub : public Component {
public:
    CommandHub(std::string_view name, uint32_t id) : Component(name, id) {}

    // Input from the outside world (Radio/Bridge)
    InputPort<CommandPacket> cmd_input;

    void init() override {}

    void step() override {
        if (cmd_input.hasNew()) {
            CommandPacket cmd = cmd_input.consume();
            
            // Route the command if a destination port exists
            if (routes.count(cmd.target_id)) {
                routes[cmd.target_id]->send(cmd);
            }
        }
    }

    // Helper to wire components to the hub
    void registerRoute(uint32_t comp_id, OutputPort<CommandPacket>* port) {
        routes[comp_id] = port;
    }

private:
    std::map<uint32_t, OutputPort<CommandPacket>*> routes;
};

} // namespace deltav