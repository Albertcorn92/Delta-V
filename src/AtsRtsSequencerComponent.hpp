#pragma once
// =============================================================================
// AtsRtsSequencerComponent.hpp — DELTA-V Absolute/Relative Sequence Engine
// =============================================================================
// Civilian operations helper:
// - ATS: executes commands at staged absolute MET/UTC schedule points.
// - RTS: executes commands after matching event triggers plus relative delay.
// - Uses fixed-size deterministic storage (no runtime heap allocation).
// =============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace deltav {

class AtsRtsSequencerComponent final : public Component {
public:
    static constexpr size_t MAX_SEQUENCE_ENTRIES = 32U;

    enum class TriggerType : uint8_t {
        ABS_MET_MS = 0,
        ABS_UTC_SEC = 1,
        REL_EVENT_DELAY_MS = 2,
    };

    // target_id: this component
    static constexpr uint32_t OP_SET_TARGET = 1;
    static constexpr uint32_t OP_SET_OPCODE = 2;
    static constexpr uint32_t OP_SET_ARGUMENT = 3;
    static constexpr uint32_t OP_SET_TRIGGER_TYPE = 4;
    static constexpr uint32_t OP_SET_TIME_HI16 = 5;
    static constexpr uint32_t OP_SET_TIME_LO16 = 6;
    static constexpr uint32_t OP_SET_EVENT_SOURCE = 7;
    static constexpr uint32_t OP_COMMIT_ENTRY = 8;
    static constexpr uint32_t OP_ARM = 9;
    static constexpr uint32_t OP_DISARM = 10;
    static constexpr uint32_t OP_CLEAR = 11;

    InputPort<CommandPacket>          cmd_in{};
    InputPort<EventPacket>            event_in{};
    OutputPort<CommandPacket>         command_out{};
    OutputPort<Serializer::ByteArray> telemetry_out{};
    OutputPort<EventPacket>           event_out{};

