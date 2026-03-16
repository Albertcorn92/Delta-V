#pragma once
// =============================================================================
// TimeSyncComponent.hpp — DELTA-V Ground Time Synchronization Service
// =============================================================================
// Civilian operations helper:
// - Accepts staged UTC word fields from command path.
// - Applies UTC offset to TimeService on command.
// - Emits sync status telemetry and events for operator visibility.
// =============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <array>
#include <cmath>
#include <cstdint>

namespace deltav {

class TimeSyncComponent final : public Component {
public:
    // target_id: this component
    static constexpr uint32_t OP_SET_UTC_HI16 = 1;
    static constexpr uint32_t OP_SET_UTC_MIDHI16 = 2;
    static constexpr uint32_t OP_SET_UTC_MIDLO16 = 3;
    static constexpr uint32_t OP_SET_UTC_LO16 = 4;
    static constexpr uint32_t OP_APPLY_SYNC = 5;
    static constexpr uint32_t OP_PUBLISH_STATUS = 6;

    InputPort<CommandPacket>          cmd_in{};
    OutputPort<Serializer::ByteArray> telemetry_out{};
    OutputPort<EventPacket>           event_out{};

    TimeSyncComponent(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    auto init() -> void override {
        words_.fill(0U);
        publishEvent(Severity::INFO, "TIME_SYNC_READY");
    }

    auto step() -> void override {
        CommandPacket cmd{};
        while (cmd_in.tryConsume(cmd)) {
            handleCommand(cmd);
        }

        const TelemetryPacket tlm{
            TimeService::getMET(),
            getId(),
            static_cast<float>(TimeService::getUtcOffsetMs() / 1000),
        };
        (void)sendOrRecordError(telemetry_out, Serializer::pack(tlm));
    }

private:
    std::array<uint16_t, 4> words_{};

    auto handleCommand(const CommandPacket& cmd) -> void {
        switch (cmd.opcode) {
        case OP_SET_UTC_HI16:
            words_[0] = toU16(cmd.argument);
            publishEvent(Severity::INFO, "TIME_WORD0");
            break;
        case OP_SET_UTC_MIDHI16:
            words_[1] = toU16(cmd.argument);
            publishEvent(Severity::INFO, "TIME_WORD1");
            break;
        case OP_SET_UTC_MIDLO16:
            words_[2] = toU16(cmd.argument);
            publishEvent(Severity::INFO, "TIME_WORD2");
            break;
        case OP_SET_UTC_LO16:
            words_[3] = toU16(cmd.argument);
            publishEvent(Severity::INFO, "TIME_WORD3");
            break;
        case OP_APPLY_SYNC: {
            const uint64_t utc_ms = composeUtcMs();
            TimeService::setUtcFromUnixMs(utc_ms);
            publishEvent(Severity::INFO, "TIME_SYNC_APPLIED");
            break;
        }
        case OP_PUBLISH_STATUS:
            publishEvent(Severity::INFO, TimeService::hasUtcSync() ? "TIME_SYNCED" : "TIME_UNSYNCED");
            break;
        default:
            recordError();
            publishEvent(Severity::WARNING, "TIME_BAD_OPCODE");
            break;
        }
    }

    [[nodiscard]] auto composeUtcMs() const -> uint64_t {
        return (static_cast<uint64_t>(words_[0]) << 48U) |
               (static_cast<uint64_t>(words_[1]) << 32U) |
               (static_cast<uint64_t>(words_[2]) << 16U) |
               static_cast<uint64_t>(words_[3]);
    }

    static auto toU16(float value) -> uint16_t {
        if (!std::isfinite(value) || value < 0.0F) {
            return 0U;
        }
        if (value > 65535.0F) {
            return 65535U;
        }
        return static_cast<uint16_t>(value);
    }

    auto publishEvent(uint32_t severity, const char* msg) -> void {
        (void)sendOrRecordError(event_out, EventPacket::create(severity, getId(), msg));
    }
};

} // namespace deltav
