#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include <array>
#include <iostream>

namespace deltav {

// Define strict, compile-time memory boundaries
constexpr size_t MAX_TELEM_INPUTS = 32;
constexpr size_t MAX_TELEM_LISTENERS = 8;

class TelemHub : public Component {
public:
    TelemHub(std::string_view name, uint32_t id) : Component(name, id) {}

    void init() override {}

    void step() override {
        // Iterate only over the active, registered ports
        for (size_t i = 0; i < input_count; ++i) {
            if (internal_inputs[i].hasNew()) {
                auto data = internal_inputs[i].consume();
                
                // Broadcast to all registered listeners (Radio, Logger, etc.)
                for (size_t j = 0; j < listener_count; ++j) {
                    listeners[j]->receive(data);
                }
            }
        }
    }

    // Connects a sensor to the Hub using pre-allocated contiguous memory
    void connectInput(OutputPort<Serializer::ByteArray>& source) {
        if (input_count < MAX_TELEM_INPUTS) {
            source.connect(&internal_inputs[input_count]);
            input_count++;
        } else {
            std::cerr << "[FATAL] " << getName() << " exceeded MAX_TELEM_INPUTS boundary. Refusing connection." << std::endl;
        }
    }

    // Registers external components to receive the stream
    void registerListener(InputPort<Serializer::ByteArray>* dest) {
        if (listener_count < MAX_TELEM_LISTENERS) {
            listeners[listener_count++] = dest;
        } else {
            std::cerr << "[FATAL] " << getName() << " exceeded MAX_TELEM_LISTENERS boundary. Refusing listener." << std::endl;
        }
    }

private:
    // Zero dynamic memory allocation. All ports exist in a fixed memory footprint at boot.
    std::array<InputPort<Serializer::ByteArray>, MAX_TELEM_INPUTS> internal_inputs;
    std::array<InputPort<Serializer::ByteArray>*, MAX_TELEM_LISTENERS> listeners;
    
    size_t input_count = 0;
    size_t listener_count = 0;
};

} // namespace deltav