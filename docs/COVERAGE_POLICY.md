# Coverage Policy (Software Baseline)

Date: 2026-03-06

DELTA-V enforces minimum coverage thresholds in CI using `coverage_guard`.

## Commands

```bash
cmake -B build_cov -DCMAKE_BUILD_TYPE=Debug
cmake --build build_cov --target coverage
cmake --build build_cov --target coverage_guard
```

## Threshold Source

- `docs/COVERAGE_THRESHOLDS.json`

Current minimums:

- Line coverage: `>= 60%`
- Branch coverage: `>= 40%`
- Function coverage: `>= 70%`

## Notes

- Thresholds are set for cross-host CI stability and may be tightened over time.
- This gate is software-only and does not replace on-target hardware verification.
