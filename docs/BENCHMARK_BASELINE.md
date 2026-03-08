# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-08T06:02:44.676711+00:00`
- Git commit: `c403af0290dcb62e267da3e8d5a5c7c4bce9c680`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `25127.768` |
| Uplink latency p50 (us) | `37.417` |
| Uplink latency p95 (us) | `48.375` |
| CRC-16 throughput (MB/s) | `115.576` |
| COBS roundtrip throughput (MB/s) | `827.059` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `25127.768` | `N/A` |
| Uplink latency p95 (us) | `48.375` | `N/A` |
| CRC-16 throughput (MB/s) | `115.576` | `N/A` |
| COBS roundtrip throughput (MB/s) | `827.059` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
