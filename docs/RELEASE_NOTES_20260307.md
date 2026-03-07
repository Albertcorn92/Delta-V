# DELTA-V Release Notes (2026-03-07)

Release baseline: `1b7036a796e232f8aee81969bcc8a203a33bbc94`

## Toolchain Versions

- CMake: `4.2.3`
- Compiler (host build): AppleClang `17.0.0.17000604`
- Compiler (coverage build): GCC `12.5.0`
- Python (system): `3.12.12`
- Python (.venv): `3.14.3`
- Host OS: `Darwin 25.2.0 arm64`

## Included Assurance Outputs

- `cmake --build build --target software_final` PASS
- `cmake --build build_cov --target coverage_guard` PASS
- `cmake --build build --target cubesat_readiness` PASS (report generated)
- `cmake --build build --target quickstart_10min` PASS

## Known Limitations

- No archived ESP32 soak JSON at `>= 3600s` in this release snapshot.
- Sensor-attached HIL campaign is not executed in this scope-limited closeout.
- Mission environment/comms/ops campaigns remain manual mission-team work.

## Legal/Compliance Constraints

- Civilian/open-source scope only.
- No command-path cryptography baseline in framework release profile.
- Repository policy docs do not provide legal clearance; operators remain responsible for export/sanctions and end-use/end-user screening decisions.

## Required Manual Mission Work

- Execute and archive long-duration hardware soak evidence.
- Execute sensor-attached HIL campaign evidence.
- Complete mission environment test matrix, comms validation, and operations readiness rehearsals.

## Safety-Case Evidence Location

- Framework safety-case baseline: `docs/safety_case/`
- Mission process templates and records: `docs/process/`
- CubeSat readiness status snapshots: `docs/CUBESAT_READINESS_STATUS*.{md,json}`
