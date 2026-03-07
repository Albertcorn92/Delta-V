# Open-Source Release Checklist

Date: 2026-03-07

Use this checklist before publishing a DELTA-V framework release to GitHub.

## 1. Technical Gates

- [ ] `python3 tools/legal_compliance_check.py` passes.
- [ ] `cmake --build build --target quickstart_10min` passes on a clean checkout.
- [ ] `cmake --build build --target run_system_tests` passes.
- [ ] `cmake --build build --target vnv_stress` passes (repeat/shuffle stress gate).
- [ ] `cmake --build build --target benchmark_baseline` passes and refreshes docs baseline artifacts.
- [ ] `cmake --build build --target benchmark_guard` passes against thresholds.
- [ ] `cmake --build build --target sitl_smoke` passes.
- [ ] `cmake --build build --target sitl_soak` passes.
- [ ] `cmake --build build_cov --target coverage_guard` passes.
- [ ] `cmake --build build --target flight_readiness` passes.
- [ ] `cmake --build build --target qualification_bundle` passes.
- [ ] `cmake --build build --target software_final` passes.
- [ ] ESP local-only build boots on hardware (`DELTAV_LOCAL_ONLY=ON`).
- [ ] 3-5 minute ESP monitor smoke test passes with no reboot/panic/stack-overflow output.
- [ ] `python3 tools/esp32_soak.py --project-dir ports/esp32 --build-dir build_esp32 --port /dev/cu.usbmodem101 --duration 1800` passes.
- [ ] `python3 tools/esp32_runtime_guard.py --project-dir ports/esp32 --build-dir build_esp32 --port /dev/cu.usbmodem101 --duration 300` passes.
- [ ] `python3 tools/esp32_reboot_campaign.py --project-dir ports/esp32 --build-dir build_esp32 --port /dev/cu.usbmodem101 --cycles 10 --cycle-seconds 12` passes.
- [ ] `docs/REQUIREMENTS_TRACE_MATRIX.md` and `.json` are refreshed.
- [ ] `docs/qualification_report.md` and `.json` are refreshed.
- [ ] `docs/SOFTWARE_FINAL_STATUS.md` is refreshed.
- [ ] `docs/BENCHMARK_BASELINE.md` and `.json` are refreshed.
- [ ] CI safety-assurance workflow passes on clean branch.

## 2. Documentation

- [ ] `README.md` quick-start commands are current.
- [ ] `README.md` includes `quickstart_10min`, system tests, benchmark baseline, and SITL smoke commands.
- [ ] `README.md` links `docs/CIVILIAN_USE_POLICY.md` and `docs/EXPORT_CONTROL_NOTE.md`.
- [ ] `README.md` links `docs/LEGAL_FAQ.md`.
- [ ] `README.md` and `DISCLAIMER.md` include non-operational-use limits.
- [ ] `docs/SAFETY_ASSURANCE.md` references current gate commands.
- [ ] `docs/BENCHMARK_PROTOCOL.md` reflects current benchmark scope and artifacts.
- [ ] `docs/COVERAGE_POLICY.md` and `docs/COVERAGE_THRESHOLDS.json` reflect current CI thresholds.
- [ ] `docs/COVERAGE_RAMP_PLAN.md` is updated with current stage and next-stage criteria.
- [ ] `docs/ESP32_BRINGUP.md` includes latest no-sensor and hardware validation steps.
- [ ] `docs/ESP32_SENSORLESS_BASELINE.md` reflects the latest local-only hardware run.
- [ ] `docs/evidence/ESP32_SENSORLESS_EVIDENCE_*.md` is refreshed for public review.
- [ ] `docs/LEGAL_SCOPE_CHECKLIST.md` reflects current legal/civilian release guidance.
- [ ] `docs/MAINTAINER_BOUNDARY_POLICY.md` reflects current maintainer support limits.
- [ ] `docs/process/*.md` templates are present for mission teams.
- [ ] `docs/safety_case/` templates are present and linked to release evidence.

## 3. Security and Safety Hygiene

- [ ] No production secrets in repository history or source files.
- [ ] `artifacts/` logs/reports are not committed (or are explicitly redacted).
- [ ] No command-path cryptography/encryption features added to baseline framework.
- [ ] Dev/test uplink keys are clearly marked as non-flight keys.
- [ ] Safety-critical changes have review evidence recorded.
- [ ] Civilian-use scope maintained (no weapon/military-targeting functionality).
- [ ] Destination/end-user screening process is defined for deployment operations.
- [ ] Maintainer boundary policy is enforced for public interactions.

## 4. Release Packaging

- [ ] Tag created (`vX.Y.Z`).
- [ ] License file included.
- [ ] Bootstrap and tooling scripts executable.
- [ ] Release notes include:
  - [ ] toolchain versions
  - [ ] known limitations
  - [ ] legal/compliance constraints (non-defense scope, export/sanctions responsibility)
  - [ ] required manual on-target qualification work
  - [ ] safety-case evidence package location (`docs/safety_case/`)
