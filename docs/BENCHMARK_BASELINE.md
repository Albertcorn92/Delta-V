# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-14T14:55:34.162693+00:00`
- Git commit: `6438f271174bdb96ae8dbd8a74385a403e7f5d24`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit-Mach-O`
- Benchmark runs: `3`
- Aggregation: `per-metric median across benchmark runs`
- Raw run artifact: `build/benchmark/benchmark_runs.json`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `27167.232` |
| Uplink latency p50 (us) | `34.750` |
| Uplink latency p95 (us) | `45.833` |
| CRC-16 throughput (MB/s) | `119.266` |
| COBS roundtrip throughput (MB/s) | `847.149` |
| Command router throughput (cmd/s) | `8123889.312` |
| Command router latency p95 (us) | `0.125` |
| Telem fanout throughput (pkt/s) | `19116639.218` |
| Telem fanout latency p95 (us) | `0.042` |
| Startup time to ready (s) | `3.000` |
| Runtime sampling supported | `False` |
| Runtime sample count | `0` |
| Runtime CPU p95 (%) | `0.000` |
| Runtime RSS p95 (MB) | `0.000` |

## Run Stability Snapshot

| Metric | Median | p95 | Min | Max |
|---|---|---|---|---|
| Uplink throughput (cmd/s) | `27167.232` | `27167.232` | `24811.160` | `27556.905` |
| Uplink latency p95 (us) | `45.833` | `45.833` | `42.500` | `48.792` |
| CRC-16 throughput (MB/s) | `119.266` | `119.266` | `119.262` | `119.438` |
| COBS roundtrip throughput (MB/s) | `847.149` | `847.149` | `847.095` | `847.163` |
| Command router throughput (cmd/s) | `8123889.312` | `8123889.312` | `7603958.469` | `8191125.162` |
| Command router latency p95 (us) | `0.125` | `0.125` | `0.125` | `0.125` |
| Telem fanout throughput (pkt/s) | `19116639.218` | `19116639.218` | `19102963.061` | `19124261.803` |
| Telem fanout latency p95 (us) | `0.042` | `0.042` | `0.042` | `0.042` |
| Startup time to ready (s) | `3.000` | `3.000` | `3.000` | `3.000` |
| Runtime CPU p95 (%) | `0.000` | `0.000` | `0.000` | `0.000` |
| Runtime RSS p95 (MB) | `0.000` | `0.000` | `0.000` | `0.000` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `27167.232` | `N/A` |
| Uplink latency p95 (us) | `45.833` | `N/A` |
| CRC-16 throughput (MB/s) | `119.266` | `N/A` |
| COBS roundtrip throughput (MB/s) | `847.149` | `N/A` |
| Command router throughput (cmd/s) | `8123889.312` | `N/A` |
| Command router latency p95 (us) | `0.125` | `N/A` |
| Telem fanout throughput (pkt/s) | `19116639.218` | `N/A` |
| Telem fanout latency p95 (us) | `0.042` | `N/A` |
| Startup time to ready (s) | `3.000` | `N/A` |
| Runtime CPU p95 (%) | `0.000` | `N/A` |
| Runtime RSS p95 (MB) | `0.000` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
