# Open-Source Release Checklist

Date: 2026-03-07

Use this checklist before publishing a DELTA-V framework release to GitHub.

## 1. Technical Gates

- [x] `python3 tools/legal_compliance_check.py` passes.
- [x] `cmake --build build --target quickstart_10min` passes on a clean checkout.
- [x] `cmake --build build --target run_system_tests` passes.
- [x] `cmake --build build --target vnv_stress` passes (repeat/shuffle stress gate).
- [x] `cmake --build build --target benchmark_baseline` passes and refreshes docs baseline artifacts.
- [x] `cmake --build build --target benchmark_guard` passes against thresholds.
- [x] `cmake --build build --target sitl_smoke` passes.
- [x] `cmake --build build --target sitl_soak` passes.
- [x] `cmake --build build --target portability_matrix` passes (host/SITL profile sweep).
- [x] `cmake --build build_cov --target coverage_guard` passes.
- [x] `cmake --build build --target flight_readiness` passes.
- [x] `cmake --build build --target qualification_bundle` passes.
- [x] `cmake --build build --target software_final` passes.
- [x] `cmake --build build --target cubesat_readiness` runs and updates `docs/CUBESAT_READINESS_STATUS.{md,json}`.
- [x] ESP local-only build boots on hardware (`DELTAV_LOCAL_ONLY=ON`).
- [x] 3-5 minute ESP monitor smoke test passes with no reboot/panic/stack-overflow output.
- [x] `python3 tools/esp32_soak.py --project-dir ports/esp32 --build-dir build_esp32 --port /dev/cu.usbmodem101 --duration 1800` passes.
- [x] `python3 tools/esp32_runtime_guard.py --project-dir ports/esp32 --build-dir build_esp32 --port /dev/cu.usbmodem101 --duration 300` passes.
- [x] `python3 tools/esp32_reboot_campaign.py --project-dir ports/esp32 --build-dir build_esp32 --port /dev/cu.usbmodem101 --cycles 10 --cycle-seconds 12` passes.
- [x] `docs/REQUIREMENTS_TRACE_MATRIX.md` and `.json` are refreshed.
- [x] `docs/qualification_report.md` and `.json` are refreshed.
- [x] `docs/SOFTWARE_FINAL_STATUS.md` is refreshed.
- [x] `docs/BENCHMARK_BASELINE.md` and `.json` are refreshed.
- [ ] CI safety-assurance workflow passes on clean branch.

## 2. Documentation

- [x] `README.md` quick-start commands are current.
- [x] `README.md` includes `quickstart_10min`, system tests, benchmark baseline, and SITL smoke commands.
- [x] `README.md` links `docs/CIVILIAN_USE_POLICY.md` and `docs/EXPORT_CONTROL_NOTE.md`.
- [x] `README.md` links `docs/LEGAL_FAQ.md`.
- [x] `README.md` and `DISCLAIMER.md` include non-operational-use limits.
- [x] `docs/SAFETY_ASSURANCE.md` references current gate commands.
- [x] `docs/CUBESAT_TEAM_READINESS.md` reflects current mission-team workflow and templates.
- [x] `docs/BENCHMARK_PROTOCOL.md` reflects current benchmark scope and artifacts.
- [x] `docs/COVERAGE_POLICY.md` and `docs/COVERAGE_THRESHOLDS.json` reflect current CI thresholds.
- [x] `docs/COVERAGE_RAMP_PLAN.md` is updated with current stage and next-stage criteria.
- [x] `docs/ESP32_BRINGUP.md` includes latest no-sensor and hardware validation steps.
- [x] `docs/ESP32_SENSORLESS_BASELINE.md` reflects the latest local-only hardware run.
- [x] `docs/evidence/ESP32_SENSORLESS_EVIDENCE_*.md` is refreshed for public review.
- [x] `docs/LEGAL_SCOPE_CHECKLIST.md` reflects current legal/civilian release guidance.
- [x] `docs/MAINTAINER_BOUNDARY_POLICY.md` reflects current maintainer support limits.
- [x] `docs/process/*.md` templates are present for mission teams.
- [x] `docs/evidence/ESP32_SENSOR_ATTACHED_EVIDENCE_TEMPLATE.md` is present for sensor-attached runs.
- [x] `docs/safety_case/` templates are present and linked to release evidence.
- [x] `docs/safety_case/fmea.md` and `docs/safety_case/fta.md` are refreshed for current release assumptions.
- [x] `docs/process/INDEPENDENT_REVIEW_RECORD_*.md` and `docs/process/CCB_RELEASE_SIGNOFF_*.md` exist for this release cycle.

## 3. Security and Safety Hygiene

- [x] No production secrets in repository history or source files (`docs/process/SECRETS_SCAN_20260308.md`).
- [x] `artifacts/` logs/reports are not committed (or are explicitly redacted).
- [x] No command-path cryptography/encryption features added to baseline framework.
- [x] Host/SITL uplink safety toggles are documented (`DELTAV_ENABLE_UNAUTH_UPLINK`, `DELTAV_UPLINK_ALLOW_IP`).
- [x] Safety-critical changes have review evidence recorded (`docs/process/INDEPENDENT_REVIEW_RECORD_20260307.md`).
- [x] Civilian-use scope maintained (no weapon/military-targeting functionality).
- [x] Destination/end-user screening process is defined for deployment operations (`docs/process/DEPLOYMENT_SCREENING_PROCEDURE.md`, `docs/process/DEPLOYMENT_SCREENING_LOG_20260307.md`).
- [x] Maintainer boundary policy is enforced for public interactions (`docs/MAINTAINER_BOUNDARY_POLICY.md`).

## 4. Release Packaging

- [ ] Tag created (`vX.Y.Z`).
- [x] License file included.
- [x] Bootstrap and tooling scripts executable.
- [x] Release notes include:
  - [x] toolchain versions
  - [x] known limitations
  - [x] legal/compliance constraints (non-defense scope, export/sanctions responsibility)
  - [x] required manual on-target qualification work
  - [x] safety-case evidence package location (`docs/safety_case/`)
