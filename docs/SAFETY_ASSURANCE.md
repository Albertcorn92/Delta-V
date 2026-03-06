# Safety Assurance Gates

Date: 2026-03-06

## Blocking Gates

The repository enforces these assurance gates in CI:

1. `tidy_safety`
- Runs curated high-signal checks (`clang-analyzer-*` and selected `bugprone`/`cert` rules).
- Treats findings as errors.

2. `traceability`
- Validates one-to-one coverage between requirements in `src/Requirements.hpp`
  and mapped evidence in `docs/REQUIREMENTS_TRACE.yaml`.
- Verifies every mapped test name exists in `tests/unit_tests.cpp`.
- Emits artifacts:
  - `requirements_trace_matrix.md`
  - `requirements_trace_matrix.json`

3. `software_final`
- Depends on `qualification_bundle` after `flight_readiness`.
- Re-validates legal policy scan and trace/qualification status.
- Synchronizes release evidence into `docs/` and writes `docs/SOFTWARE_FINAL_STATUS.md`.

## Advisory Gates

- `tidy`: broad static-analysis profile for modernization and maintainability improvements.
- Coverage and Python tool checks remain active in CI.

## Command Reference

```bash
cmake --build build --target tidy_safety
cmake --build build --target traceability
cmake --build build --target flight_readiness
cmake --build build --target qualification_bundle
cmake --build build --target software_final
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
