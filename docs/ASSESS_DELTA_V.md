# How To Assess DELTA-V

This document is for reviewers, recruiters, and adopters who want to judge the
repository quickly without reading the entire process package first.

## What This Repository Is

DELTA-V is a C++20 flight-software framework with:

- a host/SITL runtime,
- an ESP32 port,
- topology-driven code generation,
- a ground-data workflow,
- and an in-repo assurance package.

It is a serious software framework. It is not a complete flight program.

## What The Repository Already Proves

- The runtime architecture is coherent and implemented, not just described.
- The generated topology flow is part of the enforced build path.
- Requirements traceability exists and closes at `37/37`.
- Unit, system, stress, static-analysis, benchmark, soak, and live SITL fault
  checks are part of the normal software closeout path.
- The process package is now mission-shaped around one reference mission rather
  than being only generic templates.
- Civilian/legal scope is explicit and enforced in the repo.

## What The Repository Does Not Claim

- It does not claim NASA flight readiness.
- It does not claim hardware qualification.
- It does not claim sensor-attached HIL closure.
- It does not claim independent program authority.
- It does not ship command-path crypto/auth in the public baseline.

If someone reads this repo as “ready to fly a spacecraft by itself,” that is a
misread.

## Fast Review Path

If you only have 10-15 minutes, open these files in order:

1. `README.md`
2. `docs/ARCHITECTURE.md`
3. `docs/SOFTWARE_FINAL_STATUS.md`
4. `docs/CUBESAT_READINESS_STATUS.md`
5. `docs/qualification_report.md`

That path tells you:

- what the framework is,
- how it is structured,
- what passed,
- what did not pass,
- and what remains outside software scope.

## Technical Review Path

If you want to assess engineering quality, review these next:

1. `topology.yaml`
2. `tools/autocoder.py`
3. `src/TelemetryBridge.hpp`
4. `src/CommandHub.hpp`
5. `src/WatchdogComponent.hpp`
6. `tests/unit_tests.cpp`
7. `CMakeLists.txt`

This path shows:

- the source of truth,
- how generation is controlled,
- how the command path is validated,
- how FDIR is implemented,
- how tests are written,
- and what the build actually enforces.

## Assurance Review Path

If you want to judge process maturity, review these:

1. `docs/process/SOFTWARE_CLASSIFICATION_BASELINE.md`
2. `docs/process/NASA_REQUIREMENTS_APPLICABILITY_BASELINE.md`
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

This is where the repo stops looking like “just code” and starts looking like a
governed software baseline.

## Best Signal For A Reviewer

The strongest signals in this repository are:

- enforced generated-file freshness,
- real traceability,
- live malformed/replay fault exercise in SITL,
- consistent documentation about what is still missing,
- and a clean separation between framework closure and flight-program closure.

## Remaining Gaps

The highest-value missing items are not Mac-only documentation issues anymore.
They are:

- sensor-attached HIL,
- target-hardware timing and margin evidence,
- environmental qualification,
- independent verification authority,
- and a clean tagged release pedigree for an external release unit.

## One Command For Reviewers

Run:

```bash
cmake --build build --target review_bundle
```

That stages a curated reviewer bundle in `build/review_bundle/` and writes
`build/review_bundle.zip`.
