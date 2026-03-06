# 10-Minute Quickstart (Local)

Date: 2026-03-06

This flow validates DELTA-V end-to-end on a local machine:

1. Legal/scope checks
2. Build binaries
3. Unit + system tests
4. Software benchmark baseline
5. Short SITL smoke run

## Run

```bash
bash tools/quickstart_10min.sh
```

## Manual Equivalent

```bash
python3 tools/legal_compliance_check.py
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target flight_software
cmake --build build --target run_tests
cmake --build build --target run_system_tests
ctest --test-dir build --output-on-failure --timeout 90
cmake --build build --target benchmark_baseline
python3 tools/sitl_smoke.py --build-dir build --duration 8
```

## Expected Result

- All checks pass with no fatal runtime signatures.
- Benchmark artifacts are generated in `build/benchmark/`.
- `docs/BENCHMARK_BASELINE.*` are refreshed.
