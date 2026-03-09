# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-09T04:58:02.949851+00:00`
- Git commit: `35822864d164c100fd343fc2f81f2261a810170b`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit-Mach-O`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `12547.484` |
| Uplink latency p50 (us) | `48.375` |
| Uplink latency p95 (us) | `186.333` |
| CRC-16 throughput (MB/s) | `98.294` |
| COBS roundtrip throughput (MB/s) | `744.113` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `12547.484` | `N/A` |
| Uplink latency p95 (us) | `186.333` | `N/A` |
| CRC-16 throughput (MB/s) | `98.294` | `N/A` |
| COBS roundtrip throughput (MB/s) | `744.113` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
