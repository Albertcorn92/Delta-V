# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-07T02:09:34.243041+00:00`
- Git commit: `ba53e5c6f2d0f5be7a89f620e363351a8c5366b8`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit-Mach-O`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `16586.320` |
| Uplink latency p50 (us) | `43.708` |
| Uplink latency p95 (us) | `136.542` |
| CRC-16 throughput (MB/s) | `109.535` |
| COBS roundtrip throughput (MB/s) | `786.635` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `16586.320` | `N/A` |
| Uplink latency p95 (us) | `136.542` | `N/A` |
| CRC-16 throughput (MB/s) | `109.535` | `N/A` |
| COBS roundtrip throughput (MB/s) | `786.635` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
