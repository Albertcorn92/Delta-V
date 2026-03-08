# 🛰️ DELTA-V Autonomy Framework  v4.0

DELTA-V is a high-performance, zero-copy **C++20 flight software framework** for civilian aerospace, robotics, and research missions. It is designed for deterministic behavior and high assurance without heavyweight queues, XML dictionaries, or complex toolchains.

The framework targets **DO-178C DAL-B compliance** and is designed for embedded aerospace systems from prototype SITL on macOS/Linux through to FreeRTOS on Cortex-M7 hardware.

Roadmap for raising assurance rigor: `docs/HIGH_ASSURANCE_ROADMAP.md`.
Safety gates and traceability workflow: `docs/SAFETY_ASSURANCE.md`.
Documentation map for first-time users and mission teams: `docs/README.md`.
ESP32 hardware bring-up plan: `docs/ESP32_BRINGUP.md`.
ESP32 no-sensor baseline evidence: `docs/ESP32_SENSORLESS_BASELINE.md`.
ESP32 golden-image bootchain workflow: `docs/ESP32_GOLDEN_IMAGE_BOOTCHAIN.md`.
Coverage threshold ramp policy: `docs/COVERAGE_RAMP_PLAN.md`.
Mission safety-case starter templates: `docs/safety_case/README.md`.

---

## 🚀 Philosophy: Compile-Time Mission Safety

Using **C++20 Concepts and Template Metaprogramming**, DELTA-V moves system validation to the **compiler**. Component wiring and data types are verified at compile time. If the spacecraft's internal architecture is incorrect, **the program will not compile**.

In host/SITL builds, strict runtime heap lock is optional and can be enabled with
`DELTAV_ENABLE_HOST_HEAP_GUARD=1`.
On ESP builds, heap lock is intentionally disabled in the default profile to keep
runtime compatibility with ESP-IDF/FreeRTOS internals.

---

## 🧱 What's New in v4.0

| Feature | Description |
|---|---|
| **HeapGuard** | Optional strict host runtime heap lock via `DELTAV_ENABLE_HOST_HEAP_GUARD=1` |
| **TmrStore** | Triple Modular Redundancy for critical params — SEU self-healing via majority vote |
| **KISS Serial Framing** | UART-friendly framing/escaping for serial radio links |
| **MissionFsm** | Formal FSM gates uplink commands by state: SAFE_MODE blocks OPERATIONAL opcodes |
| **Requirements.hpp** | RTM header — every DV-XXX-NN requirement linked to tests |
| **FaultInjection** | `WatchdogComponent::injectBatteryLevel()` hook for HIL destructive testing |
| **TMR Scrub** | Watchdog calls `TmrRegistry::scrubAll()` every 30 cycles |
| **System Tests** | End-to-end command/event/telemetry integration tests via `run_system_tests` |
| **Bench Baseline** | Reproducible software performance metrics (uplink, CRC-16, COBS) |
| **SITL Smoke + Quickstart** | Local runtime smoke validation and one-command onboarding flow |
| **Serial-KISS Link Mode** | Optional UART transport for CCSDS frames over KISS instead of UDP |
| **Ops App Bundle** | Built-in Sequencer, File Transfer, Dwell/Patch, Time Sync, Playback, and OTA managers |
| **Expanded HAL** | Added UART and PWM HAL interfaces with deterministic SITL mock drivers |
| **Store & Forward** | Playback component replays historical telemetry from recorder logs |
| **Time Sync** | Ground-driven UTC sync via staged "time at tone" words |
| **OTA Manager** | CRC32-verified update staging with reboot request signaling |
| **ATS/RTS Sequencer** | Absolute and relative time script execution with event triggers |
| **Limit Checker** | Ground-updatable warn/critical thresholds for dynamic alarms |
| **CFDP-Style Transfer** | Chunk tracking + missing-chunk reporting for lossy links |
| **Mode Manager** | Mode-driven command orchestration for detumble/sun/science/downlink |
| **Golden Bootchain Kit** | ESP32 partition + CRC-only workflow for golden-image fallback design |

