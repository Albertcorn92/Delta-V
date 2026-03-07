# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-07T22:58:52.979236+00:00`
- Git commit: `1b7036a796e232f8aee81969bcc8a203a33bbc94`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `24407.318` |
| Uplink latency p50 (us) | `37.459` |
| Uplink latency p95 (us) | `51.041` |
| CRC-16 throughput (MB/s) | `116.627` |
| COBS roundtrip throughput (MB/s) | `816.524` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `24407.318` | `N/A` |
| Uplink latency p95 (us) | `51.041` | `N/A` |
| CRC-16 throughput (MB/s) | `116.627` | `N/A` |
| COBS roundtrip throughput (MB/s) | `816.524` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
