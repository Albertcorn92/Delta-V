# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-08T17:18:07.584532+00:00`
- Git commit: `c91c47e52f5ae2ce272b510a941225217acafcda`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `17789.493` |
| Uplink latency p50 (us) | `40.541` |
| Uplink latency p95 (us) | `126.000` |
| CRC-16 throughput (MB/s) | `109.982` |
| COBS roundtrip throughput (MB/s) | `808.328` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `17789.493` | `N/A` |
| Uplink latency p95 (us) | `126.000` | `N/A` |
| CRC-16 throughput (MB/s) | `109.982` | `N/A` |
| COBS roundtrip throughput (MB/s) | `808.328` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
