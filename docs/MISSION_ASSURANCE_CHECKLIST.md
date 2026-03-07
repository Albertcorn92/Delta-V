# DELTA-V Mission Assurance Checklist (Civilian Baseline)

Date: 2026-03-07

Use this checklist to move from open-source baseline toward high-assurance mission readiness while keeping civilian scope.

## A. Software Gates (repeatable, in-repo)

- [x] Legal scope guard passes (`python3 tools/legal_compliance_check.py`).
- [x] Unit + system tests pass (`cmake --build build --target run_tests run_system_tests`).
- [x] Stress regression passes (`cmake --build build --target vnv_stress`).
- [x] Blocking static analysis passes (`cmake --build build --target tidy_safety`).
- [x] Requirements traceability passes (`cmake --build build --target traceability`).
- [x] Benchmark guard passes (`cmake --build build --target benchmark_guard`).
- [x] SITL runtime health checks pass (`cmake --build build --target sitl_smoke sitl_soak`).
- [x] Qualification artifacts generate (`cmake --build build --target qualification_bundle`).
- [x] Software final gate passes (`cmake --build build --target software_final`).
- [x] Coverage guard passes in dedicated GCC coverage build (`cmake --build build_cov --target coverage_guard`).

## B. On-Target ESP32 Evidence (sensorless-capable)

- [x] Build/flash completes on ESP32-S3 local-only profile.
- [x] 3-5 minute smoke run passes (no panic/assert/stack-overflow/reboot loop).
- [x] 30-minute soak passes (`python3 tools/esp32_soak.py ... --duration 1800`).
- [x] Runtime metric guard passes (`python3 tools/esp32_runtime_guard.py ...`).
- [x] Reboot campaign passes (`python3 tools/esp32_reboot_campaign.py ...`).
- [ ] Multi-hour soak evidence archived (>= 1 hour recommended; explicitly deferred in scope-limited closeout).

## C. Process Evidence (mission program dependent)

- [x] Independent review records attached for safety-critical changes (`docs/process/INDEPENDENT_REVIEW_RECORD_20260307.md`).
- [x] Mission-level hazard log/FMEA/FTA evidence linked to requirements (`docs/safety_case/hazards.md`, `docs/safety_case/fmea.md`, `docs/safety_case/fta.md`, `docs/safety_case/mitigations.md`, `docs/safety_case/verification_links.md`).
- [x] Configuration/control board records and release sign-off complete (`docs/process/CCB_RELEASE_SIGNOFF_20260307.md`).

## D. Civilian Scope and Legal Hygiene

- [x] Civilian-use policy and export-control note are present.
- [x] Repository avoids command-path cryptography in baseline.
- [x] Repository avoids military/weapon-targeting functionality.
- [x] Maintainer boundary policy is documented.
- [x] Release owner performs destination/end-user checks before operational deployment (`docs/process/DEPLOYMENT_SCREENING_PROCEDURE.md`, `docs/process/DEPLOYMENT_SCREENING_LOG_20260307.md`).

## Suggested Execution Order

1. `cmake --build build --target software_final`
2. `cmake --build build_cov --target coverage_guard`
3. `python3 tools/esp32_runtime_guard.py --project-dir ports/esp32 --build-dir build_esp32 --port /dev/cu.usbmodem101 --duration 300`
4. `python3 tools/esp32_reboot_campaign.py --project-dir ports/esp32 --build-dir build_esp32 --port /dev/cu.usbmodem101 --cycles 10 --cycle-seconds 12`
5. `python3 tools/esp32_soak.py --project-dir ports/esp32 --build-dir build_esp32 --port /dev/cu.usbmodem101 --duration 3600`
6. `cmake --build build --target cubesat_readiness`

Scope-limited readiness snapshot (excludes 1h soak + sensor-attached HIL):

- `python3 tools/cubesat_readiness_report.py --workspace . --build-dir build --exclude-check esp32-soak-1h --exclude-check sensor-attached-evidence --output-md docs/CUBESAT_READINESS_STATUS_SCOPE.md --output-json docs/CUBESAT_READINESS_STATUS_SCOPE.json`
