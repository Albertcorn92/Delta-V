# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-10T06:43:54.577651+00:00`
- Git commit: `4519794f90514e579bc3d62df3e983177cf77465`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit`
- Benchmark runs: `3`
- Aggregation: `per-metric median across benchmark runs`
- Raw run artifact: `build/benchmark/benchmark_runs.json`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `27045.217` |
| Uplink latency p50 (us) | `34.875` |
| Uplink latency p95 (us) | `45.167` |
| CRC-16 throughput (MB/s) | `116.031` |
| COBS roundtrip throughput (MB/s) | `811.800` |
| Command router throughput (cmd/s) | `7952551.894` |
| Command router latency p95 (us) | `0.125` |
| Telem fanout throughput (pkt/s) | `18769790.398` |
| Telem fanout latency p95 (us) | `0.042` |

## Run Stability Snapshot

| Metric | Median | p95 | Min | Max |
|---|---|---|---|---|
| Uplink throughput (cmd/s) | `27045.217` | `27045.217` | `25372.902` | `27215.550` |
| Uplink latency p95 (us) | `45.167` | `45.167` | `43.792` | `46.459` |
| CRC-16 throughput (MB/s) | `116.031` | `116.031` | `115.601` | `116.346` |
| COBS roundtrip throughput (MB/s) | `811.800` | `811.800` | `809.791` | `827.803` |
| Command router throughput (cmd/s) | `7952551.894` | `7952551.894` | `7900454.276` | `7982438.635` |
| Command router latency p95 (us) | `0.125` | `0.125` | `0.125` | `0.125` |
| Telem fanout throughput (pkt/s) | `18769790.398` | `18769790.398` | `18725855.350` | `18770530.267` |
| Telem fanout latency p95 (us) | `0.042` | `0.042` | `0.042` | `0.042` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `27045.217` | `N/A` |
| Uplink latency p95 (us) | `45.167` | `N/A` |
| CRC-16 throughput (MB/s) | `116.031` | `N/A` |
| COBS roundtrip throughput (MB/s) | `811.800` | `N/A` |
| Command router throughput (cmd/s) | `7952551.894` | `N/A` |
| Command router latency p95 (us) | `0.125` | `N/A` |
| Telem fanout throughput (pkt/s) | `18769790.398` | `N/A` |
| Telem fanout latency p95 (us) | `0.042` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
