# CubeSat Team Readiness Guide

Date: 2026-03-07

This guide defines a practical readiness flow for CubeSat teams using DELTA-V
while keeping the project in civilian scope.

## 1) Framework Gates (Automated)

Run software assurance gates first:

```bash
cmake --build build --target software_final
cmake --build build_cov --target coverage_guard
```

Then generate the readiness snapshot:

```bash
cmake --build build --target cubesat_readiness
```

The report is written to:

- `docs/CUBESAT_READINESS_STATUS.md`
- `docs/CUBESAT_READINESS_STATUS.json`

## 2) Hardware Evidence (Mission-Dependent)

Minimum recommended evidence set:

1. ESP32 runtime guard PASS (`tools/esp32_runtime_guard.py`)
2. ESP32 reboot campaign PASS (`tools/esp32_reboot_campaign.py`)
3. ESP32 soak PASS >= 3600s (`tools/esp32_soak.py`)
4. Sensor-attached run evidence (not simulation fallback)

Use:

- `docs/evidence/ESP32_SENSOR_ATTACHED_EVIDENCE_TEMPLATE.md`

## 3) Program Evidence (Mission-Dependent)

Use these templates for mission closeout records:

- `docs/process/FLIGHT_ENV_TEST_MATRIX_TEMPLATE.md`
- `docs/process/COMMS_LINK_VALIDATION_TEMPLATE.md`
- `docs/process/OPERATIONS_READINESS_TEMPLATE.md`
- `docs/process/RELEASE_MANIFEST_TEMPLATE.md`

## 4) Civilian Scope and Legal Hygiene

- Keep civilian-use policy documents current.
- Keep legal compliance checks in release flow.
- Do not claim legal clearance from framework reports.
- Mission operators remain responsible for destination/end-user screening before deployment.

## 5) Interpreting Readiness Status

- `framework_release_ready=true`: software/process baseline is in good standing.
- `cubesat_flight_ready=true`: framework plus mission evidence set is complete.

If `cubesat_flight_ready=false`, read the `Remaining Gaps` section in
`docs/CUBESAT_READINESS_STATUS.md` and close each item with evidence.

For scope-limited closeout runs (for example, excluding 1h soak or sensor-HIL),
generate a separate waived report:

```bash
python3 tools/cubesat_readiness_report.py \
  --workspace . \
  --build-dir build \
  --exclude-check esp32-soak-1h \
  --exclude-check sensor-attached-evidence \
  --output-md docs/CUBESAT_READINESS_STATUS_SCOPE.md \
  --output-json docs/CUBESAT_READINESS_STATUS_SCOPE.json
```
