# DELTA-V Autonomy Framework v4.0

[![DELTA-V CI](https://github.com/Albertcorn92/Delta-V/actions/workflows/ci.yml/badge.svg)](https://github.com/Albertcorn92/Delta-V/actions/workflows/ci.yml)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Latest Tag](https://img.shields.io/github/v/tag/Albertcorn92/Delta-V)](https://github.com/Albertcorn92/Delta-V/tags)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)](https://en.cppreference.com/w/cpp/20)

DELTA-V is a zero-copy C++20 flight software framework for civilian aerospace, robotics, and research missions. It is built for deterministic runtime behavior, compile-time safety, and fast SITL-to-embedded bring-up without heavyweight tooling.

`software_final: PASS` `requirements: 37/37` `civilian-scope: enforced` `flight-ready: pending physical evidence`

## Quick Links

- [10-minute quickstart](docs/QUICKSTART_10_MIN.md)
- [Software final status](docs/SOFTWARE_FINAL_STATUS.md)
- [CubeSat readiness status](docs/CUBESAT_READINESS_STATUS.md)
- [Safety assurance docs](docs/SAFETY_ASSURANCE.md)
- [Legal scope checklist](docs/LEGAL_SCOPE_CHECKLIST.md)

## Get Started in 60 Seconds

```bash
# New project from this framework
./bootstrap.sh my_spacecraft
cd my_spacecraft

# Immediate local validation gate
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target quickstart_10min
```

What this proves quickly:
- Project scaffolding works end-to-end.
- Build/test wiring is functional on your machine.
- You can see real framework behavior before reading deep docs.

If you already cloned this repo directly:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target quickstart_10min
```

Quickstart details: `docs/QUICKSTART_10_MIN.md`

## What DELTA-V Is

DELTA-V pushes mission safety checks earlier into the compiler:
- Component wiring and interface contracts are validated at compile time.
- Runtime data paths favor fixed-layout packets and zero-copy transfer.
- Deterministic scheduling and explicit fault-management primitives are first-class.

Host/SITL heap lock can be enabled with `DELTAV_ENABLE_HOST_HEAP_GUARD=1`.  
ESP builds keep heap lock disabled by default for ESP-IDF/FreeRTOS compatibility.

## Standard Civilian Ops Apps (Included)

Install the baseline ops bundle:

```bash
python3 tools/dv-util.py install-standard-apps
```

Included app set:
- `CommandSequencerComponent`
- `FileTransferComponent`
- `MemoryDwellComponent` (dwell + patch)
- `TimeSyncComponent`
- `PlaybackComponent`
- `OtaComponent`
- `AtsRtsSequencerComponent`
- `LimitCheckerComponent`
- `CfdpComponent`
- `ModeManagerComponent`

## Mission Assurance and Certification Status

Current snapshot (2026-03-09 UTC):
- `software_final`: PASS
- `requirements_traced_with_direct_tests`: `37/37`
- `framework_release_readiness`: `True`
- `cubesat_flight_readiness`: `False` (open physical evidence gates)

Open unwaived flight-ready items:
1. `esp32-soak-1h` passing evidence
2. Sensor-attached HIL evidence execution

Primary assurance artifacts:
- Software final status: `docs/SOFTWARE_FINAL_STATUS.md`
- CubeSat readiness status: `docs/CUBESAT_READINESS_STATUS.md`
- Scope-limited readiness snapshot: `docs/CUBESAT_READINESS_STATUS_SCOPE.md`
- Requirements trace matrix: `docs/REQUIREMENTS_TRACE_MATRIX.json`
- Qualification report: `docs/qualification_report.md`
- Safety assurance workflow: `docs/SAFETY_ASSURANCE.md`
- High-assurance roadmap: `docs/HIGH_ASSURANCE_ROADMAP.md`

## Architecture at a Glance

- Ground segment: Streamlit GDS (`gds/gds_dash.py`) with telemetry/event/command workflows.
- Link layer: CCSDS framing with CRC-16 over UDP or serial KISS (`DELTAV_LINK_MODE=serial_kiss`).
- Runtime core: `TelemetryBridge` + `CommandHub` + `TelemHub` + `EventHub`.
- Ops/FDIR: `MissionFsm`, `WatchdogComponent`, TMR-backed parameter storage, dynamic limits, ATS/RTS.

Deep architecture reference: `docs/ARCHITECTURE.md`

## Advanced and Program Gates

Use these after quickstart, not before it:

```bash
# Full software-only finalization evidence
cmake --build build --target software_final

# Readiness snapshots
cmake --build build --target cubesat_readiness
cmake --build build --target cubesat_readiness_scope
```

ESP32 mission-team evidence bundle:

```bash
python3 tools/team_ready_esp32.py --soak-duration 3600
```

More gate references:
- Mission assurance checklist: `docs/MISSION_ASSURANCE_CHECKLIST.md`
- Team closeout checklist: `docs/TEAM_READY_MASTER_CHECKLIST.md`
- Flight environment matrix: `docs/process/FLIGHT_ENV_TEST_MATRIX_20260307.md`
- Release manifest template: `docs/process/RELEASE_MANIFEST_20260307.md`

## Manual Build and SITL Run

Prereqs:
- C++20 compiler (Clang 14+ or GCC 11+)
- CMake 3.15+
- Python 3.9+

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Terminal 1
./build/flight_software

# Terminal 2
streamlit run gds/gds_dash.py
```

## Build Your Own Components

Fastest path:

```bash
python3 tools/dv-util.py boot-menu
```

One-command scaffold:

```bash
python3 tools/dv-util.py quickstart-component ThermalControl --build
```

Non-interactive path:

```bash
python3 tools/dv-util.py add-component ThermalControl --profile controller --register
python3 tools/dv-util.py add-command SET_HEATER --target-id 400 --opcode 1 \
  --description "Set heater target temperature (deg C)"
python3 tools/autocoder.py
```

## Security Notes

Uplink command path uses CCSDS frame validation with anti-replay sequence checks and mission-state gating (`MissionFsm`).

This baseline intentionally excludes command-path encryption features.

Replay-state persistence path:

```bash
export DELTAV_REPLAY_SEQ_FILE="replay_seq.db"
```

Local-only SITL command ingest (explicit opt-in):

```bash
export DELTAV_ENABLE_UNAUTH_UPLINK=1
export DELTAV_UPLINK_ALLOW_IP="127.0.0.1"
```

## Legal Scope

- DELTA-V is scoped for civilian, scientific, and educational use.
- Military, weapons, targeting, or fire-control behavior is out of scope and rejected.
- Command-path cryptography/encryption features are intentionally excluded in this baseline.
- This repository is not certified for operational or safety-critical deployment.
- Repository evidence and checklists are engineering guidance, not legal advice or certification.
- Maintainer does not provide direct operational support to non-U.S. users.

Legal references:
- `docs/CIVILIAN_USE_POLICY.md`
- `docs/EXPORT_CONTROL_NOTE.md`
- `docs/LEGAL_FAQ.md`
- `docs/LEGAL_SCOPE_CHECKLIST.md`
- `docs/MAINTAINER_BOUNDARY_POLICY.md`

## Docs Map

- First-time docs index: `docs/README.md`
- ESP32 bring-up: `docs/ESP32_BRINGUP.md`
- ESP32 sensorless baseline: `docs/ESP32_SENSORLESS_BASELINE.md`
- Golden-image bootchain workflow: `docs/ESP32_GOLDEN_IMAGE_BOOTCHAIN.md`
- Coverage policy and ramp plan: `docs/COVERAGE_POLICY.md`, `docs/COVERAGE_RAMP_PLAN.md`
- Safety case templates: `docs/safety_case/README.md`

## Author

Albert Cornelius - Apache License 2.0

## Project Governance

- Contributing guide: `CONTRIBUTING.md`
- Security policy: `SECURITY.md`
- Code of conduct: `CODE_OF_CONDUCT.md`
- Release notes: `CHANGELOG.md`
- Disclaimer: `DISCLAIMER.md`
