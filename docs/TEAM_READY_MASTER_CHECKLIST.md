# DELTA-V Team-Ready Master Checklist

Updated: 2026-03-08

Use this as the single closeout checklist for mission-team handoff. This is an
engineering readiness checklist, not legal advice or certification approval.

## 1) Civilian Scope and Compliance

- [ ] Keep baseline civilian-only boundaries in place (`docs/CIVILIAN_USE_POLICY.md`).
- [ ] Keep export/scope notes current (`docs/EXPORT_CONTROL_NOTE.md`, `docs/LEGAL_FAQ.md`).
- [ ] Confirm no command-path encryption additions in baseline.
- [ ] Run legal gate:
  - `cmake --build build --target legal_compliance`

## 2) Software Assurance (Host)

- [ ] `cmake --build build --target run_tests`
- [ ] `cmake --build build --target run_system_tests`
- [ ] `cmake --build build --target vnv_stress`
- [ ] `cmake --build build --target tidy_safety`
- [ ] `cmake --build build --target traceability`
- [ ] `cmake --build build --target qualification_bundle`
- [ ] `cmake --build build --target software_final`
- [ ] Verify artifacts:
  - `docs/SOFTWARE_FINAL_STATUS.md`
  - `docs/REQUIREMENTS_TRACE_MATRIX.json`
  - `docs/qualification_report.json`

## 3) ESP32 On-Target Evidence

Plug in the ESP32 board, then run:

```bash
python3 tools/team_ready_esp32.py --soak-duration 3600
```

If auto-detect does not find the board:

```bash
python3 tools/team_ready_esp32.py --port /dev/cu.usbmodem101 --soak-duration 3600
```

The script runs:

- `software_final` (unless `--skip-host-gates`)
- `tools/esp32_runtime_guard.py`
- `tools/esp32_reboot_campaign.py`
- `tools/esp32_soak.py` (default 1 hour)
- `cubesat_readiness` (+ scope report unless skipped)

Outputs:

- `artifacts/team_ready_esp32_*.json`
- `artifacts/esp32_runtime_guard_*.json`
- `artifacts/esp32_reboot_campaign_*.json`
- `artifacts/esp32_soak_*.json`
- `docs/CUBESAT_READINESS_STATUS.md`

## 4) Sensor-Attached Evidence (Manual but Required for Flight)

- [ ] Execute sensor-attached run on hardware.
- [ ] Record evidence using:
  - `docs/evidence/ESP32_SENSOR_ATTACHED_EVIDENCE_TEMPLATE.md`
- [ ] Save as:
  - `docs/evidence/ESP32_SENSOR_ATTACHED_EVIDENCE_<date>.md`

## 5) Mission Program Records

- [ ] Flight environment matrix: `docs/process/FLIGHT_ENV_TEST_MATRIX_<date>.md`
- [ ] Comms link validation: `docs/process/COMMS_LINK_VALIDATION_<date>.md`
- [ ] Operations readiness: `docs/process/OPERATIONS_READINESS_<date>.md`
- [ ] Release manifest: `docs/process/RELEASE_MANIFEST_<date>.md`

## 6) Operations Capability Evidence

- [ ] ATS/RTS sequence execution evidence.
- [ ] Dynamic limit-table update and alarm evidence.
- [ ] Store-and-forward playback evidence.
- [ ] Memory dwell/patch procedure evidence.
- [ ] UTC sync command evidence.
- [ ] OTA staging + rollback/golden-image drill evidence.
- [ ] Mode transition evidence (detumble/sun/science/downlink as applicable).

## 7) Team Handoff Quality Bar

- [ ] New operator can complete quickstart in <= 10 minutes.
- [ ] New operator can scaffold component + command using `dv-util` boot menu.
- [ ] GDS shows live telemetry/event flow and command ack path.
- [ ] Runbook walk-through completed (`docs/process/OPERATIONS_RUNBOOK.md`).

## 8) Final Acceptance Criteria

- [ ] `docs/CUBESAT_READINESS_STATUS.md` shows:
  - `framework_release_ready: true`
  - `cubesat_flight_ready: true` (unwaived)
- [ ] Remaining gaps section is empty for unwaived readiness.
- [ ] Release commit/tag recorded and evidence artifacts archived.

## 9) Scope Waivers (Only for Internal Progress Tracking)

Scope waivers can be used for interim progress reports, but they are not a
substitute for missing flight evidence.

- Waived report target:
  - `cmake --build build --target cubesat_readiness_scope`
- Unwaived flight status remains authoritative for mission readiness.
