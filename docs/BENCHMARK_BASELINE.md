# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-06T22:56:06.039198+00:00`
- Git commit: `927517e4ca00c6083c1d87962de9cce191efe76c`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `25334.601` |
| Uplink latency p50 (us) | `36.458` |
| Uplink latency p95 (us) | `47.209` |
| CRC-16 throughput (MB/s) | `20.155` |
| COBS roundtrip throughput (MB/s) | `214.518` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (fill) |
|---|---|---|
| Uplink throughput (cmd/s) | `25334.601` | `TBD` |
| Uplink latency p95 (us) | `47.209` | `TBD` |
| CRC-16 throughput (MB/s) | `20.155` | `TBD` |
| COBS roundtrip throughput (MB/s) | `214.518` | `TBD` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