---

## 🏗️ System Architecture

### Flight Software Layers (C++20)

```
┌─────────────────────────────────────────────────┐
│  Ground Data System (Python / Streamlit)         │
│  gds/gds_dash.py ←UDP 9001→ TelemetryBridge      │
│               ←UDP 9002→  (CCSDS command path)     │
└───────────────────┬─────────────────────────────┘
                    │
┌───────────────────▼─────────────────────────────┐
│  TelemetryBridge (ActiveComponent, 20 Hz)        │
│  Downlink: CCSDS + CRC-16 (UDP or serial-KISS)   │
│  Uplink:   CCSDS command frame validation         │
└────┬──────────────┬──────────────────────────────┘
     │              │
┌────▼────┐   ┌─────▼──────┐   ┌─────────────────┐
│TelemHub │   │ CommandHub │   │   EventHub      │
│ Fan-Out │   │ +MissionFsm│   │  Broadcast      │
└────┬────┘   └─────┬──────┘   └────────┬────────┘
     │              │                   │
┌────▼──────────────▼───────────────────▼────────┐
│           Component Layer (Passive)             │
│  StarTracker  BatterySystem  ImuComponent       │
│  SensorComp   PowerComp      (HAL-injected)     │
└────────────────────┬────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────┐
│  WatchdogComponent (FDIR Supervisor)             │
│  MissionFsm  ·  TmrRegistry  ·  ParamDb CRC     │
└─────────────────────────────────────────────────┘
```

---

## 🛠️ Building

### Prerequisites

- C++20: Clang 14+ or GCC 11+
- CMake 3.15+
- Python 3.9+ (GDS dashboard)
- `python3 -m venv .venv && source .venv/bin/activate`
- `pip install -r requirements.txt`
- `python3 tools/dv-util.py guide` (optional first-run walkthrough)

### Compile

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Run SITL

```bash
# Terminal 1 — Flight Software
./build/flight_software

# Terminal 2 — GDS Dashboard
streamlit run gds/gds_dash.py
```

### Run with Serial-KISS Link (No UDP)

```bash
export DELTAV_LINK_MODE=serial_kiss
export DELTAV_SERIAL_PORT=/dev/tty.usbserial-0001
export DELTAV_SERIAL_BAUD=115200
./build/flight_software
```

This mode keeps CCSDS framing and anti-replay checks while switching transport
from UDP sockets to UART KISS framing for radio-oriented integration work.

### Install Standard Civilian Ops Apps

```bash
python3 tools/dv-util.py install-standard-apps
```

This installs/wires baseline mission ops components:

- `CommandSequencerComponent`
- `FileTransferComponent`
- `MemoryDwellComponent` (dwell + patch command set)
- `TimeSyncComponent`
- `PlaybackComponent`
- `OtaComponent`
- `AtsRtsSequencerComponent`
- `LimitCheckerComponent`
- `CfdpComponent`
- `ModeManagerComponent`

### ESP32-S3 (Local-Only Baseline)

```bash
source $HOME/esp/esp-idf/export.sh
cd ports/esp32
idf.py set-target esp32s3
idf.py -B build_esp32 build flash -p /dev/cu.usbmodem101
idf.py -B build_esp32 -p /dev/cu.usbmodem101 monitor
```

### ESP32-S3 Sensorless Soak (Automated)

```bash
source $HOME/esp/esp-idf/export.sh
python3 tools/esp32_soak.py \
  --project-dir ports/esp32 \
  --build-dir build_esp32 \
  --port /dev/cu.usbmodem101 \
  --duration 1800
```

Expected runtime lines:

- `[RadioLink] Local-only mode: UDP bridge disabled.`
- `[RGE] Embedded cooperative scheduler running.`

