# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-07T01:54:12.360726+00:00`
- Git commit: `bed6ae40cd19491b747e65df280ebd8c3ecc47b8`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `23235.361` |
| Uplink latency p50 (us) | `38.541` |
| Uplink latency p95 (us) | `51.833` |
| CRC-16 throughput (MB/s) | `20.201` |
| COBS roundtrip throughput (MB/s) | `202.181` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `23235.361` | `N/A` |
| Uplink latency p95 (us) | `51.833` | `N/A` |
| CRC-16 throughput (MB/s) | `20.201` | `N/A` |
| COBS roundtrip throughput (MB/s) | `202.181` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
