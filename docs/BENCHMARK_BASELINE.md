# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-08T18:12:15.039960+00:00`
- Git commit: `001f6c861ce8ae647bf880a7c74fed82a0ca8d29`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `19825.696` |
| Uplink latency p50 (us) | `39.167` |
| Uplink latency p95 (us) | `82.875` |
| CRC-16 throughput (MB/s) | `115.035` |
| COBS roundtrip throughput (MB/s) | `803.898` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `19825.696` | `N/A` |
| Uplink latency p95 (us) | `82.875` | `N/A` |
| CRC-16 throughput (MB/s) | `115.035` | `N/A` |
| COBS roundtrip throughput (MB/s) | `803.898` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
