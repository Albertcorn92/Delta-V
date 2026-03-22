# How To Assess DELTA-V

Use this guide to review the repository without reading every process file first.

## What The Repository Contains

DELTA-V includes:

- a host/SITL runtime
- an ESP32 port
- topology-driven code generation
- a Streamlit ground-data workflow
- tests, release tooling, and process records in the same repository

## What The Repository Proves

The repository provides evidence for:

- implemented runtime architecture
- enforced generated-file freshness
- requirements traceability
- unit and system testing
- stress, benchmark, and SITL validation
- a controlled public release process

Generated readiness and qualification reports in this repository are best read
as summaries of those local artifacts and gates. They are not independent
certification or mission-assurance findings.

## What The Repository Does Not Prove

The repository does not claim:

- hardware qualification
- sensor-attached HIL completion
- independent mission authority
- standalone flight readiness
- protected uplink security features in the public baseline

## Short Review Path

For a 10-15 minute review, read:

1. `README.md`
2. `docs/ARCHITECTURE.md`
3. `docs/SOFTWARE_FINAL_STATUS.md`
4. `docs/CUBESAT_READINESS_STATUS.md`
5. `docs/qualification_report.md`

## Code Review Path

For the implementation details, read:

1. `topology.yaml`
2. `tools/autocoder.py`
3. `src/TelemetryBridge.hpp`
4. `src/CommandHub.hpp`
5. `src/WatchdogComponent.hpp`
6. `tests/unit_tests.cpp`
7. `CMakeLists.txt`

## Process Review Path

For the assurance and release side, read:

1. `docs/process/SOFTWARE_CLASSIFICATION_BASELINE.md`
2. `docs/process/EXTERNAL_ASSURANCE_APPLICABILITY_BASELINE.md`
3. `docs/process/PSAC_DELTAV_BASELINE.md`
4. `docs/process/SOFTWARE_SAFETY_PLAN_BASELINE.md`
5. `docs/process/STATIC_ANALYSIS_DEVIATION_LOG.md`
6. `docs/process/TAILORING_AND_SCOPE_DEVIATIONS_BASELINE.md`
7. `docs/process/SVVP_DELTAV_BASELINE.md`
8. `docs/process/REFERENCE_PAYLOAD_PROFILE.md`
9. `docs/process/REFERENCE_MISSION_REQUIREMENTS_ALLOCATION.md`
10. `docs/process/RISK_REGISTER_BASELINE.md`
11. `docs/process/CONFIGURATION_AUDIT_BASELINE.md`
12. `docs/process/PUBLIC_SECURITY_POSTURE_BASELINE.md`
13. `docs/safety_case/`

## Strongest Signals

The clearest indicators in the repository are:

- generated-file checks are enforced
- runtime and dictionary generation share one source of truth
- requirements map to tests
- release and reviewer bundles are generated from current artifacts
- scope limits are documented explicitly

## Main Remaining Gaps

The main gaps are outside repository-only closure:

- sensor-attached HIL
- target-hardware timing and margin evidence
- environmental qualification
- independent review authority
- mission-specific security and operational approval

## Review Bundle

```bash
cmake --build build --target review_bundle
```

Output:

- `build/review_bundle/`
- `build/review_bundle.zip`
