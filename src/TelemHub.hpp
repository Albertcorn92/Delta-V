#pragma once
// =============================================================================
// TelemHub.hpp — DELTA-V Telemetry Broadcast Bus
// =============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include <array>
#include <cstdio>
#include <string_view>

namespace deltav {

constexpr size_t MAX_TELEM_INPUTS    = 32;
constexpr size_t MAX_TELEM_LISTENERS = 8;

class TelemHub : public Component {
public:
    /* NOLINTNEXTLINE(bugprone-easily-swappable-parameters) */
    TelemHub(std::string_view comp_name, uint32_t comp_id) : Component(comp_name, comp_id) {}

    auto init() -> void override {}

    auto step() -> void override {
        Serializer::ByteArray data{};
        for (size_t i = 0; i < input_count; ++i) {
            while (internal_inputs.at(i).tryConsume(data)) {
                for (size_t j = 0; j < listener_count; ++j) {
                    if (!internal_outputs.at(j).send(data)) {
                        recordError();
                    }
                }
            }
        }
    }

    auto connectInput(OutputPort<Serializer::ByteArray>& source) -> void {
        if (input_count >= MAX_TELEM_INPUTS) {
            recordError();
            (void)std::fprintf(stderr,
                "[TelemHub] WARN: exceeded MAX_TELEM_INPUTS (%zu)\n",
                static_cast<size_t>(MAX_TELEM_INPUTS));
            return;
        }
        source.connect(&internal_inputs.at(input_count++));
    }

    auto registerListener(InputPort<Serializer::ByteArray>* dest) -> void {
        if (dest == nullptr) {
            recordError();
            (void)std::fprintf(stderr, "[TelemHub] WARN: rejected null listener\n");
            return;
        }
        if (listener_count >= MAX_TELEM_LISTENERS) {
            recordError();
            (void)std::fprintf(stderr,
                "[TelemHub] WARN: exceeded MAX_TELEM_LISTENERS (%zu)\n",
                static_cast<size_t>(MAX_TELEM_LISTENERS));
            return;
        }
        internal_outputs.at(listener_count).connect(dest);
        ++listener_count;
    }

    [[nodiscard]] auto getInputCount() const -> size_t { return input_count; }
    [[nodiscard]] auto getListenerCount() const -> size_t { return listener_count; }
    [[nodiscard]] auto isInputConnected(
        size_t index,
        const OutputPort<Serializer::ByteArray>& source) const -> bool {
        return index < input_count && source.isConnectedTo(&internal_inputs.at(index));
    }

private:
    std::array<InputPort<Serializer::ByteArray>, MAX_TELEM_INPUTS>     internal_inputs{};
    std::array<OutputPort<Serializer::ByteArray>, MAX_TELEM_LISTENERS> internal_outputs{};
    size_t input_count{0};
    size_t listener_count{0};
};

} // namespace deltav
