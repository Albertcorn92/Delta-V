#pragma once
// =============================================================================
// WatchdogComponent.hpp — DELTA-V Fault Detection, Isolation & Recovery (FDIR)
// =============================================================================
// The Watchdog is the system's safety net. Every scheduler tick it:
//   1. Checks battery level against safe-mode threshold
//   2. Polls reportHealth() on every registered subsystem
//   3. Attempts automatic restart of failed ActiveComponents (up to MAX_RESTARTS)
//   4. Transitions the mission FSM: NOMINAL → DEGRADED → SAFE_MODE → EMERGENCY
//   5. Verifies ParamDb integrity on a slower cadence
//   6. Emits a periodic heartbeat so the GDS can detect communication loss
//
// DO-178C: No dynamic allocation. All subsystem pointers are registered at boot.
// =============================================================================
#include "Component.hpp"
#include "ParamDb.hpp"
#include "Port.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <array>
#include <cstdio>
#include <iostream>

namespace deltav {

constexpr size_t MAX_MONITORED_SUBSYSTEMS = 32;
constexpr uint32_t MAX_RESTARTS_PER_COMPONENT = 3;
constexpr uint32_t HEARTBEAT_INTERVAL_CYCLES   = 10; // at 1 Hz → 10 s
constexpr uint32_t PARAM_CHECK_INTERVAL_CYCLES = 30; // at 1 Hz → 30 s

// ---------------------------------------------------------------------------
// Mission FSM states — ordered by severity (higher = worse)
// ---------------------------------------------------------------------------
enum class MissionState : uint8_t {
    BOOT        = 0,
    NOMINAL     = 1,
    DEGRADED    = 2, // At least one WARNING component
    SAFE_MODE   = 3, // Battery critical or CRITICAL_FAILURE component
    EMERGENCY   = 4, // Unable to recover — request ground intervention
};

inline const char* missionStateName(MissionState s) {
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
    WatchdogComponent(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    InputPort<float>       battery_level_in;
    OutputPort<EventPacket> event_out;

    void init() override {
        mission_state = MissionState::NOMINAL;
        event_out.send(EventPacket::create(Severity::INFO, getId(),
            "WATCHDOG: Init complete. FDIR active."));
        std::cout << "[" << getName() << "] FDIR online. Supervising "
                  << monitored_count << " subsystems." << std::endl;
    }

    void registerSubsystem(Component* comp) {
        if (monitored_count < MAX_MONITORED_SUBSYSTEMS) {
            monitored_systems[monitored_count] = comp;
            restart_counts[monitored_count]    = 0;
            ++monitored_count;
        }
    }

    MissionState getMissionState() const { return mission_state; }

    void step() override {
        ++cycles;

        checkBattery();
        checkSubsystems();

        // Periodic ParamDb integrity check
        if (cycles % PARAM_CHECK_INTERVAL_CYCLES == 0) {
            if (!ParamDb::getInstance().verifyIntegrity()) {
                emit(Severity::CRITICAL, "FDIR: ParamDb CRC mismatch! Memory may be corrupted.");
                escalate(MissionState::SAFE_MODE);
            }
        }

        // Heartbeat
        if (cycles % HEARTBEAT_INTERVAL_CYCLES == 0) {
            char msg[28];
            std::snprintf(msg, sizeof(msg), "HB: %s T+%lus",
                missionStateName(mission_state),
                static_cast<unsigned long>(TimeService::getMET() / 1000));
            event_out.send(EventPacket::create(Severity::INFO, getId(), msg));
        }
    }

private:
    Component* monitored_systems[MAX_MONITORED_SUBSYSTEMS]{};
    uint32_t   restart_counts[MAX_MONITORED_SUBSYSTEMS]{};
    size_t     monitored_count{0};

    MissionState          mission_state{MissionState::BOOT};
    uint32_t              cycles{0};

    // -----------------------------------------------------------------------
    // Battery FDIR
    // -----------------------------------------------------------------------
    void checkBattery() {
        float batt{0.0f};
        while (battery_level_in.tryConsume(batt)) {
            if (batt <= 2.0f) {
                emit(Severity::CRITICAL, "FDIR: Battery <=2%. EMERGENCY.");
                escalate(MissionState::EMERGENCY);
            } else if (batt <= 5.0f) {
                emit(Severity::CRITICAL, "FDIR: Battery <=5%. SAFE MODE.");
                escalate(MissionState::SAFE_MODE);
            } else if (batt <= 15.0f) {
                emit(Severity::WARNING, "FDIR: Battery <=15%. DEGRADED.");
                escalate(MissionState::DEGRADED);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Software FDIR — poll every registered subsystem
    // -----------------------------------------------------------------------
    void checkSubsystems() {
        bool any_critical = false;
        bool any_warning  = false;

        for (size_t i = 0; i < monitored_count; ++i) {
            Component* comp = monitored_systems[i];
            HealthStatus health = comp->reportHealth();

            if (health == HealthStatus::CRITICAL_FAILURE) {
                any_critical = true;
                handleCritical(i);
            } else if (health == HealthStatus::WARNING) {
                any_warning = true;
                char msg[28];
                std::snprintf(msg, sizeof(msg), "FDIR: %s WARNING (%u errs)",
                    comp->getName().data(), comp->getErrorCount());
                emit(Severity::WARNING, msg);
            }
        }

        // Ratchet mission state upward (never auto-recover without ground cmd)
        if (any_critical) escalate(MissionState::SAFE_MODE);
        else if (any_warning) escalate(MissionState::DEGRADED);
    }

    // -----------------------------------------------------------------------
    // Component restart (ActiveComponent only)
    // Up to MAX_RESTARTS_PER_COMPONENT attempts before declaring EMERGENCY.
    // -----------------------------------------------------------------------
    void handleCritical(size_t idx) {
        Component* comp = monitored_systems[idx];

        char msg[28];
        std::snprintf(msg, sizeof(msg), "FDIR: CRIT %s (%u errs)",
            comp->getName().data(), comp->getErrorCount());
        emit(Severity::CRITICAL, msg);

        if (comp->isActive()) {
            if (restart_counts[idx] < MAX_RESTARTS_PER_COMPONENT) {
                ++restart_counts[idx];
                comp->joinThread();
                comp->clearErrors();
                comp->startThread();
                char rmsg[28];
                std::snprintf(rmsg, sizeof(rmsg), "FDIR: Restarted %s (#%u)",
                    comp->getName().data(), restart_counts[idx]);
                emit(Severity::WARNING, rmsg);
                std::cout << "[" << getName() << "] " << rmsg << std::endl;
            } else {
                char emsg[28];
                std::snprintf(emsg, sizeof(emsg), "FDIR: %s unrecoverable",
                    comp->getName().data());
                emit(Severity::CRITICAL, emsg);
                escalate(MissionState::EMERGENCY);
            }
        }
    }

    // -----------------------------------------------------------------------
    // FSM helpers
    // -----------------------------------------------------------------------
    void escalate(MissionState next) {
        if (static_cast<uint8_t>(next) > static_cast<uint8_t>(mission_state)) {
            char msg[28];
            std::snprintf(msg, sizeof(msg), "FSM: %s -> %s",
                missionStateName(mission_state), missionStateName(next));
            mission_state = next;
            emit(Severity::CRITICAL, msg);
            std::cout << "[" << getName() << "] Mission state: " << msg << std::endl;
        }
    }

    void emit(uint32_t sev, const char* msg) {
        event_out.send(EventPacket::create(sev, getId(), msg));
    }
};

} // namespace deltav