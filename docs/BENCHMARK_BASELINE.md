# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-10T07:06:40.464774+00:00`
- Git commit: `8698ddbb760aff3f4b281325ce947646802e11c3`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit`
- Benchmark runs: `3`
- Aggregation: `per-metric median across benchmark runs`
- Raw run artifact: `build/benchmark/benchmark_runs.json`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `25345.662` |
| Uplink latency p50 (us) | `36.666` |
| Uplink latency p95 (us) | `48.833` |
| CRC-16 throughput (MB/s) | `113.621` |
| COBS roundtrip throughput (MB/s) | `814.149` |
| Command router throughput (cmd/s) | `7642703.606` |
| Command router latency p95 (us) | `0.125` |
| Telem fanout throughput (pkt/s) | `19120458.891` |
| Telem fanout latency p95 (us) | `0.042` |

## Run Stability Snapshot

| Metric | Median | p95 | Min | Max |
|---|---|---|---|---|
| Uplink throughput (cmd/s) | `25345.662` | `25345.662` | `23294.109` | `25727.501` |
| Uplink latency p95 (us) | `48.833` | `48.833` | `47.000` | `54.666` |
| CRC-16 throughput (MB/s) | `113.621` | `113.621` | `113.451` | `115.905` |
| COBS roundtrip throughput (MB/s) | `814.149` | `814.149` | `806.698` | `827.033` |
| Command router throughput (cmd/s) | `7642703.606` | `7642703.606` | `6412739.291` | `8134214.540` |
| Command router latency p95 (us) | `0.125` | `0.125` | `0.125` | `0.125` |
| Telem fanout throughput (pkt/s) | `19120458.891` | `19120458.891` | `18570102.136` | `19126566.227` |
| Telem fanout latency p95 (us) | `0.042` | `0.042` | `0.042` | `0.042` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `25345.662` | `N/A` |
| Uplink latency p95 (us) | `48.833` | `N/A` |
| CRC-16 throughput (MB/s) | `113.621` | `N/A` |
| COBS roundtrip throughput (MB/s) | `814.149` | `N/A` |
| Command router throughput (cmd/s) | `7642703.606` | `N/A` |
| Command router latency p95 (us) | `0.125` | `N/A` |
| Telem fanout throughput (pkt/s) | `19120458.891` | `N/A` |
| Telem fanout latency p95 (us) | `0.042` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
