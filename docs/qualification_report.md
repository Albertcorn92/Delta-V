# DELTA-V Qualification Report

- Generated (UTC): `2026-03-07T00:10:12.980215+00:00`
- Workspace: `DELTA-V Framework`

## Build Provenance

| Field | Value |
|---|---|
| Git branch | `main` |
| Git commit | `6efbcff9ff1ed19fb79959c2a70a42726e4e612d` |
| Dirty worktree | `True` |
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
- Unit test count: `130`
- DAL distribution: `{'B': 16, 'C': 5, 'A': 12}`

## Artifact Checksums (CRC-32)

| Artifact | CRC-32 |
|---|---|
| `build/requirements_trace_matrix.json` | `93ea3889` |
| `build/requirements_trace_matrix.md` | `0d46e8ba` |
| `tests/unit_tests.cpp` | `4ebaedba` |
| `build/flight_software` | `1178a4e9` |
| `build/run_tests` | `37072520` |

## Manual Evidence Remaining

- On-target HIL campaign evidence (ESP32 runtime, fault injection, soak).
- Timing/WCET and stack margin evidence on target.
- Program-level DO-178C process records (review signatures, independence, audits).

