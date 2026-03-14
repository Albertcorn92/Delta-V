# DELTA-V Developer Guide  v4.0

---

## 1. Introduction

This guide covers component creation, topology updates, generated files, and the main validation targets. Read `docs/ARCHITECTURE.md` first if the runtime layout is unfamiliar.

---

## 2. Creating a Component

### 2.1 Scaffold with dv-util

```bash
# One-time setup
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

# Optional walkthrough
python3 tools/dv-util.py guide

# Optional baseline app bundle
python3 tools/dv-util.py install-standard-apps

# Interactive wizard
python3 tools/dv-util.py boot-menu

# One-command scaffold
python3 tools/dv-util.py quickstart-component ThermalControl --build

# Passive component
python3 tools/dv-util.py add-component ThermalControl --profile utility

# Active component
python3 tools/dv-util.py add-component ThermalControl --active --profile controller

# Register directly in topology.yaml
python3 tools/dv-util.py add-component ThermalControl --register
```

The boot menu quickstart path can scaffold a component, add the first command,
run `tools/autocoder.py`, and optionally build `flight_software`.

The `install-standard-apps` path adds a baseline bundle used by many flight
teams:

- `CommandSequencerComponent`
- `FileTransferComponent`
- `MemoryDwellComponent` (dwell + patch controls)
- `TimeSyncComponent`
- `PlaybackComponent`
- `OtaComponent`
- `AtsRtsSequencerComponent` (absolute + relative time scripting)
- `LimitCheckerComponent` (dynamic warning/critical thresholds)
- `CfdpComponent` (missing-chunk tracking for lossy links)
- `ModeManagerComponent` (operational mode orchestration)

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
python3 tools/autocoder.py --check
```

`tools/autocoder.py` regenerates `src/Types.hpp`, `src/TopologyManager.hpp`,
and `dictionary.json`. `--check` verifies that the committed generated files are current.

Do not edit generated files by hand.

---

## 4. Requirements and Test Traceability

Every significant behaviour must have a requirement ID from `src/Requirements.hpp`. Reference it in associated unit tests:

```cpp
// Verifies DV-FDIR-01: system enters SAFE_MODE when battery < 5%
TEST(WatchdogComponent, BatterySafeModeThreshold) {
    ...
}
```

Traceability is used by the release and qualification tooling. Each behavior should point to a requirement ID and a test that exercises it.

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
| Generated-file check | `cmake --build build --target autocoder_check` | Verify generated files are current |
| Flight binary | `cmake --build build --target flight_software` | SITL executable |
| Unit tests | `cmake --build build --target run_tests` | GTest suite |
| System tests | `cmake --build build --target run_system_tests` | End-to-end integration checks |
| V&V stress | `cmake --build build --target vnv_stress` | Shuffled repeat stability gate |
| Coverage configure | `bash tools/configure_coverage_build.sh` | Configure the dedicated GCC coverage build |
| Coverage report | `cmake --build build_cov --target coverage` | lcov HTML report |
| Coverage guard | `cmake --build build_cov --target coverage_guard` | Enforce minimum coverage thresholds |
| Coverage trend | `cmake --build build_cov --target coverage_trend` | Emit coverage trend JSON snapshot |
| Static analysis | `cmake --build build --target tidy` | clang-tidy |
| Benchmark baseline | `cmake --build build --target benchmark_baseline` | Refresh benchmark evidence artifacts |
| Benchmark guard | `cmake --build build --target benchmark_guard` | Enforce perf regression thresholds |
| SITL smoke | `cmake --build build --target sitl_smoke` | Short runtime marker/fatal scan |
| SITL soak | `cmake --build build --target sitl_soak` | Extended runtime stability gate |
| SITL fault campaign | `cmake --build build --target sitl_fault_campaign` | Live UDP malformed/replay/valid traffic campaign |
| Portability matrix | `cmake --build build --target portability_matrix` | Validate host/SITL software profile portability |
| Quickstart gate | `cmake --build build --target quickstart_10min` | One-command local validation path |
| Flight readiness gate | `cmake --build build --target flight_readiness` | Legal + tests + safety + traceability |
| Qualification bundle | `cmake --build build --target qualification_bundle` | Evidence report and artifact hashes |
| Software final gate | `cmake --build build --target software_final` | Sync docs evidence + final software check |
| Release preflight | `cmake --build build --target release_preflight` | Record current release blockers without requiring a clean tag |
| Release candidate gate | `cmake --build build --target release_candidate` | Require clean tagged baseline and generate current release pedigree |
| Review bundle | `cmake --build build --target review_bundle` | Stage a curated reviewer package and zip |
| CubeSat readiness report | `cmake --build build --target cubesat_readiness` | Consolidated framework + mission gap snapshot |

Safety-case starter templates for mission teams are in `docs/safety_case/`.

---

## 8. GDS Workflow

1. Start flight software: `./build/flight_software`
2. Start GDS: `streamlit run gds/gds_dash.py`
3. Use the sidebar command console to filter, inspect, and send commands from `dictionary.json`
4. Track startup readiness in the interactive boot checklist tab
5. Events appear in `gds/runtime/events.log` and the event stream panel
6. Telemetry is logged to `gds/runtime/live_telem.csv` and plotted in real time

For radio-integration work without UDP:

```bash
export DELTAV_LINK_MODE=serial_kiss
export DELTAV_SERIAL_PORT=/dev/tty.usbserial-0001
export DELTAV_SERIAL_BAUD=115200
./build/flight_software
```

---

## 9. Adding a Command to the FSM Policy

FSM policy is topology-driven. Add/update the command in `topology.yaml` and set
`op_class`, then regenerate:

```yaml
commands:
  - name: "MY_NEW_COMMAND"
    target_id: 200
    opcode: 50
    op_class: "RESTRICTED"
    description: "Example restricted command"
```

```bash
python3 tools/autocoder.py
```

`OpClass::HOUSEKEEPING` = allowed in `BOOT`, `NOMINAL`, `DEGRADED`, and `SAFE_MODE`.
`OpClass::OPERATIONAL` = blocked in `SAFE_MODE` and `EMERGENCY`.
`OpClass::RESTRICTED` = allowed in `NOMINAL` only.
