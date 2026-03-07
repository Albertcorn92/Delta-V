# Safety Assurance Gates

Date: 2026-03-06

## Blocking Gates

The repository enforces these assurance gates in CI:

1. `vnv_stress`
- Runs the full GTest suite in shuffled repeated mode (`--gtest_shuffle --gtest_repeat=25` by default).
- Detects nondeterministic/flaky behavior before static-analysis and traceability gates.

2. `flight_readiness`
- Enforces legal checks, test execution, safety static analysis, traceability validation,
  and final flight binary build in a single target.
- Includes unit + system integration coverage through `ctest` (`DeltaV_Unit_Tests` and
  `DeltaV_System_Tests`).

3. `tidy_safety`
- Runs curated high-signal checks (`clang-analyzer-*` and selected `bugprone`/`cert` rules).
- Treats findings as errors.

4. `traceability`
- Validates one-to-one coverage between requirements in `src/Requirements.hpp`
  and mapped evidence in `docs/REQUIREMENTS_TRACE.yaml`.
- Verifies every mapped test name exists in `tests/unit_tests.cpp`.
- Emits artifacts:
  - `requirements_trace_matrix.md`
  - `requirements_trace_matrix.json`

5. `software_final`
- Depends on `qualification_bundle` after `flight_readiness` plus
  `benchmark_guard` and `sitl_soak`.
- Re-validates legal policy scan (civilian scope + no command-path crypto patterns)
  and trace/qualification status.
- Synchronizes release evidence into `docs/` and writes `docs/SOFTWARE_FINAL_STATUS.md`.

## Advisory Gates

- `tidy`: broad static-analysis profile for modernization and maintainability improvements.
- `benchmark_baseline`: regenerates reproducible host/SITL performance baseline
  (`docs/BENCHMARK_BASELINE.{md,json}`).
- `benchmark_guard`: enforces threshold-based performance regression checks from
  `docs/BENCHMARK_THRESHOLDS.json`.
- `sitl_smoke`: short runtime health check for startup markers, early process exit, and fatal signature scan.
- `sitl_soak`: extended runtime soak to detect delayed failures and unstable shutdown behavior.
- `coverage_guard`: enforces minimum line/branch/function coverage thresholds in CI.
- `coverage-trend` artifact: captures per-run coverage percentages for staged threshold increases.
- `quickstart_10min`: one-command local path for legal + build + tests + benchmark + smoke.
- `cubesat_readiness`: generates a consolidated CubeSat mission-readiness status snapshot.
- Coverage and Python tool checks remain active in CI.

## Command Reference

```bash
cmake --build build --target run_system_tests
cmake --build build --target tidy_safety
cmake --build build --target traceability
cmake --build build --target vnv_stress
cmake --build build --target flight_readiness
cmake --build build --target qualification_bundle
cmake --build build --target software_final
cmake --build build --target cubesat_readiness
cmake --build build --target benchmark_baseline
cmake --build build --target benchmark_guard
cmake --build build --target sitl_smoke
cmake --build build --target sitl_soak
cmake --build build_cov --target coverage_guard
cmake --build build --target quickstart_10min
```

## Qualification Bundle

`qualification_bundle` produces audit artifacts in:

- `build/qualification/qualification_report.md`
- `build/qualification/qualification_report.json`

`software_final` additionally updates:

- `docs/REQUIREMENTS_TRACE_MATRIX.md`
- `docs/REQUIREMENTS_TRACE_MATRIX.json`
- `docs/qualification_report.md`
- `docs/qualification_report.json`
- `docs/SOFTWARE_FINAL_STATUS.md`

Mission-team safety-case templates are maintained in:

- `docs/safety_case/README.md`
- `docs/safety_case/hazards.md`
- `docs/safety_case/mitigations.md`
- `docs/safety_case/verification_links.md`
- `docs/safety_case/change_impact.md`
