# Coverage Policy (Software Baseline)

Date: 2026-03-06

DELTA-V enforces minimum coverage thresholds in CI using `coverage_guard`.

## Commands

```bash
cmake -B build_cov -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc-12 -DCMAKE_CXX_COMPILER=g++-12
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

- Coverage generation is enabled in CMake when the active compiler is GCC.
- Thresholds are set for cross-host CI stability and may be tightened over time.
- This gate is software-only and does not replace on-target hardware verification.
