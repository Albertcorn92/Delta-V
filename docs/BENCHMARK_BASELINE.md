# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-08T00:19:49.997010+00:00`
- Git commit: `688145aa49732155fc701dcec800b0207b708b6b`
- Git branch: `main`
- Dirty worktree: `False`
- Host: `macOS-26.2-arm64-arm-64bit`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `25078.644` |
| Uplink latency p50 (us) | `37.542` |
| Uplink latency p95 (us) | `47.583` |
| CRC-16 throughput (MB/s) | `117.221` |
| COBS roundtrip throughput (MB/s) | `833.886` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `25078.644` | `N/A` |
| Uplink latency p95 (us) | `47.583` | `N/A` |
| CRC-16 throughput (MB/s) | `117.221` | `N/A` |
| COBS roundtrip throughput (MB/s) | `833.886` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
