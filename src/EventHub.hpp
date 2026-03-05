#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Types.hpp"
#include <vector>

namespace deltav {

class EventHub : public Component {
public:
    EventHub(std::string_view name, uint32_t id) : Component(name, id) {}

    // Input ports for components to "shout" events
    std::vector<InputPort<EventPacket>*> inputs;
    
    // Output to the Radio/Bridge
    OutputPort<EventPacket> event_out;

    void init() override {}

    void step() override {
        for (auto* port : inputs) {
            if (port->hasNew()) {
                event_out.send(port->consume());
            }
        }
    }

    void registerEventSource(InputPort<EventPacket>* port) {
        inputs.push_back(port);
    }
};

} // namespace deltav