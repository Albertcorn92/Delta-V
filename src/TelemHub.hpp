#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include <vector>
#include <memory>

namespace deltav {

class TelemHub : public Component {
public:
    TelemHub(std::string_view name, uint32_t id) : Component(name, id) {}

    void init() override {}

    void step() override {
        for (auto& port : internal_inputs) {
            if (port->hasNew()) {
                auto data = port->consume();
                // Push data to every registered listener's InputPort
                for (auto* listener : listeners) {
                    listener->receive(data);
                }
            }
        }
    }

    // Connects a sensor to the Hub using stable heap memory
    void connectInput(OutputPort<Serializer::ByteArray>& source) {
        auto new_port = std::make_unique<InputPort<Serializer::ByteArray>>();
        source.connect(new_port.get());
        internal_inputs.push_back(std::move(new_port));
    }

    // Registers external components (Radio, Logger) to receive the stream
    void registerListener(InputPort<Serializer::ByteArray>* dest) {
        listeners.push_back(dest);
    }

private:
    std::vector<std::unique_ptr<InputPort<Serializer::ByteArray>>> internal_inputs;
    std::vector<InputPort<Serializer::ByteArray>*> listeners;
};

} // namespace deltav