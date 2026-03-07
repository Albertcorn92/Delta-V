# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-07T00:10:17.768926+00:00`
- Git commit: `6efbcff9ff1ed19fb79959c2a70a42726e4e612d`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `24693.633` |
| Uplink latency p50 (us) | `36.750` |
| Uplink latency p95 (us) | `49.625` |
| CRC-16 throughput (MB/s) | `20.457` |
| COBS roundtrip throughput (MB/s) | `203.199` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `24693.633` | `N/A` |
| Uplink latency p95 (us) | `49.625` | `N/A` |
| CRC-16 throughput (MB/s) | `20.457` | `N/A` |
| COBS roundtrip throughput (MB/s) | `203.199` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
