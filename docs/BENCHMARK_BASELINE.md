# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-10T07:00:55.308109+00:00`
- Git commit: `a1467623d7edfabb61867e2c09f68750537048b7`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit`
- Benchmark runs: `3`
- Aggregation: `per-metric median across benchmark runs`
- Raw run artifact: `build/benchmark/benchmark_runs.json`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `26846.663` |
| Uplink latency p50 (us) | `35.166` |
| Uplink latency p95 (us) | `45.042` |
| CRC-16 throughput (MB/s) | `115.790` |
| COBS roundtrip throughput (MB/s) | `828.682` |
| Command router throughput (cmd/s) | `7945045.708` |
| Command router latency p95 (us) | `0.125` |
| Telem fanout throughput (pkt/s) | `18338108.883` |
| Telem fanout latency p95 (us) | `0.042` |

## Run Stability Snapshot

| Metric | Median | p95 | Min | Max |
|---|---|---|---|---|
| Uplink throughput (cmd/s) | `26846.663` | `26846.663` | `25545.583` | `27772.196` |
| Uplink latency p95 (us) | `45.042` | `45.042` | `43.500` | `47.875` |
| CRC-16 throughput (MB/s) | `115.790` | `115.790` | `114.169` | `116.661` |
| COBS roundtrip throughput (MB/s) | `828.682` | `828.682` | `790.798` | `829.424` |
| Command router throughput (cmd/s) | `7945045.708` | `7945045.708` | `7865499.951` | `8021523.351` |
| Command router latency p95 (us) | `0.125` | `0.125` | `0.125` | `0.125` |
| Telem fanout throughput (pkt/s) | `18338108.883` | `18338108.883` | `18284310.051` | `18480735.681` |
| Telem fanout latency p95 (us) | `0.042` | `0.042` | `0.042` | `0.042` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `26846.663` | `N/A` |
| Uplink latency p95 (us) | `45.042` | `N/A` |
| CRC-16 throughput (MB/s) | `115.790` | `N/A` |
| COBS roundtrip throughput (MB/s) | `828.682` | `N/A` |
| Command router throughput (cmd/s) | `7945045.708` | `N/A` |
| Command router latency p95 (us) | `0.125` | `N/A` |
| Telem fanout throughput (pkt/s) | `18338108.883` | `N/A` |
| Telem fanout latency p95 (us) | `0.042` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
