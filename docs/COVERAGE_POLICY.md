# Coverage Policy

Date: 2026-03-06

The coverage gates run in a dedicated GCC build because the repository uses `lcov`.

## Configure the Coverage Build

Use the helper script to select an available GCC/G++ pair and configure `build_cov`:

```bash
bash tools/configure_coverage_build.sh
```

Manual equivalent:

```bash
cmake -B build_cov -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc-12 -DCMAKE_CXX_COMPILER=g++-12
```

## Run the Coverage Gates

```bash
cmake --build build_cov --target coverage
cmake --build build_cov --target coverage_guard
cmake --build build_cov --target coverage_trend
```

## Threshold Source

- `docs/COVERAGE_THRESHOLDS.json`

Current minimums:

- Line coverage: `>= 70%`
- Branch coverage: `>= 50%`
- Function coverage: `>= 80%`

Planned next step:

- Line coverage: `>= 75%`
- Branch coverage: `>= 55%`
- Function coverage: `>= 85%`

## Notes

- Coverage generation is enabled only when the active compiler is GCC.
- Coverage is a software-only gate. It does not replace hardware validation.
- CI archives the coverage HTML report and the trend snapshot artifact.
