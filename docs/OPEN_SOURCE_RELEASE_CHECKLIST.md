# Open-Source Release Checklist

Date: 2026-03-06

Use this checklist before publishing a DELTA-V framework release to GitHub.

## 1. Technical Gates

- [ ] `python3 tools/legal_compliance_check.py` passes.
- [ ] `cmake --build build --target flight_readiness` passes.
- [ ] `cmake --build build --target qualification_bundle` passes.
- [ ] `docs/REQUIREMENTS_TRACE_MATRIX.md` and `.json` are refreshed.
- [ ] CI safety-assurance workflow passes on clean branch.

## 2. Documentation

- [ ] `README.md` quick-start commands are current.
- [ ] `README.md` links `docs/CIVILIAN_USE_POLICY.md` and `docs/EXPORT_CONTROL_NOTE.md`.
- [ ] `README.md` and `DISCLAIMER.md` include non-operational-use limits.
- [ ] `docs/SAFETY_ASSURANCE.md` references current gate commands.
- [ ] `docs/ESP32_BRINGUP.md` includes latest hardware validation steps.
- [ ] `docs/process/*.md` templates are present for mission teams.

## 3. Security and Safety Hygiene

- [ ] No production secrets in repository history or source files.
- [ ] Dev/test uplink keys are clearly marked as non-flight keys.
- [ ] Safety-critical changes have review evidence recorded.
- [ ] Civilian-use scope maintained (no weapon/military-targeting functionality).
- [ ] Destination/end-user screening process is defined for deployment operations.

## 4. Release Packaging

- [ ] Tag created (`vX.Y.Z`).
- [ ] License file included.
- [ ] Bootstrap and tooling scripts executable.
- [ ] Release notes include:
  - [ ] toolchain versions
  - [ ] known limitations
  - [ ] legal/compliance constraints (non-defense scope, export/sanctions responsibility)
  - [ ] required manual on-target qualification work
