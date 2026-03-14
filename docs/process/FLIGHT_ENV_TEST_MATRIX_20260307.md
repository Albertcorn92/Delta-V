# Flight Environment Test Matrix

Date: 2026-03-07
Mission: DELTA-V framework baseline
Release: main (software gates dated 2026-03-07)
Status: COMPLETE (framework simulation baseline with explicit mission hardware handoff)

This matrix records framework-level evidence available inside this repository.
Physical qualification campaigns are mission-integrator activities and must be
executed on mission hardware articles.

## Test Matrix

| Test Area | Procedure Ref | Unit/Article | Profile | Pass Criteria | Result | Evidence Path | Owner | Date |
|---|---|---|---|---|---|---|---|---|
| Vibration (software proxy) | `docs/SAFETY_ASSURANCE.md` | SITL binary | `vnv_stress` shuffled repeats + ASan/UBSan build | All tests pass, no fatal runtime signatures | PASS | `docs/qualification_report.json`, `docs/evidence/SIM_OPERATIONS_EVIDENCE_20260307.md` | Framework maintainer | 2026-03-07 |
| Shock / reset resilience | `tools/esp32_reboot_campaign.py` | ESP32 baseline board (no external sensors) | 10 reboot cycles | 0 failed cycles | PASS | `artifacts/esp32_reboot_campaign_20260307T072234Z.json` | Framework maintainer | 2026-03-07 |
| Thermal cycle (software proxy) | `tools/sitl_soak.py` | SITL binary | 600 s soak in software_final gate | Required markers present, no fatal signatures | PASS | `docs/SOFTWARE_FINAL_STATUS.md`, `build/sitl/sitl_soak_result.json` | Framework maintainer | 2026-03-14 |
| Thermal vacuum | Mission hardware procedure | Flight article | TVAC chamber campaign | Mission acceptance criteria met | MISSION HANDOFF | `docs/process/RELEASE_MANIFEST_20260307.md` | Mission team | mission phase |
| Power cycle resilience | `tools/esp32_runtime_guard.py` + reboot campaign | ESP32 baseline board | Runtime guard + reboot campaign | Thresholds met and reboot campaign passes | PASS | `artifacts/esp32_runtime_guard_20260307T071843Z.json` | Framework maintainer | 2026-03-07 |
| EMI/EMC (protocol robustness proxy) | `tests/unit_tests.cpp` communication suites | SITL/unit-test harness | COBS/CRC/replay malformed-frame tests | Relevant tests pass in qualification gates | PASS | `docs/REQUIREMENTS_TRACE_MATRIX.md` | Framework maintainer | 2026-03-07 |

## Notes

- This record intentionally separates software-scope evidence from mission
  hardware environmental qualification.
- Mission environmental certification evidence is outside this repository scope
  and must be attached by the operating mission team.
