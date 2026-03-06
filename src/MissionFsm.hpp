#pragma once
// =============================================================================
// MissionFsm.hpp — DELTA-V Formal Mission Finite State Machine
// =============================================================================
// Governs WHICH commands and operations are legal in WHICH mission state.
// This prevents dangerous sequences like ACTUATOR_ENABLE during SAFE_MODE.
//
// Architecture:
//   - MissionState enum is the authoritative state (owned by WatchdogComponent)
//   - MissionFsm::isAllowed(state, opcode) is called by CommandHub BEFORE
//     dispatching any uplinked command
//   - Forbidden commands return NACK with reason "FSM_BLOCKED"
//
// Transition table (read-only, computed at compile time):
//   BOOT        → only PING (opcode 0) allowed
//   NOMINAL     → all commands allowed
//   DEGRADED    → all commands allowed (degraded but flying)
//   SAFE_MODE   → only housekeeping commands allowed (RESET_BATTERY, PING)
//   EMERGENCY   → NO commands allowed (await reboot or ground intervention)
//
// DO-178C: Zero dynamic allocation. Lookup is a constexpr table search in O(1).
// =============================================================================
#include <cstdint>
#include <array>

namespace deltav {

// MissionState must match WatchdogComponent.hpp enum
enum class MissionState : uint8_t;

// ---------------------------------------------------------------------------
// OpClass — categorises each opcode for FSM gating
// ---------------------------------------------------------------------------
enum class OpClass : uint8_t {
    HOUSEKEEPING   = 0, // Safe in all states (e.g. PING, RESET_BATTERY)
    OPERATIONAL    = 1, // Allowed in NOMINAL and DEGRADED only
    RESTRICTED     = 2, // Allowed in NOMINAL only (e.g. high-risk actuator commands)
};

// ---------------------------------------------------------------------------
// OpcodePolicy — maps opcode values to their class
// Add new opcodes here. Default (unlisted) = OPERATIONAL.
// ---------------------------------------------------------------------------
struct OpcodePolicy {
    uint32_t opcode;
    OpClass  cls;
};

constexpr std::array<OpcodePolicy, 8> OPCODE_TABLE = {{
    { 0,  OpClass::HOUSEKEEPING },  // PING
    { 1,  OpClass::HOUSEKEEPING },  // RESET_BATTERY
    { 2,  OpClass::OPERATIONAL  },  // SET_DRAIN_RATE
    { 3,  OpClass::OPERATIONAL  },  // SET_AMPLITUDE
    { 10, OpClass::RESTRICTED   },  // ACTUATOR_ENABLE (future)
    { 11, OpClass::RESTRICTED   },  // DEPLOY_APPENDAGE (future)
    { 20, OpClass::HOUSEKEEPING },  // REQUEST_TELEMETRY_DUMP
    { 99, OpClass::OPERATIONAL  },  // DEBUG_MODE
}};

// ---------------------------------------------------------------------------
// MissionFsm — stateless helper (all logic is pure function calls)
// ---------------------------------------------------------------------------
class MissionFsm {
public:
    // Returns true if the opcode is permitted in the current mission state.
    static bool isAllowed(uint8_t state_raw, uint32_t opcode) {
        return isAllowedClass(state_raw, classify(opcode));
    }

    static bool isAllowed(MissionState state, uint32_t opcode) {
        return isAllowed(static_cast<uint8_t>(state), opcode);
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

    static OpClass classify(uint32_t opcode) {
        for (const auto& entry : OPCODE_TABLE) {
            if (entry.opcode == opcode) return entry.cls;
        }
        return OpClass::OPERATIONAL; // default
    }
};

} // namespace deltav
