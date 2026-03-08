# DELTA-V Qualification Report

- Generated (UTC): `2026-03-08T06:02:42.993462+00:00`
- Workspace: `DELTA-V Framework`

## Build Provenance

| Field | Value |
|---|---|
| Git branch | `main` |
| Git commit | `c403af0290dcb62e267da3e8d5a5c7c4bce9c680` |
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
- Unit test count: `158`
- DAL distribution: `{'B': 19, 'C': 6, 'A': 12}`

## Artifact Checksums (CRC-32)

| Artifact | CRC-32 |
|---|---|
| `build/requirements_trace_matrix.json` | `ee56fb4c` |
| `build/requirements_trace_matrix.md` | `1600f887` |
| `tests/unit_tests.cpp` | `b056dcd6` |
| `build/flight_software` | `a4964491` |
| `build/run_tests` | `495d5e17` |

## Manual Evidence Remaining

- Multi-hour sensorless soak evidence on target (`tools/esp32_soak.py`, >=1h).

