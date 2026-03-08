#pragma once
// =============================================================================
// ModeManagerComponent.hpp — DELTA-V Operational Mode Orchestrator
// =============================================================================
// Civilian operations helper:
// - Maintains spacecraft operational mode (DETUMBLE/SUN/SCIENCE/DOWNLINK).
// - Applies mode-specific component command rules without recompiling.
// - Uses fixed-size deterministic storage and CommandPacket dispatch.
// =============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace deltav {

class ModeManagerComponent final : public Component {
public:
    static constexpr size_t MAX_RULES = 32U;

    enum class Mode : uint8_t {
        DETUMBLE = 1U,
        SUN_POINTING = 2U,
        SCIENCE_GATHERING = 3U,
        DOWNLINK = 4U,
    };

    static constexpr uint8_t MODE_MASK_DETUMBLE = 0x01U;
    static constexpr uint8_t MODE_MASK_SUN = 0x02U;
    static constexpr uint8_t MODE_MASK_SCIENCE = 0x04U;
    static constexpr uint8_t MODE_MASK_DOWNLINK = 0x08U;

    // target_id: this component
    static constexpr uint32_t OP_STAGE_TARGET_ID = 1;
    static constexpr uint32_t OP_STAGE_ENABLE_OPCODE = 2;
    static constexpr uint32_t OP_STAGE_ENABLE_ARG = 3;
    static constexpr uint32_t OP_STAGE_DISABLE_OPCODE = 4;
    static constexpr uint32_t OP_STAGE_DISABLE_ARG = 5;
    static constexpr uint32_t OP_STAGE_MODE_MASK = 6;
    static constexpr uint32_t OP_COMMIT_RULE = 7;
    static constexpr uint32_t OP_CLEAR_RULES = 8;
    static constexpr uint32_t OP_SET_MODE = 9;
    static constexpr uint32_t OP_APPLY_MODE = 10;
    static constexpr uint32_t OP_REPORT_STATUS = 11;

    InputPort<CommandPacket>          cmd_in{};
    OutputPort<CommandPacket>         command_out{};
    OutputPort<Serializer::ByteArray> telemetry_out{};
    OutputPort<EventPacket>           event_out{};

