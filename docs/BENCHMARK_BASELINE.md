# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-07T02:15:17.134913+00:00`
- Git commit: `1567e346f6776030337c0cf0c8383b1a69b090c8`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit-Mach-O`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `25093.551` |
| Uplink latency p50 (us) | `37.208` |
| Uplink latency p95 (us) | `49.708` |
| CRC-16 throughput (MB/s) | `115.842` |
| COBS roundtrip throughput (MB/s) | `845.540` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `25093.551` | `N/A` |
| Uplink latency p95 (us) | `49.708` | `N/A` |
| CRC-16 throughput (MB/s) | `115.842` | `N/A` |
| COBS roundtrip throughput (MB/s) | `845.540` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
