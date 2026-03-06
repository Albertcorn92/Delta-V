# DELTA-V Qualification Report

- Generated (UTC): `2026-03-06T21:48:20.569233+00:00`
- Workspace: `DELTA-V Framework`

## Build Provenance

| Field | Value |
|---|---|
| Git branch | `main` |
| Git commit | `deac8586b76215c1c873f5e9d492408e4639849c` |
| Dirty worktree | `True` |
| Host OS | `macOS-26.2-arm64-arm-64bit` |
| Python | `3.12.12` |
| CMake | `cmake version 4.2.3` |

## Qualification Gates

| Gate | Status | Evidence |
|---|---|---|
| Build/Test Gate | PASS | flight_readiness target execution |
| Static Safety Gate | PASS | tidy_safety in flight_readiness |
| Requirements Traceability Gate | PASS | build/requirements_trace_matrix.json |

## Requirements Coverage

- Total requirements: `33`
- With direct test evidence: `33`
- Mapping errors: `0`
- Unit test count: `119`
- DAL distribution: `{'B': 16, 'C': 5, 'A': 12}`

## Artifact Checksums (CRC-32)

| Artifact | CRC-32 |
|---|---|
| `build/requirements_trace_matrix.json` | `93ea3889` |
| `build/requirements_trace_matrix.md` | `0d46e8ba` |
| `tests/unit_tests.cpp` | `af3a9e85` |
| `build/flight_software` | `63e7be9f` |
| `build/run_tests` | `8f2a1680` |

## Manual Evidence Remaining

- On-target HIL campaign evidence (ESP32 runtime, fault injection, soak).
- Timing/WCET and stack margin evidence on target.
- Program-level DO-178C process records (review signatures, independence, audits).

