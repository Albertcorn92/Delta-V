# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-07T23:53:08.716097+00:00`
- Git commit: `01ebc70467a76e04aab3233fc3749a92bc58595d`
- Git branch: `main`
- Dirty worktree: `False`
- Host: `macOS-26.2-arm64-arm-64bit`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `24831.340` |
| Uplink latency p50 (us) | `37.291` |
| Uplink latency p95 (us) | `49.209` |
| CRC-16 throughput (MB/s) | `96.407` |
| COBS roundtrip throughput (MB/s) | `792.749` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `24831.340` | `N/A` |
| Uplink latency p95 (us) | `49.209` | `N/A` |
| CRC-16 throughput (MB/s) | `96.407` | `N/A` |
| COBS roundtrip throughput (MB/s) | `792.749` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
