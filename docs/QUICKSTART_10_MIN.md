# Quickstart

Use this guide for the first local validation run.

## Prerequisites

- Python 3.9+
- CMake 3.15+
- C++20 compiler
- `pip`

## Repository Validation

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target quickstart_10min
```

## What `quickstart_10min` Does

The target runs the normal local validation path:

1. generated-file freshness check
2. legal and scope checks
3. unit tests
4. system tests
5. benchmark threshold checks
6. SITL smoke run

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

The run is successful when:

- all commands exit with status `0`
- no generated-file drift is reported
- `build/benchmark/` contains benchmark artifacts
- `build/sitl/` contains SITL smoke artifacts

## Common Problems

Stale generated files:

```bash
python3 tools/autocoder.py
cmake --build build --target autocoder_check
```

Missing Python packages:

```bash
source .venv/bin/activate
pip install -r requirements.txt
```

Missing compiler or CMake:

- install the platform build tools first
- rerun the configure step

## Next Step

After the quickstart succeeds, continue with:

1. `docs/DEVELOPER_GUIDE.md`
2. `docs/ARCHITECTURE.md`
