#pragma once
// =============================================================================
// TelemHub.hpp — DELTA-V Telemetry Broadcast Bus
// =============================================================================
// Central nervous system. Accepts telemetry from multiple sensor OutputPorts
// and broadcasts each packet to all registered listeners simultaneously.
//
// Architecture: N inputs → internal InputPorts → N listener OutputPorts.
// No dynamic allocation. All connections made at boot during wiring phase.
// =============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include <array>

namespace deltav {

constexpr size_t MAX_TELEM_INPUTS    = 32;
constexpr size_t MAX_TELEM_LISTENERS = 8;

class TelemHub : public Component {
public:
    TelemHub(std::string_view comp_name, uint32_t comp_id) : Component(comp_name, comp_id) {}

    void init() override {}

    void step() override {
        Serializer::ByteArray data{};
        for (size_t i = 0; i < input_count; ++i) {
            // Drain all queued packets from this input
            while (internal_inputs[i].tryConsume(data)) {
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

    void registerListener(InputPort<Serializer::ByteArray>* dest) {
        if (listener_count < MAX_TELEM_LISTENERS) {
            internal_outputs[listener_count].connect(dest);
            ++listener_count;
        }
    }

private:
    std::array<InputPort<Serializer::ByteArray>, MAX_TELEM_INPUTS>     internal_inputs{};
    std::array<OutputPort<Serializer::ByteArray>, MAX_TELEM_LISTENERS> internal_outputs{};
    size_t input_count{0};
    size_t listener_count{0};
};

} // namespace deltav