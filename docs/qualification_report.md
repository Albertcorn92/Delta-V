# DELTA-V Qualification Report

- Generated (UTC): `2026-03-06T17:28:49.921670+00:00`
- Workspace: `/Users/albertcornelius/DELTA-V Framework`

## Build Provenance

| Field | Value |
|---|---|
| Git branch | `main` |
| Git commit | `6787a08b5de3bddb1571fe34603a9f78e9e13092` |
| Dirty worktree | `True` |
| Host OS | `macOS-26.2-arm64-arm-64bit` |
| Python | `3.12.12` |
| CMake | `cmake version 4.2.3` |

## Qualification Gates

| Gate | Status | Evidence |
|---|---|---|
| Build/Test Gate | PASS | flight_readiness target execution |
| Static Safety Gate | PASS | tidy_safety in flight_readiness |
| Requirements Traceability Gate | PASS | /Users/albertcornelius/DELTA-V Framework/build/requirements_trace_matrix.json |

## Requirements Coverage

- Total requirements: `33`
- With direct test evidence: `33`
- Mapping errors: `0`
- Unit test count: `121`
- DAL distribution: `{'B': 16, 'C': 5, 'A': 12}`

## Artifact Hashes (SHA-256)

| Artifact | SHA-256 |
|---|---|
| `/Users/albertcornelius/DELTA-V Framework/build/requirements_trace_matrix.json` | `0a8783134a1e71f69fbf847acf9173888f763b982c68f2bbf55e9eefa6bf8698` |
| `/Users/albertcornelius/DELTA-V Framework/build/requirements_trace_matrix.md` | `abb2db41b71298af3c7ab8fbf294845e3f5623382723b9e1137c09cf6dbd4755` |
| `/Users/albertcornelius/DELTA-V Framework/tests/unit_tests.cpp` | `a14f64659f5cd90035167fc4bb836a02c957e578d307366f9ca264f8809d8b61` |
| `/Users/albertcornelius/DELTA-V Framework/build/flight_software` | `56a777b9910ede0a7a07a64d375851232d26460700a3cd80bfaca336ddee0ae7` |
| `/Users/albertcornelius/DELTA-V Framework/build/run_tests` | `39e7ab414c8bf3ab954efd3ab0b10d405cb27e3df066052c7cd08c429b63a527` |

## Manual Evidence Remaining

- On-target HIL campaign evidence (ESP32 runtime, fault injection, soak).
- Timing/WCET and stack margin evidence on target.
- Program-level DO-178C process records (review signatures, independence, audits).

