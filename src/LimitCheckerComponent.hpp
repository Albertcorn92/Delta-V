#pragma once
// =============================================================================
// LimitCheckerComponent.hpp — DELTA-V Dynamic Telemetry Limit Checking
// =============================================================================
// Civilian operations helper:
// - Ground can adjust warning/critical thresholds without recompiling.
// - Evaluates live telemetry against per-component limit table.
// - Emits warning/critical events for dynamic FDIR workflows.
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

class LimitCheckerComponent final : public Component {
public:
    static constexpr size_t MAX_LIMIT_ROWS = 64U;

    // target_id: this component
    static constexpr uint32_t OP_SET_TARGET_ID = 1;
    static constexpr uint32_t OP_SET_WARN_LOW = 2;
    static constexpr uint32_t OP_SET_WARN_HIGH = 3;
    static constexpr uint32_t OP_SET_CRIT_LOW = 4;
    static constexpr uint32_t OP_SET_CRIT_HIGH = 5;
    static constexpr uint32_t OP_APPLY_LIMITS = 6;
    static constexpr uint32_t OP_CLEAR_TARGET = 7;
    static constexpr uint32_t OP_ENABLE = 8;
    static constexpr uint32_t OP_DISABLE = 9;

    InputPort<CommandPacket>          cmd_in{};
    InputPort<Serializer::ByteArray>  telem_in{};
    OutputPort<Serializer::ByteArray> telemetry_out{};
    OutputPort<EventPacket>           event_out{};

    LimitCheckerComponent(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    auto init() -> void override {
        for (auto& row : rows_) {
            row = Row{};
        }
        enabled_ = true;
        staged_target_id_ = 0U;
        staged_warn_low_ = -1.0e9F;
        staged_warn_high_ = 1.0e9F;
        staged_crit_low_ = -1.0e9F;
        staged_crit_high_ = 1.0e9F;
        alarm_count_ = 0U;
        publishEvent(Severity::INFO, "LIM_READY");
    }

    auto step() -> void override {
        CommandPacket cmd{};
        while (cmd_in.tryConsume(cmd)) {
            handleCommand(cmd);
        }

        Serializer::ByteArray raw{};
        while (telem_in.tryConsume(raw)) {
            if (!enabled_) {
                continue;
            }
            const TelemetryPacket p = Serializer::unpackTelem(raw);
            evaluateSample(p);
        }

        const TelemetryPacket status{
            TimeService::getMET(),
            getId(),
            static_cast<float>(alarm_count_),
        };
        (void)sendOrRecordError(telemetry_out, Serializer::pack(status));
    }

private:
    enum class AlarmLevel : uint8_t {
        NONE = 0,
        WARN = 1,
        CRIT = 2,
    };

    struct Row {
        bool active{false};
        uint32_t target_id{0U};
        float warn_low{-1.0e9F};
        float warn_high{1.0e9F};
        float crit_low{-1.0e9F};
        float crit_high{1.0e9F};
        AlarmLevel last_level{AlarmLevel::NONE};
    };

    std::array<Row, MAX_LIMIT_ROWS> rows_{};
    bool enabled_{true};

    uint32_t staged_target_id_{0U};
    float staged_warn_low_{-1.0e9F};
    float staged_warn_high_{1.0e9F};
    float staged_crit_low_{-1.0e9F};
    float staged_crit_high_{1.0e9F};

    uint32_t alarm_count_{0U};

    auto handleCommand(const CommandPacket& cmd) -> void {
        switch (cmd.opcode) {
        case OP_SET_TARGET_ID:
            staged_target_id_ = toU32(cmd.argument);
            break;
        case OP_SET_WARN_LOW:
            staged_warn_low_ = cmd.argument;
            break;
        case OP_SET_WARN_HIGH:
            staged_warn_high_ = cmd.argument;
            break;
        case OP_SET_CRIT_LOW:
            staged_crit_low_ = cmd.argument;
            break;
        case OP_SET_CRIT_HIGH:
            staged_crit_high_ = cmd.argument;
            break;
        case OP_APPLY_LIMITS:
            applyStagedLimits();
            break;
        case OP_CLEAR_TARGET:
            clearTarget(staged_target_id_);
            break;
        case OP_ENABLE:
            enabled_ = true;
            publishEvent(Severity::INFO, "LIM_ON");
            break;
        case OP_DISABLE:
            enabled_ = false;
            publishEvent(Severity::INFO, "LIM_OFF");
            break;
        default:
            recordError();
            publishEvent(Severity::WARNING, "LIM_BAD_OP");
            break;
        }
    }

    auto applyStagedLimits() -> void {
        if (staged_target_id_ == 0U) {
            recordError();
            publishEvent(Severity::WARNING, "LIM_NO_TARGET");
            return;
        }
        if (!(staged_crit_low_ <= staged_warn_low_ && staged_warn_low_ <= staged_warn_high_ &&
              staged_warn_high_ <= staged_crit_high_)) {
            recordError();
            publishEvent(Severity::WARNING, "LIM_BAD_RANGE");
            return;
        }

        Row* row = findRow(staged_target_id_);
        if (row == nullptr) {
            row = allocRow(staged_target_id_);
        }
        if (row == nullptr) {
            recordError();
            publishEvent(Severity::WARNING, "LIM_FULL");
            return;
        }
        row->active = true;
        row->target_id = staged_target_id_;
        row->warn_low = staged_warn_low_;
        row->warn_high = staged_warn_high_;
        row->crit_low = staged_crit_low_;
        row->crit_high = staged_crit_high_;
        row->last_level = AlarmLevel::NONE;
        publishEvent(Severity::INFO, "LIM_APPLY");
    }

    auto clearTarget(uint32_t target_id) -> void {
        Row* row = findRow(target_id);
        if (row == nullptr) {
            return;
        }
        *row = Row{};
        publishEvent(Severity::INFO, "LIM_CLEAR");
    }

    auto evaluateSample(const TelemetryPacket& p) -> void {
        if (p.component_id == getId()) {
            return;
        }
        Row* row = findRow(p.component_id);
        if (row == nullptr) {
            return;
        }
        const float v = p.data_payload;
        AlarmLevel level = AlarmLevel::NONE;
        if (v <= row->crit_low || v >= row->crit_high) {
            level = AlarmLevel::CRIT;
        } else if (v <= row->warn_low || v >= row->warn_high) {
            level = AlarmLevel::WARN;
        }

        if (level == row->last_level || level == AlarmLevel::NONE) {
            row->last_level = level;
            return;
        }
        row->last_level = level;
        ++alarm_count_;
        if (level == AlarmLevel::CRIT) {
            publishEvent(Severity::CRITICAL, "LIM_CRIT");
        } else {
            publishEvent(Severity::WARNING, "LIM_WARN");
        }
    }

    auto findRow(uint32_t target_id) -> Row* {
        for (auto& row : rows_) {
            if (row.active && row.target_id == target_id) {
                return &row;
            }
        }
        return nullptr;
    }

    auto allocRow(uint32_t target_id) -> Row* {
        for (auto& row : rows_) {
            if (!row.active) {
                row.active = true;
                row.target_id = target_id;
                return &row;
            }
        }
        return nullptr;
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

    auto publishEvent(uint32_t severity, const char* msg) -> void {
        (void)sendOrRecordError(event_out, EventPacket::create(severity, getId(), msg));
    }
};

} // namespace deltav
