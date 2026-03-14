# DELTA-V Qualification Report

- Generated (UTC): `2026-03-14T23:00:53.394581+00:00`
- Workspace: `DELTA-V Framework`

## Build Provenance

| Field | Value |
|---|---|
| Git branch | `main` |
| Git commit | `1750d3d012fa616913692eb89713beb78939b52f` |
| Exact tag | `untagged` |
| Dirty worktree | `False` |
| Host OS | `macOS-26.2-arm64-arm-64bit-Mach-O` |
| Python | `3.14.3` |
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
- Unit test count: `174`
- DAL distribution: `{'B': 19, 'C': 6, 'A': 12}`

## Artifact Checksums (CRC-32)

| Artifact | CRC-32 | Digest-256 |
|---|---|---|
| `build/requirements_trace_matrix.json` | `ee56fb4c` | `b4e36530c5655172f3c0a0e8cc4c93bc87890ebff9e8e389a3b4a56242e6ccd5` |
| `build/requirements_trace_matrix.md` | `1600f887` | `8a2cf9a0f41421d2808cbfe95015202df7699086861bf1042fe91baec69e4da9` |
| `tests/unit_tests.cpp` | `f00eace4` | `e5c488d54d63c796b96ab86911eaa040b646bd1a42080d937628740b798dce14` |
| `build/flight_software` | `b532917b` | `ee2cf707a70e072e743327654edfd69cadfac67c04a8fc5b1561f1f173001491` |
| `build/run_tests` | `adad57ac` | `86edbe145c6bc7843ac75deecdab6a7db3909f13571c7d30c311f4ebbc54470a` |

## Manual Evidence Remaining

- None for baseline release profile.

