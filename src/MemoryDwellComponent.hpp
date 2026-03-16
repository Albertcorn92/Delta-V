#pragma once
// =============================================================================
// MemoryDwellComponent.hpp — DELTA-V Safe Dwell and Snapshot Service
// =============================================================================
// Civilian operations helper:
// - Exposes a fixed allow-list of diagnostic channels (no raw address access).
// - Supports periodic dwell sampling and one-shot snapshot commands.
// - Publishes selected channel value as standard telemetry.
// =============================================================================
#include "Component.hpp"
#include "ParamDb.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <array>
#include <cmath>
#include <cinttypes>
#include <cstdio>
#include <cstdint>

namespace deltav {

class MemoryDwellComponent final : public Component {
public:
    // Dwell command opcodes (target: this component ID)
    static constexpr uint32_t OP_SELECT_CHANNEL = 1;     // arg: channel id
    static constexpr uint32_t OP_SET_PERIOD_SECONDS = 2; // arg: dwell period seconds
    static constexpr uint32_t OP_SAMPLE_NOW = 3;         // arg ignored
    static constexpr uint32_t OP_SET_ADDR_HI16 = 10;     // arg: upper 16 bits
    static constexpr uint32_t OP_SET_ADDR_LO16 = 11;     // arg: lower 16 bits
    static constexpr uint32_t OP_DWELL_ADDRESS = 12;     // arg ignored
    static constexpr uint32_t OP_SET_VALUE_HI16 = 13;    // arg: upper 16 bits
    static constexpr uint32_t OP_SET_VALUE_LO16 = 14;    // arg: lower 16 bits
    static constexpr uint32_t OP_PATCH_ADDRESS = 15;     // arg ignored

    static constexpr uint32_t DEBUG_MEM_BASE_ADDR = 0x20000000U;
    static constexpr size_t DEBUG_MEM_WORDS = 256U;

    InputPort<CommandPacket>          cmd_in{};
    OutputPort<Serializer::ByteArray> telemetry_out{};
    OutputPort<EventPacket>           event_out{};

