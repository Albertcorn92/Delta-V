#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Types.hpp"
#include <array>
#include <iostream>

namespace deltav {

// Define strict, compile-time memory boundaries
constexpr size_t MAX_EVENT_SOURCES = 32;

class EventHub : public Component {
public:
    EventHub(std::string_view name, uint32_t id) : Component(name, id) {}

    // Input ports for components to "shout" events
    std::array<InputPort<EventPacket>*, MAX_EVENT_SOURCES> inputs;
    size_t input_count = 0;

    // Output to the Radio/Bridge
    OutputPort<EventPacket> event_out;

    void init() override {}

    void step() override {
        // Iterate only over registered event sources
        for (size_t i = 0; i < input_count; ++i) {
            if (inputs[i]->hasNew()) {
                event_out.send(inputs[i]->consume());
            }
        }
    }

    void registerEventSource(InputPort<EventPacket>* port) {
        if (input_count < MAX_EVENT_SOURCES) {
            inputs[input_count++] = port;
        } else {
            std::cerr << "[FATAL] " << getName() << " exceeded MAX_EVENT_SOURCES boundary. Refusing source." << std::endl;
        }
    }
};

} // namespace deltav