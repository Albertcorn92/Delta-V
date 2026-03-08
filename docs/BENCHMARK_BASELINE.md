# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-08T04:57:33.419248+00:00`
- Git commit: `885d1762e9c66abbd160a7a7a3fefe20f8a7f5e8`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `19691.237` |
| Uplink latency p50 (us) | `41.000` |
| Uplink latency p95 (us) | `78.875` |
| CRC-16 throughput (MB/s) | `114.552` |
| COBS roundtrip throughput (MB/s) | `814.576` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `19691.237` | `N/A` |
| Uplink latency p95 (us) | `78.875` | `N/A` |
| CRC-16 throughput (MB/s) | `114.552` | `N/A` |
| COBS roundtrip throughput (MB/s) | `814.576` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