    MemoryDwellComponent(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    [[nodiscard]] auto readDebugWordForTest(uint32_t addr) const -> uint32_t {
        const int idx = resolveWordIndex(addr);
        if (idx < 0) {
            return 0U;
        }
        return debug_mem_[static_cast<size_t>(idx)];
    }

    auto init() -> void override {
        selected_channel_ = 0U;
        period_ms_ = 1000U;
        last_emit_ms_ = 0U;
        staged_addr_hi16_ = static_cast<uint16_t>(DEBUG_MEM_BASE_ADDR >> 16U);
        staged_addr_lo16_ = static_cast<uint16_t>(DEBUG_MEM_BASE_ADDR & 0xFFFFU);
        staged_value_hi16_ = 0U;
        staged_value_lo16_ = 0U;
        debug_mem_.fill(0U);
        publishEvent(Severity::INFO, "DWELL_READY");
    }

    auto step() -> void override {
        CommandPacket cmd{};
        while (cmd_in.tryConsume(cmd)) {
            handleCommand(cmd);
        }

        const uint32_t now = TimeService::getMET();
        if ((now - last_emit_ms_) >= period_ms_) {
            emitSample();
            last_emit_ms_ = now;
        }
    }

private:
    uint32_t selected_channel_{0U};
    uint32_t period_ms_{1000U};
    uint32_t last_emit_ms_{0U};
    uint16_t staged_addr_hi16_{0U};
    uint16_t staged_addr_lo16_{0U};
    uint16_t staged_value_hi16_{0U};
    uint16_t staged_value_lo16_{0U};
    std::array<uint32_t, DEBUG_MEM_WORDS> debug_mem_{};

    auto handleCommand(const CommandPacket& cmd) -> void {
        switch (cmd.opcode) {
        case OP_SELECT_CHANNEL:
            if (!std::isfinite(cmd.argument) || cmd.argument < 0.0F) {
                selected_channel_ = 0U;
                recordError();
            } else {
                selected_channel_ = static_cast<uint32_t>(cmd.argument);
            }
            publishEvent(Severity::INFO, "DWELL_CH_SET");
            break;
        case OP_SET_PERIOD_SECONDS: {
            const float s = (cmd.argument <= 0.0F) ? 0.25F : cmd.argument;
            period_ms_ = static_cast<uint32_t>(s * 1000.0F);
            if (period_ms_ < 50U) {
                period_ms_ = 50U;
            }
            publishEvent(Severity::INFO, "DWELL_RATE_SET");
            break;
        }
        case OP_SAMPLE_NOW:
            emitSample();
            publishEvent(Severity::INFO, "DWELL_SAMPLE");
            break;
        case OP_SET_ADDR_HI16:
            staged_addr_hi16_ = toU16(cmd.argument);
            publishEvent(Severity::INFO, "DWELL_ADDR_HI");
            break;
        case OP_SET_ADDR_LO16:
            staged_addr_lo16_ = toU16(cmd.argument);
            publishEvent(Severity::INFO, "DWELL_ADDR_LO");
            break;
        case OP_DWELL_ADDRESS:
            dwellAddress();
            break;
        case OP_SET_VALUE_HI16:
            staged_value_hi16_ = toU16(cmd.argument);
            publishEvent(Severity::INFO, "DWELL_VAL_HI");
            break;
        case OP_SET_VALUE_LO16:
            staged_value_lo16_ = toU16(cmd.argument);
            publishEvent(Severity::INFO, "DWELL_VAL_LO");
            break;
        case OP_PATCH_ADDRESS:
            patchAddress();
            break;
        default:
            recordError();
            publishEvent(Severity::WARNING, "DWELL_BAD_OP");
            break;
        }
    }

    auto emitSample() -> void {
        const TelemetryPacket p{
            TimeService::getMET(),
            getId(),
            readChannelValue(selected_channel_),
        };
        (void)sendOrRecordError(telemetry_out, Serializer::pack(p));
    }

    static auto readParam(const char* name, float def) -> float {
        return ParamDb::getInstance().getParam(ParamDb::fnv1a(name), def);
    }

    auto readChannelValue(uint32_t channel) -> float {
        switch (channel) {
        case 0U:
            return static_cast<float>(TimeService::getMET()) / 1000.0F; // MET seconds
        case 1U:
            return readParam("safe_mode_battery_pct", 5.0F);
        case 2U:
            return readParam("degraded_battery_pct", 15.0F);
        case 3U:
            return readParam("emergency_battery_pct", 2.0F);
        case 4U:
            return static_cast<float>(ParamDb::getInstance().paramCount());
        default:
            recordError();
            return -1.0F;
        }
    }

    auto dwellAddress() -> void {
        const uint32_t addr = composeAddress();
        const int idx = resolveWordIndex(addr);
        if (idx < 0) {
            recordError();
            publishEvent(Severity::WARNING, "DWELL_ADDR_RANGE");
            return;
        }

        const uint32_t value = debug_mem_[static_cast<size_t>(idx)];
        const TelemetryPacket p{
            TimeService::getMET(),
            getId(),
            static_cast<float>(value & 0x00FFFFFFU),
        };
        (void)sendOrRecordError(telemetry_out, Serializer::pack(p));

        char msg[28]{};
        (void)std::snprintf(
            msg,
            sizeof(msg),
            "DW:%08" PRIX32 "=%08" PRIX32,
            static_cast<uint32_t>(addr),
            static_cast<uint32_t>(value)
        );
        (void)sendOrRecordError(event_out, EventPacket::create(Severity::INFO, getId(), msg));
    }

    auto patchAddress() -> void {
        const uint32_t addr = composeAddress();
        const int idx = resolveWordIndex(addr);
        if (idx < 0) {
            recordError();
            publishEvent(Severity::WARNING, "PATCH_ADDR_RANGE");
            return;
        }

        const uint32_t value = composeValue();
        debug_mem_[static_cast<size_t>(idx)] = value;
        char msg[28]{};
        (void)std::snprintf(
            msg,
            sizeof(msg),
            "PT:%08" PRIX32 "=%08" PRIX32,
            static_cast<uint32_t>(addr),
            static_cast<uint32_t>(value)
        );
        (void)sendOrRecordError(event_out, EventPacket::create(Severity::INFO, getId(), msg));
    }

    [[nodiscard]] auto composeAddress() const -> uint32_t {
        return (static_cast<uint32_t>(staged_addr_hi16_) << 16U) |
               static_cast<uint32_t>(staged_addr_lo16_);
    }

    [[nodiscard]] auto composeValue() const -> uint32_t {
        return (static_cast<uint32_t>(staged_value_hi16_) << 16U) |
               static_cast<uint32_t>(staged_value_lo16_);
    }

    [[nodiscard]] auto resolveWordIndex(uint32_t addr) const -> int {
        if (addr < DEBUG_MEM_BASE_ADDR) {
            return -1;
        }
        const uint32_t offset = addr - DEBUG_MEM_BASE_ADDR;
        if ((offset % 4U) != 0U) {
            return -1;
        }
        const uint32_t word = offset / 4U;
        if (word >= DEBUG_MEM_WORDS) {
            return -1;
        }
        return static_cast<int>(word);
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
