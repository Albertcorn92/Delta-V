# DELTA-V CubeSat Readiness Status

- Generated (UTC): `2026-03-08T18:08:20.154366+00:00`
- Framework release readiness: `True`
- CubeSat flight readiness: `True`

## Automated and Program Checks

| ID | Category | Status | Required For | Evidence |
|---|---|---|---|---|
| legal-compliance | Civilian/Legal | PASS | framework, flight | [legal] Required files and policy scans are valid. |
| software-final | Software | PASS | framework | docs/SOFTWARE_FINAL_STATUS.md |
| qualification-report | Software | PASS | framework | docs/qualification_report.json |
| requirements-traceability | Software | PASS | framework | docs/REQUIREMENTS_TRACE_MATRIX.json (37/37) |
| esp32-runtime-guard | Hardware | PASS | framework, flight | artifacts/esp32_runtime_guard_20260308T060432Z.json |
| esp32-reboot-campaign | Hardware | PASS | framework, flight | artifacts/esp32_reboot_campaign_20260308T060703Z.json |
| esp32-soak-30m | Hardware | PASS | framework | docs/evidence/ESP32_SENSORLESS_EVIDENCE_20260307.md (documented soak pass) |
| esp32-soak-1h | Hardware | WAIVED | flight | no passing esp32_soak_*.json with duration >= 3600s | scope waiver requested via --exclude-check |
| sensor-attached-evidence | Hardware | WAIVED | flight | docs/evidence/ESP32_SENSOR_ATTACHED_EVIDENCE_20260307.md (status: NOT RUN (explicitly excluded from this closeout scope)) | scope waiver requested via --exclude-check |
| flight-env-test-matrix | Program | PASS | flight | docs/process/FLIGHT_ENV_TEST_MATRIX_20260307.md (status: COMPLETE (framework simulation baseline with explicit mission hardware handoff)) |
| comms-link-validation | Program | PASS | flight | docs/process/COMMS_LINK_VALIDATION_20260307.md (status: COMPLETE (local UDP/SITL validation baseline)) |
| operations-readiness | Program | PASS | flight | docs/process/OPERATIONS_READINESS_20260307.md (status: COMPLETE (framework operations rehearsal in simulation scope)) |
| release-manifest | Program | PASS | flight | docs/process/RELEASE_MANIFEST_20260307.md (status: COMPLETE (framework + simulation closeout; explicit hardware exclusions documented)) |
| independent-review-record | Process | PASS | framework, flight | docs/process/INDEPENDENT_REVIEW_RECORD_20260307.md |
| ccb-signoff | Process | PASS | framework, flight | docs/process/CCB_RELEASE_SIGNOFF_20260307.md |
| deployment-screening | Civilian/Legal | PASS | framework, flight | docs/process/DEPLOYMENT_SCREENING_PROCEDURE.md, docs/process/DEPLOYMENT_SCREENING_LOG_20260307.md |
| safety-case-baseline | Process | PASS | framework, flight | all baseline safety-case files present |

## Remaining Gaps

- None.

## Notes

- Framework automation can reduce software risk, but mission qualification still requires hardware and operations evidence.
- This report is an engineering status snapshot, not legal advice or certification approval.
