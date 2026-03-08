# DELTA-V CubeSat Readiness Status

- Generated (UTC): `2026-03-08T00:03:00.159204+00:00`
- Framework release readiness: `True`
- CubeSat flight readiness: `False`

## Automated and Program Checks

| ID | Category | Status | Required For | Evidence |
|---|---|---|---|---|
| legal-compliance | Civilian/Legal | PASS | framework, flight | [legal] Required files and policy scans are valid. |
| software-final | Software | PASS | framework | docs/SOFTWARE_FINAL_STATUS.md |
| qualification-report | Software | PASS | framework | docs/qualification_report.json |
| requirements-traceability | Software | PASS | framework | docs/REQUIREMENTS_TRACE_MATRIX.json (33/33) |
| esp32-runtime-guard | Hardware | PASS | framework, flight | artifacts/esp32_runtime_guard_20260307T071843Z.json |
| esp32-reboot-campaign | Hardware | PASS | framework, flight | artifacts/esp32_reboot_campaign_20260307T072234Z.json |
| esp32-soak-30m | Hardware | PASS | framework | docs/evidence/ESP32_SENSORLESS_EVIDENCE_20260307.md (documented 1800s pass) |
| esp32-soak-1h | Hardware | FAIL | flight | no passing esp32_soak_*.json with duration >= 3600s |
| sensor-attached-evidence | Hardware | MANUAL | flight | docs/evidence/ESP32_SENSOR_ATTACHED_EVIDENCE_20260307.md (record exists, execution pending) |
| flight-env-test-matrix | Program | PASS | flight | docs/process/FLIGHT_ENV_TEST_MATRIX_20260307.md |
| comms-link-validation | Program | PASS | flight | docs/process/COMMS_LINK_VALIDATION_20260307.md |
| operations-readiness | Program | PASS | flight | docs/process/OPERATIONS_READINESS_20260307.md |
| release-manifest | Program | PASS | flight | docs/process/RELEASE_MANIFEST_20260307.md |
| independent-review-record | Process | PASS | framework, flight | docs/process/INDEPENDENT_REVIEW_RECORD_20260307.md |
| ccb-signoff | Process | PASS | framework, flight | docs/process/CCB_RELEASE_SIGNOFF_20260307.md |
| deployment-screening | Civilian/Legal | PASS | framework, flight | docs/process/DEPLOYMENT_SCREENING_PROCEDURE.md, docs/process/DEPLOYMENT_SCREENING_LOG_20260307.md |
| safety-case-baseline | Process | PASS | framework, flight | all baseline safety-case files present |

## Remaining Gaps

- esp32-soak-1h: no passing esp32_soak_*.json with duration >= 3600s
- sensor-attached-evidence: docs/evidence/ESP32_SENSOR_ATTACHED_EVIDENCE_20260307.md (record exists, execution pending)

## Notes

- Framework automation can reduce software risk, but mission qualification still requires hardware and operations evidence.
- This report is an engineering status snapshot, not legal advice or certification approval.
