#!/usr/bin/env bash

set -euo pipefail

echo "[quickstart] Running legal scope checks..."
python3 tools/legal_compliance_check.py

echo "[quickstart] Configuring build..."
cmake -B build -DCMAKE_BUILD_TYPE=Debug

echo "[quickstart] Building core binaries..."
cmake --build build --target flight_software
cmake --build build --target run_tests
cmake --build build --target run_system_tests
cmake --build build --target run_benchmarks

echo "[quickstart] Running system tests..."
ctest --test-dir build --output-on-failure --timeout 90

echo "[quickstart] Validating benchmark baseline + regression guard..."
cmake --build build --target benchmark_guard

echo "[quickstart] Running short SITL smoke..."
python3 tools/sitl_smoke.py --build-dir build --duration 8

echo "[quickstart] PASS: local 10-minute flow completed."
