#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include <vector>

namespace deltav {

// This component acts like a network switch for your satellite
class TelemHub : public Component {
public:
    TelemHub(std::string_view name, uint32_t id) : Component(name, id) {}

    void init() override {}

    void step() override {
        // Collect from all registered input ports and push to the one output
        for (auto* port : inputs) {
            if (port->hasNew()) {
                output.send(port->consume());
            }
        }
    }

    // A helper to let developers easily "plug in" to the hub
    void registerInput(InputPort<Serializer::ByteArray>* port) {
        inputs.push_back(port);
    }

    OutputPort<Serializer::ByteArray> output;

private:
    std::vector<InputPort<Serializer::ByteArray>*> inputs;
};

}