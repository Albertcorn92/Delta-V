# DELTA-V Autonomy Framework

[![DELTA-V CI](https://github.com/Albertcorn92/Delta-V/actions/workflows/ci.yml/badge.svg)](https://github.com/Albertcorn92/Delta-V/actions/workflows/ci.yml)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Latest Tag](https://img.shields.io/github/v/tag/Albertcorn92/Delta-V)](https://github.com/Albertcorn92/Delta-V/tags)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)](https://en.cppreference.com/w/cpp/20)

DELTA-V is a C++20 flight software framework for civilian aerospace, robotics,
and research systems. The repository contains the host/SITL runtime, ESP32
port, topology generator, ground tools, tests, and release tooling in one tree.

## What Is Here

- `src/`: runtime components, hubs, FDIR, and generated topology wiring
- `topology.yaml`: source of truth for components, commands, and routing
- `tools/`: generator, validation scripts, release tooling, and utilities
- `gds/`: Streamlit-based ground station
- `ports/esp32/`: ESP32 port and board-specific support
- `docs/`: user guides, architecture notes, release records, and process docs

## First Run

Set up Python, configure the build, and run the local validation target:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target quickstart_10min
```

For a scaffolded mission project:

```bash
./bootstrap.sh my_spacecraft
cd my_spacecraft

python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target quickstart_10min
```

Read next:

- [Quickstart](docs/QUICKSTART_10_MIN.md)
- [Documentation guide](docs/README.md)
- [Developer guide](docs/DEVELOPER_GUIDE.md)
- [Architecture reference](docs/ARCHITECTURE.md)

## Common Commands

| Task | Command |
|---|---|
| Verify generated files | `cmake --build build --target autocoder_check` |
| Regenerate topology outputs | `python3 tools/autocoder.py` |
| Build the host runtime | `cmake --build build --target flight_software` |
| Run unit tests | `cmake --build build --target run_tests` |
| Run system tests | `cmake --build build --target run_system_tests` |
| Run the local validation path | `cmake --build build --target quickstart_10min` |
| Run the software closeout gate | `cmake --build build --target software_final` |
| Check release blockers | `cmake --build build --target release_preflight` |
| Generate tagged release artifacts | `cmake --build build --target release_candidate` |
| Build the reviewer bundle | `cmake --build build --target review_bundle` |

## Current Baseline

The current generated status documents report:

- `software_final`: `PASS`
- direct test evidence: `37/37` requirements
- framework release readiness: `True`
- CubeSat flight readiness: `False`

The software baseline is released. Sensor-attached HIL and mission-specific
qualification remain outside the public baseline.

Reference documents:

- [Software final status](docs/SOFTWARE_FINAL_STATUS.md)
- [Qualification report](docs/qualification_report.md)
- [Requirements trace matrix](docs/REQUIREMENTS_TRACE_MATRIX.md)
- [CubeSat readiness status](docs/CUBESAT_READINESS_STATUS.md)
- [Release preflight](docs/process/RELEASE_PREFLIGHT_CURRENT.md)

## Build And Run

Requirements:

- C++20 compiler (`clang++` 14+ or `g++` 11+)
- CMake 3.15+
- Python 3.9+

Build the host runtime:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target flight_software
```

Run the host runtime and GDS:

```bash
# Terminal 1
./build/flight_software

# Terminal 2
streamlit run gds/gds_dash.py
```

## Development Workflow

Typical edit loop:

1. Update `src/` or `topology.yaml`.
2. Run `python3 tools/autocoder.py` after topology changes.
3. Rebuild the affected target.
4. Run `run_tests`, `run_system_tests`, or `quickstart_10min`.

Component scaffolding:

```bash
python3 tools/dv-util.py boot-menu
python3 tools/dv-util.py quickstart-component ThermalControl --build
```

For more detail, see [Developer guide](docs/DEVELOPER_GUIDE.md).

## Runtime Summary

- Ground link: UDP or serial KISS
- Protocol layer: CCSDS framing with CRC and anti-replay checks
- Runtime core: `TelemetryBridge`, `CommandHub`, `TelemHub`, `EventHub`, `RateGroupExecutive`
- Fault management: `WatchdogComponent`, `MissionFsm`, `ParamDb`, `TmrStore`
- Reference payload profile: `PayloadMonitorComponent` (`ID 400`)
- Generated artifacts: `src/TopologyManager.hpp`, `src/Types.hpp`, `dictionary.json`

See [Architecture reference](docs/ARCHITECTURE.md) and [ICD](docs/ICD.md).

## Scope

DELTA-V is published as a civilian public-source framework.

In scope:

- software architecture and runtime behavior
- simulation, tests, and release evidence
- educational, research, and civilian adaptation

Out of scope for this public baseline:

- weapons, targeting, or military mission logic
- command-path crypto/auth additions
- hardware qualification claims
- operational support commitments

Legal references:

- [Civilian use policy](docs/CIVILIAN_USE_POLICY.md)
- [Export control note](docs/EXPORT_CONTROL_NOTE.md)
- [Legal FAQ](docs/LEGAL_FAQ.md)
- [Maintainer boundary policy](docs/MAINTAINER_BOUNDARY_POLICY.md)
- [Public security posture baseline](docs/process/PUBLIC_SECURITY_POSTURE_BASELINE.md)

## More Documentation

- [Documentation guide](docs/README.md)
- [Quickstart](docs/QUICKSTART_10_MIN.md)
- [Developer guide](docs/DEVELOPER_GUIDE.md)
- [Architecture reference](docs/ARCHITECTURE.md)
- [Safety assurance](docs/SAFETY_ASSURANCE.md)
- [Reference mission walkthrough](docs/REFERENCE_MISSION_WALKTHROUGH.md)
- [Assessment guide](docs/ASSESS_DELTA_V.md)
- [ESP32 bring-up](docs/ESP32_BRINGUP.md)

## Project Files

- [Contributing guide](CONTRIBUTING.md)
- [Security policy](SECURITY.md)
- [Code of conduct](CODE_OF_CONDUCT.md)
- [Changelog](CHANGELOG.md)
- [Disclaimer](DISCLAIMER.md)

Albert Cornelius. Licensed under Apache-2.0.
