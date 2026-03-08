# DELTA-V Qualification Report

- Generated (UTC): `2026-03-08T00:19:48.345451+00:00`
- Workspace: `DELTA-V Framework`

## Build Provenance

| Field | Value |
|---|---|
| Git branch | `main` |
| Git commit | `688145aa49732155fc701dcec800b0207b708b6b` |
| Dirty worktree | `False` |
| Host OS | `macOS-26.2-arm64-arm-64bit` |
| Python | `3.12.12` |
| CMake | `cmake version 4.2.3` |

## Qualification Gates

| Gate | Status | Evidence |
|---|---|---|
| Build/Test Gate | PASS | flight_readiness target execution |
| V&V Stress Gate | PASS | vnv_stress target in flight_readiness |
| Static Safety Gate | PASS | tidy_safety in flight_readiness |
| Requirements Traceability Gate | PASS | build/requirements_trace_matrix.json |

## Requirements Coverage

- Total requirements: `33`
- With direct test evidence: `33`
- Mapping errors: `0`
- Unit test count: `131`
- DAL distribution: `{'B': 16, 'C': 5, 'A': 12}`

## Artifact Checksums (CRC-32)

| Artifact | CRC-32 |
|---|---|
| `build/requirements_trace_matrix.json` | `c23274da` |
| `build/requirements_trace_matrix.md` | `8fa4ddd0` |
| `tests/unit_tests.cpp` | `88c7ca73` |
| `build/flight_software` | `1bd256e0` |
| `build/run_tests` | `cc5affe6` |

## Manual Evidence Remaining

- Multi-hour sensorless soak evidence on target (`tools/esp32_soak.py`, >=1h).

