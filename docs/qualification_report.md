# DELTA-V Qualification Report

- Generated (UTC): `2026-03-07T01:54:07.480824+00:00`
- Workspace: `DELTA-V Framework`

## Build Provenance

| Field | Value |
|---|---|
| Git branch | `main` |
| Git commit | `bed6ae40cd19491b747e65df280ebd8c3ecc47b8` |
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
| `build/flight_software` | `74fb6113` |
| `build/run_tests` | `64b6b549` |

## Manual Evidence Remaining

- On-target HIL campaign evidence (ESP32 runtime, fault injection, soak).
- Timing/WCET and stack margin evidence on target (`tools/esp32_runtime_guard.py`).
- Reboot-cycle stability evidence on target (`tools/esp32_reboot_campaign.py`).
- Program-level DO-178C process records (review signatures, independence, audits).