    AtsRtsSequencerComponent(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    auto init() -> void override {
        clearAll();
        staged_cmd_ = CommandPacket{200U, 1U, 0.0F};
        staged_trigger_ = TriggerType::ABS_MET_MS;
        staged_time_hi16_ = 0U;
        staged_time_lo16_ = 0U;
        staged_event_source_ = 0U;
        armed_ = false;
        publishEvent(Severity::INFO, "ATSRTS_READY");
    }

    auto step() -> void override {
        CommandPacket cmd{};
        while (cmd_in.tryConsume(cmd)) {
            handleCommand(cmd);
        }

        EventPacket evt{};
        while (event_in.tryConsume(evt)) {
            onEvent(evt);
        }

        executeDueEntries();

        const TelemetryPacket tlm{
            TimeService::getMET(),
            getId(),
            static_cast<float>(activeCount()),
        };
        (void)telemetry_out.send(Serializer::pack(tlm));
    }

private:
    struct Entry {
        bool active{false};
        bool waiting_event{false};
        TriggerType trigger{TriggerType::ABS_MET_MS};
        uint32_t event_source{0U};
        uint64_t trigger_value{0U}; // MET ms, UTC sec, or delay ms
        CommandPacket cmd{};
    };

    std::array<Entry, MAX_SEQUENCE_ENTRIES> entries_{};
    CommandPacket staged_cmd_{};
    TriggerType staged_trigger_{TriggerType::ABS_MET_MS};
    uint16_t staged_time_hi16_{0U};
    uint16_t staged_time_lo16_{0U};
    uint32_t staged_event_source_{0U};
    bool armed_{false};

    auto handleCommand(const CommandPacket& cmd) -> void {
        switch (cmd.opcode) {
        case OP_SET_TARGET:
            staged_cmd_.target_id = toU32(cmd.argument);
            break;
        case OP_SET_OPCODE:
            staged_cmd_.opcode = toU32(cmd.argument);
            break;
        case OP_SET_ARGUMENT:
            staged_cmd_.argument = cmd.argument;
            break;
        case OP_SET_TRIGGER_TYPE:
            staged_trigger_ = toTriggerType(toU32(cmd.argument));
            break;
        case OP_SET_TIME_HI16:
            staged_time_hi16_ = toU16(cmd.argument);
            break;
        case OP_SET_TIME_LO16:
            staged_time_lo16_ = toU16(cmd.argument);
            break;
        case OP_SET_EVENT_SOURCE:
            staged_event_source_ = toU32(cmd.argument);
            break;
        case OP_COMMIT_ENTRY:
            commitEntry();
            break;
        case OP_ARM:
            armed_ = true;
            publishEvent(Severity::INFO, "ATSRTS_ARM");
            break;
        case OP_DISARM:
            armed_ = false;
            publishEvent(Severity::INFO, "ATSRTS_DISARM");
            break;
        case OP_CLEAR:
            clearAll();
            publishEvent(Severity::INFO, "ATSRTS_CLEAR");
            break;
        default:
            recordError();
            publishEvent(Severity::WARNING, "ATSRTS_BAD_OP");
            break;
        }
    }

    auto commitEntry() -> void {
        Entry* slot = nullptr;
        for (auto& e : entries_) {
            if (!e.active) {
                slot = &e;
                break;
            }
        }
        if (slot == nullptr) {
            recordError();
            publishEvent(Severity::WARNING, "ATSRTS_FULL");
            return;
        }

        const uint32_t time_word = (static_cast<uint32_t>(staged_time_hi16_) << 16U) |
                                   static_cast<uint32_t>(staged_time_lo16_);
        slot->active = true;
        slot->cmd = staged_cmd_;
        slot->trigger = staged_trigger_;
        slot->event_source = staged_event_source_;
        slot->trigger_value = static_cast<uint64_t>(time_word);
        slot->waiting_event = (staged_trigger_ == TriggerType::REL_EVENT_DELAY_MS);
        publishEvent(Severity::INFO, "ATSRTS_COMMIT");
    }

    auto executeDueEntries() -> void {
        if (!armed_) {
            return;
        }

        const uint64_t met_ms = TimeService::getMET64();
        const uint64_t utc_sec = TimeService::getUtcUnixMs() / 1000ULL;

        for (auto& e : entries_) {
            if (!e.active || e.waiting_event) {
                continue;
            }

            bool due = false;
            if (e.trigger == TriggerType::ABS_MET_MS) {
                due = met_ms >= e.trigger_value;
            } else if (e.trigger == TriggerType::ABS_UTC_SEC) {
                due = utc_sec >= e.trigger_value;
            } else {
                due = met_ms >= e.trigger_value;
            }

            if (!due) {
                continue;
            }

            if (!command_out.send(e.cmd)) {
                recordError();
                publishEvent(Severity::WARNING, "ATSRTS_SEND_FAIL");
            } else {
                publishEvent(Severity::INFO, "ATSRTS_SENT");
            }
            e.active = false;
        }
    }

    auto onEvent(const EventPacket& evt) -> void {
        if (!armed_) {
            return;
        }
        const uint64_t now_met = TimeService::getMET64();
        for (auto& e : entries_) {
            if (!e.active || !e.waiting_event) {
                continue;
            }
            if (e.event_source != 0U && e.event_source != evt.source_id) {
                continue;
            }
            e.waiting_event = false;
            e.trigger_value = now_met + e.trigger_value; // convert delay to absolute MET
            e.trigger = TriggerType::ABS_MET_MS;
            publishEvent(Severity::INFO, "ATSRTS_RTS_ARM");
        }
    }

    auto activeCount() const -> size_t {
        size_t n = 0U;
        for (const auto& e : entries_) {
            if (e.active) {
                ++n;
            }
        }
        return n;
    }

    auto clearAll() -> void {
        for (auto& e : entries_) {
            e = Entry{};
        }
    }

    static auto toU32(float value) -> uint32_t {
        if (!std::isfinite(value) || value <= 0.0F) {
            return 0U;
        }
        if (value >= 4294967295.0F) {
            return 0xFFFFFFFFU;
        }
        return static_cast<uint32_t>(value);
    }

    static auto toU16(float value) -> uint16_t {
        if (!std::isfinite(value) || value <= 0.0F) {
            return 0U;
        }
        if (value >= 65535.0F) {
            return 0xFFFFU;
        }
        return static_cast<uint16_t>(value);
    }

    static auto toTriggerType(uint32_t raw) -> TriggerType {
        if (raw == 1U) {
            return TriggerType::ABS_UTC_SEC;
        }
        if (raw == 2U) {
            return TriggerType::REL_EVENT_DELAY_MS;
        }
        return TriggerType::ABS_MET_MS;
    }

    auto publishEvent(uint32_t severity, const char* msg) -> void {
        (void)event_out.send(EventPacket::create(severity, getId(), msg));
    }
};

} // namespace deltav
