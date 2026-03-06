# DELTA-V Qualification Report

- Generated (UTC): `2026-03-06T18:53:48.885362+00:00`
- Workspace: `DELTA-V Framework`

## Build Provenance

| Field | Value |
|---|---|
| Git branch | `main` |
| Git commit | `ba58284946771d7ba43d963e03e9f3c2463eada5` |
| Dirty worktree | `True` |
| Host OS | `macOS-26.2-arm64-arm-64bit-Mach-O` |
| Python | `3.14.3` |
| CMake | `cmake version 4.2.3` |

## Qualification Gates

| Gate | Status | Evidence |
|---|---|---|
| Build/Test Gate | PASS | flight_readiness target execution |
| Static Safety Gate | PASS | tidy_safety in flight_readiness |
| Requirements Traceability Gate | PASS | build_final/requirements_trace_matrix.json |

## Requirements Coverage

- Total requirements: `33`
- With direct test evidence: `33`
- Mapping errors: `0`
- Unit test count: `119`
- DAL distribution: `{'B': 16, 'C': 5, 'A': 12}`

## Artifact Checksums (CRC-32)

| Artifact | CRC-32 |
|---|---|
| `build_final/requirements_trace_matrix.json` | `93ea3889` |
| `build_final/requirements_trace_matrix.md` | `0d46e8ba` |
| `tests/unit_tests.cpp` | `af3a9e85` |
| `build_final/flight_software` | `6944d96d` |
| `build_final/run_tests` | `85d697ea` |

## Manual Evidence Remaining

- On-target HIL campaign evidence (ESP32 runtime, fault injection, soak).
- Timing/WCET and stack margin evidence on target.
- Program-level DO-178C process records (review signatures, independence, audits).