Port-specific quick reference: `ports/esp32/README.md`.

### ESP32-S3 Runtime Guard (WCET + Stack)

```bash
source $HOME/esp/esp-idf/export.sh
python3 tools/esp32_runtime_guard.py \
  --project-dir ports/esp32 \
  --build-dir build_esp32 \
  --port /dev/cu.usbmodem101 \
  --duration 300
```

This validates scheduler runtime metrics against
`docs/ESP32_RUNTIME_THRESHOLDS.json`.

### ESP32-S3 Reboot Stability Campaign

```bash
source $HOME/esp/esp-idf/export.sh
python3 tools/esp32_reboot_campaign.py \
  --project-dir ports/esp32 \
  --build-dir build_esp32 \
  --port /dev/cu.usbmodem101 \
  --cycles 10 \
  --cycle-seconds 12
```

### Run Unit Tests

```bash
cd build && ctest --output-on-failure
```

### Generate Coverage + Enforce Thresholds

```bash
cmake -B build_cov -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=gcc-12 \
  -DCMAKE_CXX_COMPILER=g++-12
cmake --build build_cov --target coverage
cmake --build build_cov --target coverage_guard
```

### Run System Integration Tests

```bash
cmake --build build --target run_system_tests
ctest --test-dir build --output-on-failure --timeout 90
```

### Run V&V Stress Gate

```bash
cmake --build build --target vnv_stress
```

### Generate Software Benchmark Baseline

```bash
cmake --build build --target benchmark_baseline
# writes docs/BENCHMARK_BASELINE.{md,json}
```

### Run Benchmark Regression Guard

```bash
cmake --build build --target benchmark_guard
```

### Run SITL Smoke Check

```bash
cmake --build build --target sitl_smoke
```

### Run Extended SITL Soak

```bash
cmake --build build --target sitl_soak
```

### 10-Minute Local Validation (Recommended)

```bash
cmake --build build --target quickstart_10min
```

Reference: `docs/QUICKSTART_10_MIN.md`

### Validate Requirements Traceability

```bash
cmake --build build --target traceability
# emits build/requirements_trace_matrix.md and requirements_trace_matrix.json
```

### Run Full Flight Readiness Gate

```bash
cmake --build build --target flight_readiness
```

### Generate Qualification Evidence Bundle

```bash
cmake --build build --target qualification_bundle
# emits build/qualification/qualification_report.{md,json}
```

### Finalize Software-Only Release Evidence

```bash
cmake --build build --target software_final
# syncs docs/{REQUIREMENTS_TRACE_MATRIX.*,qualification_report.*}
# emits docs/SOFTWARE_FINAL_STATUS.md
# includes benchmark_guard + sitl_soak dependencies
```

### Generate CubeSat Readiness Snapshot

```bash
cmake --build build --target cubesat_readiness
# emits docs/CUBESAT_READINESS_STATUS.{md,json}
```

### Generate Scope-Limited Readiness Snapshot

```bash
cmake --build build --target cubesat_readiness_scope
# emits docs/CUBESAT_READINESS_STATUS_SCOPE.{md,json}
# applies scope waivers for: esp32-soak-1h, sensor-attached-evidence
# marks those checks as WAIVED with the explicit --exclude-check reason
```

Process templates for mission certification work are in `docs/process/`.
Guided documentation index is in `docs/README.md`.
Open-source release checklist is in `docs/OPEN_SOURCE_RELEASE_CHECKLIST.md`.
Mission assurance checklist is in `docs/MISSION_ASSURANCE_CHECKLIST.md`.
CubeSat mission-team readiness guide is in `docs/CUBESAT_TEAM_READINESS.md`.
Latest release notes snapshot is in `docs/RELEASE_NOTES_20260308.md`.
Performance methodology is in `docs/BENCHMARK_PROTOCOL.md`.
Coverage policy is in `docs/COVERAGE_POLICY.md`.

---

## 📝 Adding a New Component

