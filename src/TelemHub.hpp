#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include <array>

namespace deltav {

constexpr size_t MAX_TELEM_INPUTS = 32;
constexpr size_t MAX_TELEM_LISTENERS = 8;

class TelemHub : public Component {
public:
    TelemHub(std::string_view name, uint32_t id) : Component(name, id) {}

    void init() override {}

    void step() override {
        for (size_t i = 0; i < input_count; ++i) {
            if (internal_inputs[i].hasNew()) {
                auto data = internal_inputs[i].consume();
                
                // 🚀 FIX: Broadcast to all connected listeners using OutputPorts
                for (size_t j = 0; j < listener_count; ++j) {
                    internal_outputs[j].send(data);
                }
            }
        }
    }

    void connectInput(OutputPort<Serializer::ByteArray>& source) {
        if (input_count < MAX_TELEM_INPUTS) {
            source.connect(&internal_inputs[input_count++]);
        }
    }

    // 🚀 FIX: Establish a real Port-to-Port connection
    void registerListener(InputPort<Serializer::ByteArray>* dest) {
        if (listener_count < MAX_TELEM_LISTENERS) {
            internal_outputs[listener_count].connect(dest);
            listener_count++;
        }
    }

private:
    std::array<InputPort<Serializer::ByteArray>, MAX_TELEM_INPUTS> internal_inputs;
    // 🚀 NEW: Internal outputs to drive the listeners
    std::array<OutputPort<Serializer::ByteArray>, MAX_TELEM_LISTENERS> internal_outputs;
    
    size_t input_count = 0;
    size_t listener_count = 0;
};
}