# DELTA-V Qualification Report

- Generated (UTC): `2026-03-07T08:04:45.629491+00:00`
- Workspace: `DELTA-V Framework`

## Build Provenance

| Field | Value |
|---|---|
| Git branch | `main` |
| Git commit | `78941353402a04f6a11ae0cce4b30fefe7881598` |
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
| Requirements Traceability Gate | PASS | build_cov/requirements_trace_matrix.json |

## Requirements Coverage

- Total requirements: `33`
- With direct test evidence: `33`
- Mapping errors: `0`
- Unit test count: `130`
- DAL distribution: `{'B': 16, 'C': 5, 'A': 12}`

## Artifact Checksums (CRC-32)

| Artifact | CRC-32 |
|---|---|
| `build_cov/requirements_trace_matrix.json` | `93ea3889` |
| `build_cov/requirements_trace_matrix.md` | `0d46e8ba` |
| `tests/unit_tests.cpp` | `4ebaedba` |
| `build_cov/flight_software` | `a51f9ead` |
| `build_cov/run_tests` | `5cd8cec1` |

## Manual Evidence Remaining

- Multi-hour sensorless soak evidence on target (`tools/esp32_soak.py`, >=1h).

