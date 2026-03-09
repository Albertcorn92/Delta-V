# DELTA-V Qualification Report

- Generated (UTC): `2026-03-09T04:58:18.538028+00:00`
- Workspace: `DELTA-V Framework`

## Build Provenance

| Field | Value |
|---|---|
| Git branch | `main` |
| Git commit | `35822864d164c100fd343fc2f81f2261a810170b` |
| Dirty worktree | `True` |
| Host OS | `macOS-26.2-arm64-arm-64bit-Mach-O` |
| Python | `3.14.3` |
| CMake | `cmake version 4.2.3` |

## Qualification Gates

| Gate | Status | Evidence |
|---|---|---|
| Build/Test Gate | PASS | flight_readiness target execution |
| V&V Stress Gate | PASS | vnv_stress target in flight_readiness |
| Static Safety Gate | PASS | tidy_safety in flight_readiness |
| Requirements Traceability Gate | PASS | build_tidy/requirements_trace_matrix.json |

## Requirements Coverage

- Total requirements: `37`
- With direct test evidence: `37`
- Mapping errors: `0`
- Unit test count: `163`
- DAL distribution: `{'B': 19, 'C': 6, 'A': 12}`

## Artifact Checksums (CRC-32)

| Artifact | CRC-32 |
|---|---|
| `build_tidy/requirements_trace_matrix.json` | `ee56fb4c` |
| `build_tidy/requirements_trace_matrix.md` | `1600f887` |
| `tests/unit_tests.cpp` | `14866e9c` |
| `build_tidy/flight_software` | `381813ca` |
| `build_tidy/run_tests` | `2000c91b` |

## Manual Evidence Remaining

- None for baseline release profile.

