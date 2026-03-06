#pragma once
// =============================================================================
// WatchdogComponent.hpp — DELTA-V FDIR Supervisor  v3.0
// =============================================================================
// DO-178C Compliant:
// - Explicit memory initialization for all pointers
// - std::array bounds-checking for all strings/arrays
// - constexpr definitions for all magic numbers
// =============================================================================
#include "Component.hpp"
#include "ParamDb.hpp"
#include "Port.hpp"
#include "TimeService.hpp"
#include "TmrStore.hpp"
#include "Types.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>

namespace deltav {

constexpr size_t   MAX_MONITORED_SUBSYSTEMS     = 32;
constexpr uint32_t MAX_RESTARTS_PER_COMPONENT   = 3;
constexpr uint32_t HEARTBEAT_INTERVAL_CYCLES    = 10;
constexpr uint32_t PARAM_CHECK_INTERVAL_CYCLES  = 30;

// DO-178C: Explicitly defined FDIR thresholds
constexpr float BATT_EMERGENCY_PCT = 2.0f;
constexpr float BATT_SAFE_MODE_PCT = 5.0f;
constexpr float BATT_DEGRADED_PCT  = 15.0f;
constexpr float BATT_MAX_PCT       = 100.0f;

enum class MissionState : std::uint8_t {
    BOOT        = 0,
    NOMINAL     = 1,
    DEGRADED    = 2,
    SAFE_MODE   = 3,
    EMERGENCY   = 4,
};

inline auto missionStateName(MissionState s) -> const char* {
    switch (s) {
        case MissionState::BOOT:      return "BOOT";
        case MissionState::NOMINAL:   return "NOMINAL";
        case MissionState::DEGRADED:  return "DEGRADED";
        case MissionState::SAFE_MODE: return "SAFE_MODE";
        case MissionState::EMERGENCY: return "EMERGENCY";
        default:                      return "UNKNOWN";
    }
}

class WatchdogComponent : public Component {
public:
    /* NOLINTNEXTLINE(bugprone-easily-swappable-parameters) */
    WatchdogComponent(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
    InputPort<float>        battery_level_in{};
    OutputPort<EventPacket> event_out{};
    // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

    auto init() -> void override {
        mission_state  = MissionState::NOMINAL;
        last_batt_soc  = 100.0f;
        event_out.send(EventPacket::create(Severity::INFO, getId(),
            "WATCHDOG: Init complete. FDIR active."));
        std::cout << "[" << getName() << "] FDIR online. Supervising "
                  << monitored_count << " subsystems.\n";
    }

    auto registerSubsystem(Component* comp) -> void {
        if (monitored_count < MAX_MONITORED_SUBSYSTEMS) {
            monitored_systems.at(monitored_count) = comp;
            restart_counts.at(monitored_count)    = 0;
            ++monitored_count;
        }
    }

    auto setBatteryThresholdSources(
        TmrStore<float>* emergency_pct,
        TmrStore<float>* safe_mode_pct,
        TmrStore<float>* degraded_pct) -> void {
        batt_emergency_pct_src = emergency_pct;
        batt_safe_mode_pct_src = safe_mode_pct;
        batt_degraded_pct_src  = degraded_pct;
    }

    [[nodiscard]] auto hasBatteryThresholdSources() const -> bool {
        return batt_emergency_pct_src != nullptr &&
               batt_safe_mode_pct_src != nullptr &&
               batt_degraded_pct_src  != nullptr;
    }

    // F-11: Surface scheduler frame drops into FDIR event stream (DV-FDIR-07)
    auto pollSchedulerHealth(uint64_t frame_drops) -> void {
        if (frame_drops > last_frame_drops) {
            uint64_t new_drops = frame_drops - last_frame_drops;
            last_frame_drops   = frame_drops;
            std::array<char, EVENT_MSG_SIZE> msg{};
            (void)std::snprintf(msg.data(), msg.size(), "SCHED: %llu frame drops",
                static_cast<unsigned long long>(new_drops));
            emit(Severity::WARNING, msg.data());
            recordError();
        }
    }

    // F-17: Poll InputPort overflow and emit event
    template<typename T, size_t D>
    auto pollPortOverflow(InputPort<T, D>& port, const char* port_name) -> void {
        uint32_t ov = port.drainOverflowCount();
        if (ov > 0) {
            std::array<char, EVENT_MSG_SIZE> msg{};
            (void)std::snprintf(msg.data(), msg.size(), "OVF:%s x%u", port_name, ov);
            emit(Severity::WARNING, msg.data());
            recordError();
        }
    }

    [[nodiscard]] auto getMissionState() const -> MissionState { return mission_state; }

    // For MissionFsm gating — returns the raw uint8_t state value
    [[nodiscard]] auto getMissionStateRaw() const -> std::uint8_t {
        return static_cast<std::uint8_t>(mission_state);
    }

    // HIL / Fault Injection Test Hook (DV-FDIR validation)
    auto injectBatteryLevel(float soc) -> void {
        processBattery(soc);
    }

    auto step() -> void override {
        ++cycles;

        checkBattery();
        checkSubsystems();
        tryRecoverNominal();

        // Periodic checks
        if (cycles % PARAM_CHECK_INTERVAL_CYCLES == 0) {
            // DV-DATA-01: ParamDb CRC integrity
            if (!ParamDb::getInstance().verifyIntegrity()) {
                emit(Severity::CRITICAL, "FDIR: ParamDb CRC mismatch!");
                escalate(MissionState::SAFE_MODE);
            }
            // DV-TMR-01: TMR scrub for SEU repair
            TmrRegistry::getInstance().scrubAll();
        }

        if (cycles % HEARTBEAT_INTERVAL_CYCLES == 0) {
            std::array<char, EVENT_MSG_SIZE> msg{};
            (void)std::snprintf(msg.data(), msg.size(), "HB: %s T+%lus",
                missionStateName(mission_state),
                static_cast<unsigned long>(TimeService::getMET() / 1000));
            event_out.send(EventPacket::create(Severity::INFO, getId(), msg.data()));
        }
    }

private:
    struct BatteryThresholds {
        float emergency_pct{BATT_EMERGENCY_PCT};
        float safe_mode_pct{BATT_SAFE_MODE_PCT};
        float degraded_pct{BATT_DEGRADED_PCT};
    };

    std::array<Component*, MAX_MONITORED_SUBSYSTEMS> monitored_systems{};
    std::array<uint32_t, MAX_MONITORED_SUBSYSTEMS>   restart_counts{};
    size_t                                           monitored_count{0};

    MissionState mission_state{MissionState::BOOT};
    uint32_t     cycles{0};
    float        last_batt_soc{100.0f};
    uint64_t     last_frame_drops{0};
    TmrStore<float>* batt_emergency_pct_src{nullptr};
    TmrStore<float>* batt_safe_mode_pct_src{nullptr};
    TmrStore<float>* batt_degraded_pct_src{nullptr};
    bool threshold_fault_latched{false};

    auto checkBattery() -> void {
        float batt{0.0f};
        while (battery_level_in.tryConsume(batt)) {
            processBattery(batt);
        }
    }

    auto processBattery(float batt) -> void {
        const BatteryThresholds thresholds = currentBatteryThresholds();
        last_batt_soc = batt;
        if (batt <= thresholds.emergency_pct) {
            std::array<char, EVENT_MSG_SIZE> msg{};
            (void)std::snprintf(msg.data(), msg.size(), "FDIR: Battery <=%.1f%%. EMERG",
                thresholds.emergency_pct);
            emit(Severity::CRITICAL, msg.data());
            escalate(MissionState::EMERGENCY);
        } else if (batt <= thresholds.safe_mode_pct) {
            std::array<char, EVENT_MSG_SIZE> msg{};
            (void)std::snprintf(msg.data(), msg.size(), "FDIR: Battery <=%.1f%%. SAFE",
                thresholds.safe_mode_pct);
            emit(Severity::CRITICAL, msg.data());
            escalate(MissionState::SAFE_MODE);
        } else if (batt <= thresholds.degraded_pct) {
            std::array<char, EVENT_MSG_SIZE> msg{};
            (void)std::snprintf(msg.data(), msg.size(), "FDIR: Battery <=%.1f%%. DEGR",
                thresholds.degraded_pct);
            emit(Severity::WARNING, msg.data());
            escalate(MissionState::DEGRADED);
        }
    }

    auto checkSubsystems() -> void {
        bool any_critical = false;
        bool any_warning  = false;

        for (size_t i = 0; i < monitored_count; ++i) {
            Component* comp = nullptr;
            comp = monitored_systems.at(i);
            
            if (comp == nullptr) continue;

            HealthStatus health = comp->reportHealth();

            if (health == HealthStatus::CRITICAL_FAILURE) {
                any_critical = true;
                handleCritical(i);
            } else if (health == HealthStatus::WARNING) {
                any_warning = true;
                const auto comp_name = comp->getName();
                std::array<char, EVENT_MSG_SIZE> msg{};
                (void)std::snprintf(msg.data(), msg.size(), "FDIR: %.*s WARN (%u)",
                    static_cast<int>(comp_name.size()), comp_name.data(),
                    comp->getErrorCount());
                emit(Severity::WARNING, msg.data());
            }
        }

        if (any_critical) escalate(MissionState::SAFE_MODE);
        else if (any_warning) escalate(MissionState::DEGRADED);
    }

    // F-15: DEGRADED/SAFE_MODE -> NOMINAL auto-recovery (DV-FDIR-05)
    auto tryRecoverNominal() -> void {
        if (mission_state != MissionState::DEGRADED &&
            mission_state != MissionState::SAFE_MODE) {
            return;
        }

        bool all_nominal = true;
        for (size_t i = 0; i < monitored_count; ++i) {
            Component* comp = nullptr;
            comp = monitored_systems.at(i);
            
            if (comp != nullptr && comp->reportHealth() != HealthStatus::NOMINAL) {
                all_nominal = false;
                break;
            }
        }

        const BatteryThresholds thresholds = currentBatteryThresholds();
        if (all_nominal && last_batt_soc > thresholds.degraded_pct) {
            emit(Severity::INFO, "FDIR: Recovery -> NOMINAL");
            mission_state = MissionState::NOMINAL;
            std::cout << "[" << getName() << "] Mission recovered to NOMINAL\n";
        }
    }

    [[nodiscard]] auto currentBatteryThresholds() -> BatteryThresholds {
        BatteryThresholds thresholds{};
        if (!hasBatteryThresholdSources()) {
            return thresholds;
        }

        thresholds.emergency_pct = batt_emergency_pct_src->read();
        thresholds.safe_mode_pct = batt_safe_mode_pct_src->read();
        thresholds.degraded_pct  = batt_degraded_pct_src->read();

        if (!areThresholdsSane(thresholds)) {
            if (!threshold_fault_latched) {
                emit(Severity::CRITICAL, "FDIR: Invalid battery thresholds. Using defaults.");
                recordError();
                threshold_fault_latched = true;
            }
            return {};
        }

        if (threshold_fault_latched) {
            emit(Severity::INFO, "FDIR: Battery threshold config restored.");
            threshold_fault_latched = false;
        }
        return thresholds;
    }

    [[nodiscard]] static auto areThresholdsSane(const BatteryThresholds& thresholds) -> bool {
        if (thresholds.emergency_pct <= 0.0F) return false;
        if (thresholds.emergency_pct >= thresholds.safe_mode_pct) return false;
        if (thresholds.safe_mode_pct >= thresholds.degraded_pct) return false;
        return thresholds.degraded_pct <= BATT_MAX_PCT;
    }

    auto handleCritical(size_t idx) -> void {
        Component* comp = nullptr;
        comp = monitored_systems.at(idx);
        
        if (comp == nullptr) return;

        const auto comp_name = comp->getName();
        std::array<char, EVENT_MSG_SIZE> msg{};
        (void)std::snprintf(msg.data(), msg.size(), "FDIR: CRIT %.*s (%u)",
            static_cast<int>(comp_name.size()), comp_name.data(), comp->getErrorCount());
        emit(Severity::CRITICAL, msg.data());

        if (comp->isActive()) {
            if (restart_counts.at(idx) < MAX_RESTARTS_PER_COMPONENT) {
                ++restart_counts.at(idx);
                comp->joinThread();
                comp->clearErrors();
                comp->startThread();
                std::array<char, EVENT_MSG_SIZE> rmsg{};
                (void)std::snprintf(rmsg.data(), rmsg.size(), "FDIR: RST %.*s (#%u)",
                    static_cast<int>(comp_name.size()), comp_name.data(),
                    restart_counts.at(idx));
                emit(Severity::WARNING, rmsg.data());
                std::cout << "[" << getName() << "] " << rmsg.data() << "\n";
            } else {
                std::array<char, EVENT_MSG_SIZE> emsg{};
                (void)std::snprintf(emsg.data(), emsg.size(), "FDIR: %.*s UNRECOV",
                    static_cast<int>(comp_name.size()), comp_name.data());
                emit(Severity::CRITICAL, emsg.data());
                escalate(MissionState::EMERGENCY);
            }
        }
    }

    auto escalate(MissionState next) -> void {
        if (static_cast<std::uint8_t>(next) > static_cast<std::uint8_t>(mission_state)) {
            std::array<char, EVENT_MSG_SIZE> msg{};
            (void)std::snprintf(msg.data(), msg.size(), "FSM: %s->%s",
                missionStateName(mission_state), missionStateName(next));
            mission_state = next;
            emit(Severity::CRITICAL, msg.data());
            std::cout << "[" << getName() << "] " << msg.data() << "\n";
        }
    }

    auto emit(uint32_t sev, const char* msg) -> void {
        event_out.send(EventPacket::create(sev, getId(), msg));
    }
};

} // namespace deltav
