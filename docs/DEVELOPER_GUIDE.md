# DELTA-V Developer Guide  v4.0

---

## 1. Introduction

This guide covers creating custom flight software components, integrating them into the DELTA-V topology, and verifying them against the framework's DO-178C-influenced requirements. If you are new to the project, read `ARCHITECTURE.md` first.

---

## 2. Creating a Component

### 2.1 Scaffold with dv-util

```bash
# Passive component (runs in the scheduler master thread)
python3 tools/dv-util.py add-component ThermalControl

# Active component (runs on its own Os::Thread)
python3 tools/dv-util.py add-component ThermalControl --active
```

This creates `src/ThermalControlComponent.hpp` with all boilerplate pre-filled.

### 2.2 Component Checklist

Every component must:

- Inherit from `deltav::Component` (passive) or `deltav::ActiveComponent` (threaded)
- Call `recordError()` on every fault path — this feeds FDIR
- Drain `cmd_in` fully on every `step()` call (use `while (cmd_in.tryConsume(cmd))`)
- Use `TimeService::getMET()` for all timestamps — never `std::chrono` directly
- Handle the `default:` case in `handleCommand()` with `recordError()` + event emission

### 2.3 Ports

| Port type | Direction | Typical use |
|---|---|---|
| `InputPort<CommandPacket>` | Inbound | Commands from CommandHub |
| `OutputPort<Serializer::ByteArray>` | Outbound | Telemetry to TelemHub |
| `OutputPort<EventPacket>` | Outbound | Events to EventHub |

Overflow is automatically tracked by the `RingBuffer`. Wire critical ports into `WatchdogComponent::pollPortOverflow()` for FDIR visibility.

---

## 3. Updating topology.yaml

```yaml
components:
  - id: 400                    # Must be unique across all components
    name: "ThermalControl"
    class: "ThermalControlComponent"
    instance: "thermal"
    type: "Passive"

commands:
  - name: "SET_HEATER_TARGET"
    target_id: 400
    opcode: 1
    description: "Set target temperature in degrees C"
```

Run the autocoder after every topology change:

```bash
python3 tools/autocoder.py
```

This regenerates `src/Types.hpp` (packet definitions) and `dictionary.json` (GDS dictionary). **Never edit these files by hand.**

---

## 4. Requirements and Test Traceability

Every significant behaviour must have a requirement ID from `src/Requirements.hpp`. Reference it in your unit test:

```cpp
// Verifies DV-FDIR-01: system enters SAFE_MODE when battery < 5%
TEST(WatchdogComponent, BatterySafeModeThreshold) {
    ...
}
```

This is how high-assurance projects survive design reviews. When a reviewer asks "why does this code exist?", the answer is `DV-FDIR-01` and the unit test is the proof.

---

## 5. Protecting Critical Parameters with TMR

For parameters used in safety-critical calculations:

```cpp
#include "TmrStore.hpp"

// Declare a TMR-protected gain
TmrStore<float> kp_gain{1.5f};

// Register with the scrubber (call once in init())
TmrRegistry::getInstance().registerStore("kp_gain", &kp_gain);

// Use in step() — always read through vote()
float kp = kp_gain.read();
```

The Watchdog calls `TmrRegistry::scrubAll()` every 30 scheduler cycles to repair any SEU-corrupted copies.

---

## 6. Fault Injection Testing

The `WatchdogComponent::injectBatteryLevel()` hook allows tests to drive the mission FSM without waiting for real battery discharge:

```cpp
// In a unit test or HIL test script:
topology.watchdog.injectBatteryLevel(4.0f);  // triggers SAFE_MODE
EXPECT_EQ(topology.watchdog.getMissionState(), MissionState::SAFE_MODE);

topology.watchdog.injectBatteryLevel(1.0f);  // triggers EMERGENCY
EXPECT_EQ(topology.watchdog.getMissionState(), MissionState::EMERGENCY);
```

For I2C fault injection, subclass `hal::MockI2c` and override `read()` to return `false` after N calls:

```cpp
class FaultI2c : public deltav::hal::MockI2c {
    int calls = 0;
    bool read(...) override {
        if (++calls > 5) return false; // simulate bus failure
        return MockI2c::read(...);
    }
};
```

---

## 7. Build Targets

| Target | Command | Purpose |
|---|---|---|
| Flight binary | `cmake --build . --target flight_software` | SITL executable |
| Unit tests | `cmake --build . --target run_tests` | GTest suite |
| System tests | `cmake --build . --target run_system_tests` | End-to-end integration checks |
| V&V stress | `cmake --build . --target vnv_stress` | Shuffled repeat stability gate |
| Coverage report | `cmake --build . --target coverage` | lcov HTML report |
| Coverage guard | `cmake --build . --target coverage_guard` | Enforce minimum coverage thresholds |
| Coverage trend | `cmake --build . --target coverage_trend` | Emit coverage trend JSON snapshot |
| Static analysis | `cmake --build . --target tidy` | clang-tidy |
| Benchmark baseline | `cmake --build . --target benchmark_baseline` | Refresh benchmark evidence artifacts |
| Benchmark guard | `cmake --build . --target benchmark_guard` | Enforce perf regression thresholds |
| SITL smoke | `cmake --build . --target sitl_smoke` | Short runtime marker/fatal scan |
| SITL soak | `cmake --build . --target sitl_soak` | Extended runtime stability gate |
| Quickstart gate | `cmake --build . --target quickstart_10min` | One-command local validation path |
| Flight readiness gate | `cmake --build . --target flight_readiness` | Legal + tests + safety + traceability |
| Qualification bundle | `cmake --build . --target qualification_bundle` | Evidence report and artifact hashes |
| Software final gate | `cmake --build . --target software_final` | Sync docs evidence + final software check |

Safety-case starter templates for mission teams are in `docs/safety_case/`.

---

## 8. GDS Workflow

1. Start flight software: `./build/flight_software`
2. Start GDS: `streamlit run gds/gds_dash.py`
3. Use the sidebar to select and send commands — the dictionary auto-loads from `dictionary.json`
4. Events appear in `flight_events.log` and the GDS event panel
5. Telemetry is logged to `flight_log.csv` and plotted in real time

---

## 9. Adding a Command to the FSM Policy

If your new command is safety-critical (should be blocked in `SAFE_MODE`), add it to `src/MissionFsm.hpp`:

```cpp
constexpr std::array<OpcodePolicy, N> OPCODE_TABLE = {{
    ...
    { 50, OpClass::RESTRICTED },  // MY_NEW_COMMAND — blocked in SAFE_MODE/EMERGENCY
}};
```

`OpClass::HOUSEKEEPING` = allowed in all states.
`OpClass::OPERATIONAL` = blocked in SAFE_MODE and EMERGENCY.
`OpClass::RESTRICTED` = allowed in NOMINAL only.
