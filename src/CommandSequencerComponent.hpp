#pragma once
// =============================================================================
// CommandSequencerComponent.hpp — DELTA-V Timeline Command Sequencer
// =============================================================================
// Civilian operations helper:
// - Queues deferred CommandPacket dispatches for future MET timestamps.
// - Uses fixed-size storage (no runtime heap).
// - Designed for mission scripts, checkout timelines, and training scenarios.
// =============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace deltav {

class CommandSequencerComponent final : public Component {
public:
    static constexpr size_t MAX_SCHEDULED = 16;

    // Sequencer command opcodes (target: this component ID)
    static constexpr uint32_t OP_SET_TARGET = 1;         // arg: target_id
    static constexpr uint32_t OP_SET_OPCODE = 2;         // arg: opcode
    static constexpr uint32_t OP_SET_ARGUMENT = 3;       // arg: command argument
    static constexpr uint32_t OP_COMMIT_DELAY_SECONDS = 4; // arg: delay seconds
    static constexpr uint32_t OP_CLEAR_QUEUE = 5;        // arg ignored

    InputPort<CommandPacket>          cmd_in{};
    OutputPort<CommandPacket>         command_out{};
    OutputPort<Serializer::ByteArray> telemetry_out{};
    OutputPort<EventPacket>           event_out{};

    CommandSequencerComponent(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    auto init() -> void override {
        staged_command_.target_id = 200U;
        staged_command_.opcode = 1U;
        staged_command_.argument = 0.0f;
        publishEvent(Severity::INFO, "SEQ_READY");
    }

    auto step() -> void override {
        CommandPacket cmd{};
        while (cmd_in.tryConsume(cmd)) {
            handleCommand(cmd);
        }

        const uint32_t now_ms = TimeService::getMET();
        for (auto& entry : queue_) {
            if (!entry.active || !isDue(now_ms, entry.release_met_ms)) {
                continue;
            }

            if (!command_out.send(entry.command)) {
                recordError();
                publishEvent(Severity::WARNING, "SEQ_SEND_FAIL");
            } else {
                publishEvent(Severity::INFO, "SEQ_SENT");
            }
            entry.active = false;
        }

        const TelemetryPacket tlm{
            TimeService::getMET(),
            getId(),
            static_cast<float>(queuedCount()),
        };
        (void)telemetry_out.send(Serializer::pack(tlm));
    }

    auto scheduleCommandInMs(const CommandPacket& cmd, uint32_t delay_ms) -> bool {
        for (auto& entry : queue_) {
            if (entry.active) {
                continue;
            }
            entry.active = true;
            entry.command = cmd;
            entry.release_met_ms = TimeService::getMET() + delay_ms;
            return true;
        }
        recordError();
        publishEvent(Severity::WARNING, "SEQ_FULL");
        return false;
    }

private:
    struct ScheduledEntry {
        bool active{false};
        uint32_t release_met_ms{0};
        CommandPacket command{};
    };

    std::array<ScheduledEntry, MAX_SCHEDULED> queue_{};
    CommandPacket staged_command_{};

    auto queuedCount() const -> size_t {
        size_t active = 0;
        for (const auto& entry : queue_) {
            if (entry.active) {
                ++active;
            }
        }
        return active;
    }

    auto clearQueue() -> void {
        for (auto& entry : queue_) {
            entry.active = false;
        }
    }

    auto handleCommand(const CommandPacket& cmd) -> void {
        switch (cmd.opcode) {
        case OP_SET_TARGET:
            staged_command_.target_id = toNonNegativeUint(cmd.argument);
            publishEvent(Severity::INFO, "SEQ_TARGET_SET");
            break;
        case OP_SET_OPCODE:
            staged_command_.opcode = toNonNegativeUint(cmd.argument);
            publishEvent(Severity::INFO, "SEQ_OPCODE_SET");
            break;
        case OP_SET_ARGUMENT:
            staged_command_.argument = cmd.argument;
            publishEvent(Severity::INFO, "SEQ_ARG_SET");
            break;
        case OP_COMMIT_DELAY_SECONDS: {
            const float delay_s = (!std::isfinite(cmd.argument) || cmd.argument < 0.0F)
                ? 0.0F
                : cmd.argument;
            const float delay_ms_f = delay_s * 1000.0F;
            const uint32_t delay_ms = delay_ms_f >= 4294967295.0F
                ? 0xFFFFFFFFU
                : static_cast<uint32_t>(delay_ms_f);
            if (!scheduleCommandInMs(staged_command_, delay_ms)) {
                publishEvent(Severity::WARNING, "SEQ_COMMIT_FAIL");
            } else {
                publishEvent(Severity::INFO, "SEQ_COMMITTED");
            }
            break;
        }
        case OP_CLEAR_QUEUE:
            clearQueue();
            publishEvent(Severity::INFO, "SEQ_CLEARED");
            break;
        default:
            recordError();
            publishEvent(Severity::WARNING, "SEQ_BAD_OPCODE");
            break;
        }
    }

    static auto toNonNegativeUint(float value) -> uint32_t {
        if (!std::isfinite(value) || value <= 0.0F) {
            return 0U;
        }
        if (value >= 4294967295.0F) {
            return 0xFFFFFFFFU;
        }
        return static_cast<uint32_t>(value);
    }

    static auto isDue(uint32_t now_ms, uint32_t release_ms) -> bool {
        // Wrap-safe tick compare: due when now-release is in the forward half-range.
        return (now_ms - release_ms) < 0x80000000U;
    }

    auto publishEvent(uint32_t severity, const char* msg) -> void {
        (void)event_out.send(EventPacket::create(severity, getId(), msg));
    }
};

} // namespace deltav
