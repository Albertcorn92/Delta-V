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

- Line coverage: `>= 70%`
- Branch coverage: `>= 50%`
- Function coverage: `>= 80%`

Next staged target (after trend stability):

- Line coverage: `>= 75%`
- Branch coverage: `>= 55%`
- Function coverage: `>= 85%`

## Trend Tracking

CI now publishes a coverage trend snapshot artifact (`coverage-trend`) generated
from `build_cov/cov.info`:

```bash
python3 tools/coverage_trend.py \
  --workspace . \
  --build-dir build_cov
```

This snapshot is used to verify trend stability before raising thresholds again.

## Notes

- Coverage generation is enabled in CMake when the active compiler is GCC.
- Threshold increases are applied in staged increments to limit CI volatility.
- This gate is software-only and does not replace on-target hardware verification.