### Quick scaffold

```bash
# Interactive wizard (recommended)
python3 tools/dv-util.py boot-menu
# optional first-time setup of standard mission ops components
python3 tools/dv-util.py install-standard-apps
# then choose: "Quickstart: component + command + regenerate"
# use "Help: how the boot flow works" any time in the menu

# One command (best for first-time setup)
python3 tools/dv-util.py quickstart-component ThermalControl --build

# Non-interactive scaffolding
python3 tools/dv-util.py add-component ThermalControl --profile controller --register
python3 tools/dv-util.py add-command SET_HEATER --target-id 400 --opcode 1 \
  --description "Set heater target temperature (deg C)"
```

### Manual steps

1. Add the component to `topology.yaml` (or use `--register` in `add-component`)
2. Run `python3 tools/autocoder.py` to regenerate `Types.hpp` and `dictionary.json`
3. Wire ports in `TopologyManager.hpp` (or re-run the autocoder)
4. Add unit tests referencing the relevant DV-XXX-NN requirements

---

## 🔐 Security

Uplink commands use strict CCSDS command-frame validation plus anti-replay sequence checks. The `MissionFsm` additionally gates commands by mission state. Restricted commands (e.g. actuator enable/deploy operations) are blocked in `SAFE_MODE` and `EMERGENCY`.

The baseline framework intentionally does not include command-path encryption
features.

Set replay-state persistence path before launch:

```bash
export DELTAV_REPLAY_SEQ_FILE="replay_seq.db"
```

Enable local SITL command ingest only when needed:

```bash
export DELTAV_ENABLE_UNAUTH_UPLINK=1
# Optional: restrict accepted source IP (default is 127.0.0.1)
export DELTAV_UPLINK_ALLOW_IP="127.0.0.1"
```

---

## 📋 Requirements Traceability

See `src/Requirements.hpp` for the full RTM. Every unit test references its governing requirement ID in a comment (e.g. `// DV-FDIR-01`).

---

## ⚖️ Legal Scope

- DELTA-V is scoped for civilian, scientific, and educational use.
- Contributions that add military, weapons, targeting, or fire-control behavior are out of scope and will be rejected.
- Command-path cryptography/encryption features are intentionally excluded from this baseline and blocked by legal policy checks.
- This repository is intended for research/prototyping and is not certified for operational or safety-critical deployment.
- This repository includes compliance guidance, but it is **not legal advice** and does not provide legal clearance by itself.
- Maintainer does not provide direct operational support to non-U.S. users.
- See `docs/CIVILIAN_USE_POLICY.md` and `docs/EXPORT_CONTROL_NOTE.md` before release or deployment.
- Read `docs/LEGAL_FAQ.md` for plain-language release/deployment legal questions.
- Maintainer release checklist: `docs/LEGAL_SCOPE_CHECKLIST.md`.
- Maintainer interaction boundary: `docs/MAINTAINER_BOUNDARY_POLICY.md`.

---

## 🧪 Fault Injection / HIL Testing

```cpp
// Simulate battery drain without waiting for real discharge:
topology.watchdog.injectBatteryLevel(4.0f); // → SAFE_MODE
topology.watchdog.injectBatteryLevel(1.5f); // → EMERGENCY

// Simulate a stuck I2C bus via MockI2c override in unit tests
```

---

## Author

**Albert Cornelius** · Apache License 2.0

## Project Governance

- Contributing guide: `CONTRIBUTING.md`
- Security policy: `SECURITY.md`
- Code of conduct: `CODE_OF_CONDUCT.md`
- Release notes: `CHANGELOG.md`
- Disclaimer: `DISCLAIMER.md`
- Civilian use policy: `docs/CIVILIAN_USE_POLICY.md`
- Export-control note: `docs/EXPORT_CONTROL_NOTE.md`
- Maintainer boundary policy: `docs/MAINTAINER_BOUNDARY_POLICY.md`
