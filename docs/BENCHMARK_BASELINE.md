# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-16T23:12:21.583978+00:00`
- Git commit: `8f44015cf3676b1a9cfe825d3e304d93fc900e74`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit-Mach-O`
- Benchmark runs: `3`
- Aggregation: `per-metric median across benchmark runs`
- Raw run artifact: `build/benchmark/benchmark_runs.json`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `24330.430` |
| Uplink latency p50 (us) | `35.916` |
| Uplink latency p95 (us) | `53.042` |
| CRC-16 throughput (MB/s) | `117.112` |
| COBS roundtrip throughput (MB/s) | `839.383` |
| Command router throughput (cmd/s) | `8104410.745` |
| Command router latency p95 (us) | `0.125` |
| Telem fanout throughput (pkt/s) | `18908059.560` |
| Telem fanout latency p95 (us) | `0.042` |
| Startup time to ready (s) | `3.000` |
| Runtime sampling supported | `False` |
| Runtime sample count | `0` |
| Runtime CPU p95 (%) | `0.000` |
| Runtime RSS p95 (MB) | `0.000` |

## Run Stability Snapshot

| Metric | Median | p95 | Min | Max |
|---|---|---|---|---|
| Uplink throughput (cmd/s) | `24330.430` | `24330.430` | `22514.939` | `27310.245` |
| Uplink latency p95 (us) | `53.042` | `53.042` | `44.959` | `59.625` |
| CRC-16 throughput (MB/s) | `117.112` | `117.112` | `116.558` | `117.294` |
| COBS roundtrip throughput (MB/s) | `839.383` | `839.383` | `827.578` | `846.326` |
| Command router throughput (cmd/s) | `8104410.745` | `8104410.745` | `8092526.713` | `8209621.183` |
| Command router latency p95 (us) | `0.125` | `0.125` | `0.125` | `0.125` |
| Telem fanout throughput (pkt/s) | `18908059.560` | `18908059.560` | `18866447.250` | `18919238.501` |
| Telem fanout latency p95 (us) | `0.042` | `0.042` | `0.042` | `0.042` |
| Startup time to ready (s) | `3.000` | `3.000` | `3.000` | `3.000` |
| Runtime CPU p95 (%) | `0.000` | `0.000` | `0.000` | `0.000` |
| Runtime RSS p95 (MB) | `0.000` | `0.000` | `0.000` | `0.000` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `24330.430` | `N/A` |
| Uplink latency p95 (us) | `53.042` | `N/A` |
| CRC-16 throughput (MB/s) | `117.112` | `N/A` |
| COBS roundtrip throughput (MB/s) | `839.383` | `N/A` |
| Command router throughput (cmd/s) | `8104410.745` | `N/A` |
| Command router latency p95 (us) | `0.125` | `N/A` |
| Telem fanout throughput (pkt/s) | `18908059.560` | `N/A` |
| Telem fanout latency p95 (us) | `0.042` | `N/A` |
| Startup time to ready (s) | `3.000` | `N/A` |
| Runtime CPU p95 (%) | `0.000` | `N/A` |
| Runtime RSS p95 (MB) | `0.000` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
