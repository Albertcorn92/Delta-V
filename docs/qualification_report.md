# DELTA-V Qualification Report

- Generated (UTC): `2026-03-16T23:06:11.869844+00:00`
- Workspace: `DELTA-V Framework`

## Build Provenance

| Field | Value |
|---|---|
| Git branch | `main` |
| Git commit | `8f44015cf3676b1a9cfe825d3e304d93fc900e74` |
| Exact tag | `untagged` |
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
- Unit test count: `184`
- DAL distribution: `{'B': 19, 'C': 6, 'A': 12}`

## Artifact Checksums (CRC-32)

| Artifact | CRC-32 | Digest-256 |
|---|---|---|
| `build/requirements_trace_matrix.json` | `ee56fb4c` | `b4e36530c5655172f3c0a0e8cc4c93bc87890ebff9e8e389a3b4a56242e6ccd5` |
| `build/requirements_trace_matrix.md` | `1600f887` | `8a2cf9a0f41421d2808cbfe95015202df7699086861bf1042fe91baec69e4da9` |
| `tests/unit_tests.cpp` | `db6caa4e` | `5a9dfa5183df7a20562b7cf3c0566b176fc7658471496e6b32d7fcb9687520e4` |
| `build/flight_software` | `cca1b614` | `57bf277db3660320436e9d5ac1a652e1a9655e88453eedcf56e52191e1fbaef9` |
| `build/run_tests` | `df58075f` | `84cec5073c77e54f955ec8875841b53965338cc1237aa2982e1a423862e5ecff` |

## Manual Evidence Remaining

- None for baseline release profile.

