#pragma once
// =============================================================================
// PayloadMonitorComponent.hpp — DELTA-V Reference Payload Profile
// =============================================================================
// Public reference payload for low-rate civilian technology-demonstration work.
// The component is intentionally simple: it supports payload enable/disable,
// bounded gain adjustment, and a restricted sample-capture command.
// =============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <algorithm>
#include <cstdio>

namespace deltav {

class PayloadMonitorComponent : public Component {
public:
    static constexpr uint32_t OP_SET_ENABLE = 1u;
    static constexpr uint32_t OP_CAPTURE_SAMPLE = 2u;
    static constexpr uint32_t OP_SET_GAIN = 3u;

    PayloadMonitorComponent(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    OutputPort<Serializer::ByteArray> telemetry_out{};
    OutputPort<EventPacket> event_out{};
    InputPort<CommandPacket> cmd_in{};

    auto init() -> void override {
        enabled_ = false;
        gain_ = 1.0f;
        last_sample_ = 0.0f;
        capture_count_ = 0u;
    }

    auto step() -> void override {
        CommandPacket cmd{};
        while (cmd_in.tryConsume(cmd)) {
            handleCommand(cmd);
        }

        const float health = enabled_ ? last_sample_ : 0.0f;
        (void)sendOrRecordError(telemetry_out, Serializer::pack(
            TelemetryPacket{TimeService::getMET(), getId(), health}));
    }

    [[nodiscard]] auto isEnabledForTest() const -> bool { return enabled_; }
    [[nodiscard]] auto gainForTest() const -> float { return gain_; }
    [[nodiscard]] auto lastSampleForTest() const -> float { return last_sample_; }
    [[nodiscard]] auto captureCountForTest() const -> uint32_t { return capture_count_; }

private:
    bool enabled_{false};
    float gain_{1.0f};
    float last_sample_{0.0f};
    uint32_t capture_count_{0u};

    auto handleCommand(const CommandPacket& cmd) -> void {
        switch (cmd.opcode) {
            case OP_SET_ENABLE:
                enabled_ = (cmd.argument >= 0.5f);
                (void)sendOrRecordError(event_out, EventPacket::create(
                    Severity::INFO, getId(),
                    enabled_ ? "PAYLOAD: Enabled" : "PAYLOAD: Disabled"));
                break;

            case OP_CAPTURE_SAMPLE:
                if (!enabled_) {
                    recordError();
                    (void)sendOrRecordError(event_out, EventPacket::create(
                        Severity::WARNING, getId(),
                        "PAYLOAD: Capture rejected (disabled)"));
                    return;
                }
                last_sample_ = std::clamp(cmd.argument, -100.0f, 100.0f) * gain_;
                ++capture_count_;
                (void)sendOrRecordError(event_out, EventPacket::create(
                    Severity::INFO, getId(), "PAYLOAD: Sample captured"));
                {
                    const auto name = getName();
                    std::printf("[%.*s] Sample captured value=%.3f count=%u\n",
                        static_cast<int>(name.size()), name.data(),
                        static_cast<double>(last_sample_),
                        static_cast<unsigned>(capture_count_));
                }
                break;

            case OP_SET_GAIN:
                if (cmd.argument < 0.1f || cmd.argument > 10.0f) {
                    recordError();
                    (void)sendOrRecordError(event_out, EventPacket::create(
                        Severity::WARNING, getId(),
                        "PAYLOAD: Invalid gain"));
                    return;
                }
                gain_ = cmd.argument;
                (void)sendOrRecordError(event_out, EventPacket::create(
                    Severity::INFO, getId(), "PAYLOAD: Gain updated"));
                break;

            default:
                recordError();
                (void)sendOrRecordError(event_out, EventPacket::create(
                    Severity::WARNING, getId(), "PAYLOAD: Unknown opcode"));
                break;
        }
    }
};

} // namespace deltav