    ModeManagerComponent(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    auto init() -> void override {
        clearRules();
        current_mode_ = Mode::SUN_POINTING;
        transitions_ = 0U;
        dispatched_count_ = 0U;
        staged_target_id_ = 0U;
        staged_enable_opcode_ = 0U;
        staged_disable_opcode_ = 0U;
        staged_enable_arg_ = 0.0F;
        staged_disable_arg_ = 0.0F;
        staged_mode_mask_ = MODE_MASK_SUN;
        publishEvent(Severity::INFO, "MODE_READY");
    }

    auto step() -> void override {
        CommandPacket cmd{};
        while (cmd_in.tryConsume(cmd)) {
            handleCommand(cmd);
        }

        const TelemetryPacket mode_tlm{
            TimeService::getMET(),
            getId(),
            static_cast<float>(modeToU8(current_mode_)),
        };
        (void)telemetry_out.send(Serializer::pack(mode_tlm));
    }

private:
    struct Rule {
        bool active{false};
        uint32_t target_id{0U};
        uint32_t enable_opcode{0U};
        float enable_arg{0.0F};
        uint32_t disable_opcode{0U};
        float disable_arg{0.0F};
        uint8_t mode_mask{0U};
        bool last_enabled{false};
    };

    std::array<Rule, MAX_RULES> rules_{};

    Mode current_mode_{Mode::SUN_POINTING};
    uint32_t transitions_{0U};
    uint32_t dispatched_count_{0U};

    uint32_t staged_target_id_{0U};
    uint32_t staged_enable_opcode_{0U};
    float staged_enable_arg_{0.0F};
    uint32_t staged_disable_opcode_{0U};
    float staged_disable_arg_{0.0F};
    uint8_t staged_mode_mask_{MODE_MASK_SUN};

    auto handleCommand(const CommandPacket& cmd) -> void {
        switch (cmd.opcode) {
        case OP_STAGE_TARGET_ID:
            staged_target_id_ = toU32(cmd.argument);
            break;
        case OP_STAGE_ENABLE_OPCODE:
            staged_enable_opcode_ = toU32(cmd.argument);
            break;
        case OP_STAGE_ENABLE_ARG:
            staged_enable_arg_ = cmd.argument;
            break;
        case OP_STAGE_DISABLE_OPCODE:
            staged_disable_opcode_ = toU32(cmd.argument);
            break;
        case OP_STAGE_DISABLE_ARG:
            staged_disable_arg_ = cmd.argument;
            break;
        case OP_STAGE_MODE_MASK:
            staged_mode_mask_ = sanitizeMask(toU8(cmd.argument));
            break;
        case OP_COMMIT_RULE:
            commitRule();
            break;
        case OP_CLEAR_RULES:
            clearRules();
            publishEvent(Severity::INFO, "MODE_RULES_CLEAR");
            break;
        case OP_SET_MODE:
            setMode(toMode(toU8(cmd.argument)));
            break;
        case OP_APPLY_MODE:
            applyCurrentMode();
            break;
        case OP_REPORT_STATUS:
            reportStatus();
            break;
        default:
            recordError();
            publishEvent(Severity::WARNING, "MODE_BAD_OP");
            break;
        }
    }

    auto commitRule() -> void {
        if (staged_target_id_ == 0U) {
            recordError();
            publishEvent(Severity::WARNING, "MODE_NO_TARGET");
            return;
        }

        Rule* rule = findRule(staged_target_id_);
        if (rule == nullptr) {
            rule = allocRule();
        }
        if (rule == nullptr) {
            recordError();
            publishEvent(Severity::WARNING, "MODE_RULE_FULL");
            return;
        }

        rule->active = true;
        rule->target_id = staged_target_id_;
        rule->enable_opcode = staged_enable_opcode_;
        rule->enable_arg = staged_enable_arg_;
        rule->disable_opcode = staged_disable_opcode_;
        rule->disable_arg = staged_disable_arg_;
        rule->mode_mask = staged_mode_mask_;
        rule->last_enabled = false;
        publishEvent(Severity::INFO, "MODE_RULE_SET");
    }

    auto applyCurrentMode() -> void {
        const uint8_t bit = modeBit(current_mode_);
        for (auto& rule : rules_) {
            if (!rule.active) {
                continue;
            }
            const bool should_enable = (rule.mode_mask & bit) != 0U;
            const uint32_t opcode = should_enable ? rule.enable_opcode : rule.disable_opcode;
            const float arg = should_enable ? rule.enable_arg : rule.disable_arg;
            if (opcode == 0U) {
                continue;
            }
            const CommandPacket cmd{rule.target_id, opcode, arg};
            if (!command_out.send(cmd)) {
                recordError();
                publishEvent(Severity::WARNING, "MODE_SEND_FAIL");
            } else {
                ++dispatched_count_;
            }
            rule.last_enabled = should_enable;
        }
        publishEvent(Severity::INFO, "MODE_APPLY");
    }

    auto setMode(Mode mode) -> void {
        if (mode == current_mode_) {
            publishEvent(Severity::INFO, "MODE_NO_CHANGE");
            return;
        }
        current_mode_ = mode;
        ++transitions_;
        publishEvent(Severity::INFO, "MODE_SET");
    }

    auto reportStatus() -> void {
        const TelemetryPacket transitions_tlm{
            TimeService::getMET(),
            getId(),
            static_cast<float>(transitions_),
        };
        (void)telemetry_out.send(Serializer::pack(transitions_tlm));

        const TelemetryPacket dispatch_tlm{
            TimeService::getMET(),
            getId(),
            static_cast<float>(dispatched_count_),
        };
        (void)telemetry_out.send(Serializer::pack(dispatch_tlm));
    }

    auto clearRules() -> void {
        for (auto& rule : rules_) {
            rule = Rule{};
        }
    }

    auto findRule(uint32_t target_id) -> Rule* {
        for (auto& rule : rules_) {
            if (rule.active && rule.target_id == target_id) {
                return &rule;
            }
        }
        return nullptr;
    }

    auto allocRule() -> Rule* {
        for (auto& rule : rules_) {
            if (!rule.active) {
                return &rule;
            }
        }
        return nullptr;
    }

    static auto toU32(float value) -> uint32_t {
        if (value <= 0.0F) {
            return 0U;
        }
        if (value >= 4294967295.0F) {
            return 0xFFFFFFFFU;
        }
        return static_cast<uint32_t>(value);
    }

    static auto toU8(float value) -> uint8_t {
        if (value <= 0.0F) {
            return 0U;
        }
        if (value >= 255.0F) {
            return 0xFFU;
        }
        return static_cast<uint8_t>(value);
    }

    static auto sanitizeMask(uint8_t mask) -> uint8_t {
        constexpr uint8_t allowed = MODE_MASK_DETUMBLE | MODE_MASK_SUN |
            MODE_MASK_SCIENCE | MODE_MASK_DOWNLINK;
        const uint8_t bounded = static_cast<uint8_t>(mask & allowed);
        return bounded == 0U ? MODE_MASK_SUN : bounded;
    }

    static auto toMode(uint8_t raw) -> Mode {
        switch (raw) {
        case 1U: return Mode::DETUMBLE;
        case 2U: return Mode::SUN_POINTING;
        case 3U: return Mode::SCIENCE_GATHERING;
        case 4U: return Mode::DOWNLINK;
        default: return Mode::SUN_POINTING;
        }
    }

    static auto modeToU8(Mode mode) -> uint8_t {
        return static_cast<uint8_t>(mode);
    }

    static auto modeBit(Mode mode) -> uint8_t {
        switch (mode) {
        case Mode::DETUMBLE:
            return MODE_MASK_DETUMBLE;
        case Mode::SUN_POINTING:
            return MODE_MASK_SUN;
        case Mode::SCIENCE_GATHERING:
            return MODE_MASK_SCIENCE;
        case Mode::DOWNLINK:
            return MODE_MASK_DOWNLINK;
        default:
            return MODE_MASK_SUN;
        }
    }

    auto publishEvent(uint32_t severity, const char* msg) -> void {
        (void)event_out.send(EventPacket::create(severity, getId(), msg));
    }
};

} // namespace deltav
