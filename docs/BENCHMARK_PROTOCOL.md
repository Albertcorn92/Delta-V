# Benchmark Protocol (Software-Only)

Date: 2026-03-06

This protocol defines how to produce reproducible DELTA-V software benchmarks
and compare results against other frameworks on equal footing.

## Scope

- Software-only measurement on host/SITL.
- Civilian baseline only (no military features, no command-path crypto additions).
- Not a replacement for on-target HIL qualification.

## DELTA-V Baseline Commands

```bash
cmake --build build --target run_benchmarks
cmake --build build --target benchmark_baseline
cmake --build build --target benchmark_guard
```

`run_benchmarks` is compiled with release-style optimization flags for stable
cross-host regression behavior (even when the parent build directory is Debug).
`benchmark_baseline` runs benchmarks 3 times by default and writes median
aggregated metrics for regression gating.

Artifacts:

- `build/benchmark/benchmark_metrics.json`
- `build/benchmark/benchmark_runs.json`
- `build/benchmark/benchmark_baseline.md`
- `docs/BENCHMARK_BASELINE.md` (synced by default)
- `docs/BENCHMARK_BASELINE.json` (synced by default)
- `docs/BENCHMARK_THRESHOLDS.json` (regression gate thresholds)
- `docs/BENCHMARK_THRESHOLDS_CI.json` (GitHub CI threshold profile)

## Metrics (Current Baseline)

- Uplink command processing throughput (cmd/s)
- Uplink command processing latency p50/p95 (us)
- CRC-16 throughput (MB/s)
- COBS encode/decode roundtrip throughput (MB/s)
- CommandHub route+ACK throughput and latency p50/p95 (software path)
- TelemHub fanout throughput and latency p50/p95 (2-listener path)

## Comparison Rules

1. Same host machine and OS image for both frameworks.
2. Same compiler family/version and optimization flags.
3. Same payload sizes and command frame structures.
4. Minimum 3 runs per scenario; report median and p95.
5. Keep background load minimal and documented.

## Regression Gate Policy

- `benchmark_guard` validates current metrics against
  `docs/BENCHMARK_THRESHOLDS.json`.
- CI safety-assurance jobs use `docs/BENCHMARK_THRESHOLDS_CI.json` to account
  for shared-runner performance variance.
- Thresholds are intentionally conservative cross-host minimums for CI.
- If hardware/OS/compiler baseline changes materially, update thresholds with
  evidence in the release notes.

## Suggested Cross-Framework Matrix

- Startup time to "ready" marker
- Command throughput and p95 latency
- Telemetry handling throughput
- CPU and RSS during a fixed-duration SITL run

## Reporting Template

- Include exact commands used.
- Include commit hashes for all compared frameworks.
- Include raw JSON metrics files as artifacts.
- Avoid blanket "better than X" claims without data attached.
