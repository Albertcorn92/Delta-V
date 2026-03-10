#pragma once
// =============================================================================
// MissionFsm.hpp — DELTA-V Formal Mission Finite State Machine
// =============================================================================
// Governs WHICH command classes are legal in WHICH mission state.
// This prevents dangerous sequences like operational/restricted commands
// being dispatched during SAFE_MODE or EMERGENCY.
//
// Architecture:
//   - MissionState enum is the authoritative state (owned by WatchdogComponent)
//   - CommandHub classifies each command with a topology-derived OpClass policy
//     keyed by (target_id, opcode)
//   - MissionFsm::isAllowed(state, op_class) is called by CommandHub BEFORE
//     dispatching any uplinked command
//   - Forbidden commands return NACK with reason "FSM_BLOCKED"
//
// Transition table (read-only, computed at compile time):
//   BOOT        → housekeeping only
//   NOMINAL     → all command classes allowed
//   DEGRADED    → housekeeping + operational
//   SAFE_MODE   → housekeeping only
//   EMERGENCY   → NO commands allowed (await reboot or ground intervention)
//
// DO-178C: Zero dynamic allocation. Pure switch-based policy checks.
// =============================================================================
#include <cstdint>

namespace deltav {

// MissionState must match WatchdogComponent.hpp enum
enum class MissionState : uint8_t;

// ---------------------------------------------------------------------------
// OpClass — categorises each opcode for FSM gating
// ---------------------------------------------------------------------------
enum class OpClass : uint8_t {
    HOUSEKEEPING   = 0, // Safe in BOOT/NOMINAL/DEGRADED/SAFE_MODE
    OPERATIONAL    = 1, // Allowed in NOMINAL and DEGRADED only
    RESTRICTED     = 2, // Allowed in NOMINAL only (e.g. high-risk actuator commands)
};

// ---------------------------------------------------------------------------
// MissionFsm — stateless helper (all logic is pure function calls)
// ---------------------------------------------------------------------------
class MissionFsm {
public:
    // Returns true if the command class is permitted in the current mission state.
    static bool isAllowed(uint8_t state_raw, OpClass cls) {
        return isAllowedClass(state_raw, cls);
    }

    static bool isAllowed(MissionState state, OpClass cls) {
        return isAllowedClass(static_cast<uint8_t>(state), cls);
    }

    static const char* stateName(uint8_t state_raw) {
        switch (state_raw) {
            case 0: return "BOOT";
            case 1: return "NOMINAL";
            case 2: return "DEGRADED";
            case 3: return "SAFE_MODE";
            case 4: return "EMERGENCY";
            default: return "UNKNOWN";
        }
    }

private:
    static bool isAllowedClass(uint8_t state_raw, OpClass cls) {
        switch (state_raw) {
            case 0: // BOOT
                return cls == OpClass::HOUSEKEEPING;
            case 1: // NOMINAL
                return true; // all classes
            case 2: // DEGRADED
                return cls == OpClass::HOUSEKEEPING ||
                       cls == OpClass::OPERATIONAL;
            case 3: // SAFE_MODE
                return cls == OpClass::HOUSEKEEPING;
            case 4: // EMERGENCY
                return false; // no commands accepted
            default:
                return false;
        }
    }
};

} // namespace deltav
