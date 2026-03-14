# 10-Minute Quickstart

This flow validates the repository on a local machine:

1. Generated files are current.
2. Legal and scope checks pass.
3. Unit and system tests pass.
4. Benchmark thresholds pass.
5. The SITL smoke run starts cleanly and exits without fatal markers.

## Run

```bash
bash tools/quickstart_10min.sh
```

## Manual Equivalent

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target autocoder_check
cmake --build build --target legal_compliance
cmake --build build --target run_tests
cmake --build build --target run_system_tests
ctest --test-dir build --output-on-failure --timeout 90
cmake --build build --target benchmark_guard
cmake --build build --target benchmark_trend_guard
cmake --build build --target sitl_smoke
```

## Expected Result

- All commands above pass.
- Benchmark artifacts are written to `build/benchmark/`.
- SITL smoke artifacts are written to `build/sitl/`.
- No generated-file drift is reported.
