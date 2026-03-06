#pragma once
// =============================================================================
// WatchdogComponent.hpp — DELTA-V Fault Detection, Isolation & Recovery (FDIR)
// =============================================================================
// Every scheduler tick the Watchdog:
//   1. Checks battery level against safe-mode threshold
//   2. Polls reportHealth() on every registered subsystem (including itself)
//   3. Attempts automatic restart of failed ActiveComponents (up to MAX_RESTARTS)
//   4. Transitions the mission FSM: NOMINAL → DEGRADED → SAFE_MODE → EMERGENCY
//   5. Verifies ParamDb integrity on a slower cadence
//   6. Emits a periodic heartbeat so the GDS can detect communication loss
//   7. Recovers DEGRADED → NOMINAL when all conditions clear (F-15)
//
// Fixes applied (v2.1):
//   F-04: Watchdog can be registered with itself via registerSubsystem(&watchdog)
//         in TopologyManager::registerFdir(). Self-monitoring is now the default.
//   F-11: pollSchedulerHealth(uint64_t frame_drops) added so Scheduler's frame
//         drop count is surfaced to the FDIR event stream.
//   F-15: DEGRADED state now auto-recovers to NOMINAL when all subsystems report
//         healthy and battery is above the DEGRADED threshold. Only SAFE_MODE
//         and EMERGENCY require explicit ground command to clear.
//   F-17: pollPortOverflows() added — call with any InputPort to detect and
//         report queue overflow events to the GDS.
//
// DO-178C: No dynamic allocation. All arrays are fixed at compile time.
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

constexpr size_t   MAX_MONITORED_SUBSYSTEMS     = 32;
constexpr uint32_t MAX_RESTARTS_PER_COMPONENT   = 3;
constexpr uint32_t HEARTBEAT_INTERVAL_CYCLES    = 10; // at 1 Hz → 10 s
constexpr uint32_t PARAM_CHECK_INTERVAL_CYCLES  = 30; // at 1 Hz → 30 s

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

    InputPort<float>        battery_level_in;
    OutputPort<EventPacket> event_out;

    void init() override {
        mission_state  = MissionState::NOMINAL;
        last_batt_soc  = 100.0f;
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

    // F-11: Called by Scheduler (or a periodic task) to surface frame drops
    // into the FDIR event stream.
    void pollSchedulerHealth(uint64_t frame_drops) {
        if (frame_drops > last_frame_drops) {
            uint64_t new_drops = frame_drops - last_frame_drops;
            last_frame_drops   = frame_drops;
            char msg[28];
            std::snprintf(msg, sizeof(msg), "SCHED: %llu frame drops",
                static_cast<unsigned long long>(new_drops));
            emit(Severity::WARNING, msg);
            recordError();
        }
    }

    // F-17: Poll an InputPort for overflow and emit an event if found.
    // Call this from step() for any critical port (e.g. event_in on RadioLink).
    template<typename T, size_t D>
    void pollPortOverflow(InputPort<T, D>& port, const char* port_name) {
        uint32_t ov = port.drainOverflowCount();
        if (ov > 0) {
            char msg[28];
            std::snprintf(msg, sizeof(msg), "OVF:%s x%u", port_name, ov);
            emit(Severity::WARNING, msg);
            recordError();
        }
    }

    MissionState getMissionState() const { return mission_state; }

    void step() override {
        ++cycles;

        checkBattery();
        checkSubsystems();
        tryRecoverDegraded(); // F-15: auto-recovery for DEGRADED

        // Periodic ParamDb integrity check
        if (cycles % PARAM_CHECK_INTERVAL_CYCLES == 0) {
            if (!ParamDb::getInstance().verifyIntegrity()) {
                emit(Severity::CRITICAL, "FDIR: ParamDb CRC mismatch!");
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

    MissionState mission_state{MissionState::BOOT};
    uint32_t     cycles{0};
    float        last_batt_soc{100.0f};
    uint64_t     last_frame_drops{0};

    // -----------------------------------------------------------------------
    // Battery FDIR
    // -----------------------------------------------------------------------
    void checkBattery() {
        float batt{0.0f};
        while (battery_level_in.tryConsume(batt)) {
            last_batt_soc = batt; // track for recovery logic
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
    // Returns: true if any component is in WARNING or worse
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
                std::snprintf(msg, sizeof(msg), "FDIR: %s WARN (%u errs)",
                    comp->getName().data(), comp->getErrorCount());
                emit(Severity::WARNING, msg);
            }
        }

        if (any_critical) escalate(MissionState::SAFE_MODE);
        else if (any_warning) escalate(MissionState::DEGRADED);
    }

    // -----------------------------------------------------------------------
    // F-15: DEGRADED → NOMINAL auto-recovery.
    // Only applies to DEGRADED. SAFE_MODE and EMERGENCY require ground command.
    // Conditions: all subsystems NOMINAL + battery above DEGRADED threshold.
    // -----------------------------------------------------------------------
    void tryRecoverDegraded() {
        if (mission_state != MissionState::DEGRADED) return;

        bool all_nominal = true;
        for (size_t i = 0; i < monitored_count; ++i) {
            if (monitored_systems[i]->reportHealth() != HealthStatus::NOMINAL) {
                all_nominal = false;
                break;
            }
        }

        if (all_nominal && last_batt_soc > 15.0f) {
            emit(Severity::INFO, "FDIR: Recovery -> NOMINAL");
            mission_state = MissionState::NOMINAL;
            std::cout << "[" << getName() << "] Mission recovered to NOMINAL\n";
        }
    }

    // -----------------------------------------------------------------------
    // Component restart (ActiveComponent only)
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
            std::snprintf(msg, sizeof(msg), "FSM: %s->%s",
                missionStateName(mission_state), missionStateName(next));
            mission_state = next;
            emit(Severity::CRITICAL, msg);
            std::cout << "[" << getName() << "] " << msg << std::endl;
        }
    }

    void emit(uint32_t sev, const char* msg) {
        event_out.send(EventPacket::create(sev, getId(), msg));
    }
};

} // namespace deltav