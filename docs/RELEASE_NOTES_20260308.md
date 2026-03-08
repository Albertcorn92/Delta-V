# DELTA-V Release Notes (2026-03-08)

Release baseline: `main` (host-only finalization pass on 2026-03-08)

## Toolchain Snapshot

- CMake: `4.2.3`
- Compiler (host build): AppleClang `17.0.0.17000604`
- Compiler (coverage build): GCC `12.5.0`
- Python (system): `3.12.12`
- Host OS: `Darwin 25.2.0 arm64`

## Assurance Gate Results

- `cmake --build build --target run_tests` PASS
- `cmake --build build --target run_system_tests` PASS
- `cmake --build build --target vnv_stress` PASS
- `cmake --build build --target tidy_safety` PASS
- `cmake --build build --target traceability` PASS (`37/37` requirements with direct tests)
- `cmake --build build --target qualification_bundle` PASS
- `cmake --build build --target software_final` PASS
- `cmake --build build --target cubesat_readiness` PASS (report generated; flight readiness remains `False` until hardware gaps close)
- `cmake --build build --target cubesat_readiness_scope` PASS (scope-limited report generated)

## Scoped Readiness Waivers (Explicit)

The scope-limited report intentionally excludes these checks using
`--exclude-check`, and records each as `WAIVED` in the output:

- `esp32-soak-1h`
- `sensor-attached-evidence`

This is a traceable scope decision, not a claim that hardware evidence exists.

## Civilian/Open-Source Boundary

- Civilian/open-source scope only.
- No command-path encryption baseline in framework release profile.
- Policy and readiness reports are engineering artifacts, not legal advice.

## Remaining Mission-Hardware Work

- Execute and archive ESP32 soak evidence at `>= 3600s`.
- Execute and archive sensor-attached HIL evidence.
- Execute mission RF-chain and field-environment campaigns on mission hardware.
