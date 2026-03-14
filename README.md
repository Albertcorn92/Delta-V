# DELTA-V Autonomy Framework v4.0

[![DELTA-V CI](https://github.com/Albertcorn92/Delta-V/actions/workflows/ci.yml/badge.svg)](https://github.com/Albertcorn92/Delta-V/actions/workflows/ci.yml)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Latest Tag](https://img.shields.io/github/v/tag/Albertcorn92/Delta-V)](https://github.com/Albertcorn92/Delta-V/tags)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)](https://en.cppreference.com/w/cpp/20)

DELTA-V is a C++20 flight software framework for civilian aerospace, robotics, and research missions. The host runtime, ESP32 port, generated topology flow, and assurance tooling are all kept in the same repository.

`software_final: PASS` `requirements: 37/37` `civilian-scope: enforced` `flight-ready: pending physical evidence`

## Quick Start

For a repository clone, install the Python tools first. The local validation targets use `tools/autocoder.py`, legal checks, and reporting scripts.

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target quickstart_10min
```

For a new mission project scaffold:

```bash
./bootstrap.sh my_spacecraft
cd my_spacecraft

python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target quickstart_10min
```

Reference:
- [10-minute quickstart](docs/QUICKSTART_10_MIN.md)
- [Documentation map](docs/README.md)
- [How to assess DELTA-V](docs/ASSESS_DELTA_V.md)

## Common Commands

| Task | Command |
|---|---|
| Verify generated files are current | `cmake --build build --target autocoder_check` |
| Run the local validation flow | `cmake --build build --target quickstart_10min` |
| Open the component scaffold workflow | `python3 tools/dv-util.py boot-menu` |
| Scaffold a component and build | `python3 tools/dv-util.py quickstart-component ThermalControl --build` |
| Regenerate generated files | `python3 tools/autocoder.py` |
| Run the software closeout gate | `cmake --build build --target software_final` |
| Check release blockers without tagging | `cmake --build build --target release_preflight` |
| Validate a public tagged release | `cmake --build build --target release_candidate` |
| Assemble a reviewer bundle | `cmake --build build --target review_bundle` |
| Run the live SITL fault campaign | `cmake --build build --target sitl_fault_campaign` |
| Run archived long SITL soaks | `cmake --build build --target sitl_soak_1h` / `sitl_soak_6h` / `sitl_soak_12h` / `sitl_soak_24h` |
| Refresh long-soak status | `cmake --build build --target sitl_long_soak_status` |
| Regenerate readiness reports | `cmake --build build --target cubesat_readiness` |
| Run the host portability sweep | `cmake --build build --target portability_matrix` |

## Current Status

Current generated status documents report:

- `software_final`: `PASS`
- `requirements_traced_with_direct_tests`: `37/37`
- `framework_release_readiness`: `True`
- `cubesat_flight_readiness`: `False`
- `esp32-soak-1h`: `PASS`

The remaining unwaived flight-readiness item is sensor-attached HIL evidence.

Primary references:
- [Software final status](docs/SOFTWARE_FINAL_STATUS.md)
- [CubeSat readiness status](docs/CUBESAT_READINESS_STATUS.md)
- [Scope-limited readiness snapshot](docs/CUBESAT_READINESS_STATUS_SCOPE.md)
- [Requirements trace matrix](docs/REQUIREMENTS_TRACE_MATRIX.json)
- [Qualification report](docs/qualification_report.md)
- [Release preflight](docs/process/RELEASE_PREFLIGHT_CURRENT.md)
- [Assessment guide](docs/ASSESS_DELTA_V.md)
- [Reference mission walkthrough](docs/REFERENCE_MISSION_WALKTHROUGH.md)
- [Process baseline docs](docs/process/)

## Runtime Summary

- Ground segment: Streamlit GDS in `gds/gds_dash.py`
- Link layer: CCSDS framing over UDP or serial KISS
- Runtime core: `TelemetryBridge`, `CommandHub`, `TelemHub`, `EventHub`, `RateGroupExecutive`
- Fault management: `WatchdogComponent`, `MissionFsm`, `ParamDb`, `TmrStore`
- Reference payload profile: `PayloadMonitorComponent` (`ID 400`)
- Generated source of truth: `topology.yaml` -> `tools/autocoder.py` -> `src/TopologyManager.hpp`, `src/Types.hpp`, `dictionary.json`

Architecture reference:
- [Architecture](docs/ARCHITECTURE.md)

## Included Civilian Ops Components

The baseline repository includes:

- `CommandSequencerComponent`
- `FileTransferComponent`
- `MemoryDwellComponent`
- `TimeSyncComponent`
- `PlaybackComponent`
- `OtaComponent`
- `AtsRtsSequencerComponent`
- `LimitCheckerComponent`
- `CfdpComponent`
- `ModeManagerComponent`

Install or reapply the baseline bundle with:

```bash
python3 tools/dv-util.py install-standard-apps
```

## Build and Run

Requirements:

- C++20 compiler (`clang++` 14+ or `g++` 11+)
- CMake 3.15+
- Python 3.9+

Build the host/SITL runtime:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Run the software:

```bash
# Terminal 1
./build/flight_software

# Terminal 2
streamlit run gds/gds_dash.py
```

## Extending the Framework

Interactive path:

```bash
python3 tools/dv-util.py boot-menu
```

Direct scaffold path:

```bash
python3 tools/dv-util.py quickstart-component ThermalControl --build
```

Manual path:

```bash
python3 tools/dv-util.py add-component ThermalControl --profile controller --register
python3 tools/dv-util.py add-command SET_HEATER --target-id 400 --opcode 1 \
  --description "Set heater target temperature (deg C)"
python3 tools/autocoder.py
```

Developer references:
- [Developer guide](docs/DEVELOPER_GUIDE.md)
- [Migration guide template](docs/MIGRATION_GUIDE.md)
- [Software portability matrix](docs/SOFTWARE_PORTABILITY_MATRIX.md)

## Security Notes

The uplink path validates CCSDS headers and enforces anti-replay sequence checks and mission-state gating.

Command-path encryption features are intentionally excluded from this baseline.

Replay-sequence persistence:

```bash
export DELTAV_REPLAY_SEQ_FILE="replay_seq.db"
```

Local-only SITL command ingest:

```bash
export DELTAV_ENABLE_UNAUTH_UPLINK=1
export DELTAV_UPLINK_ALLOW_IP="127.0.0.1"
```

## Legal Scope

- DELTA-V is for civilian, scientific, and educational use.
- Military, weapons, targeting, or fire-control behavior is out of scope.
- Command-path cryptography and encryption features are intentionally excluded in this baseline.
- This repository is not certified for operational or safety-critical deployment.
- Repository evidence and checklists are engineering documentation, not legal advice or certification.
- Maintainer does not provide direct operational support to non-U.S. users.
- Maintainer support boundaries are documented separately.

Legal references:
- [Civilian use policy](docs/CIVILIAN_USE_POLICY.md)
- [Export control note](docs/EXPORT_CONTROL_NOTE.md)
- [Legal FAQ](docs/LEGAL_FAQ.md)
- [Legal scope checklist](docs/LEGAL_SCOPE_CHECKLIST.md)
- [Maintainer boundary policy](docs/MAINTAINER_BOUNDARY_POLICY.md)
- [Public security posture baseline](docs/process/PUBLIC_SECURITY_POSTURE_BASELINE.md)

## Additional Documentation

- [ESP32 bring-up](docs/ESP32_BRINGUP.md)
- [ESP32 sensorless baseline](docs/ESP32_SENSORLESS_BASELINE.md)
- [Golden-image bootchain](docs/ESP32_GOLDEN_IMAGE_BOOTCHAIN.md)
- [Coverage policy](docs/COVERAGE_POLICY.md)
- [Safety assurance](docs/SAFETY_ASSURANCE.md)
- [Software safety plan baseline](docs/process/SOFTWARE_SAFETY_PLAN_BASELINE.md)
- [Reference payload profile](docs/process/REFERENCE_PAYLOAD_PROFILE.md)
- [Static-analysis deviation log](docs/process/STATIC_ANALYSIS_DEVIATION_LOG.md)
- [NASA-style applicability baseline](docs/process/NASA_REQUIREMENTS_APPLICABILITY_BASELINE.md)
- [Safety case templates](docs/safety_case/README.md)

## Project Files

- [Contributing guide](CONTRIBUTING.md)
- [Security policy](SECURITY.md)
- [Code of conduct](CODE_OF_CONDUCT.md)
- [Release notes](CHANGELOG.md)
- [Disclaimer](DISCLAIMER.md)

Albert Cornelius. Licensed under Apache-2.0.
