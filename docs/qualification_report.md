# DELTA-V Qualification Report

- Generated (UTC): `2026-03-09T21:00:29.904740+00:00`
- Workspace: `DELTA-V Framework`

## Build Provenance

| Field | Value |
|---|---|
| Git branch | `main` |
| Git commit | `a556f0d51936280948ce7518e379ca2b16c1e975` |
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

- Total requirements: `37`
- With direct test evidence: `37`
- Mapping errors: `0`
- Unit test count: `163`
- DAL distribution: `{'B': 19, 'C': 6, 'A': 12}`

## Artifact Checksums (CRC-32)

| Artifact | CRC-32 |
|---|---|
| `build/requirements_trace_matrix.json` | `ee56fb4c` |
| `build/requirements_trace_matrix.md` | `1600f887` |
| `tests/unit_tests.cpp` | `861aa16f` |
| `build/flight_software` | `49bf6750` |
| `build/run_tests` | `3c7742b7` |

## Manual Evidence Remaining

- None for baseline release profile.

