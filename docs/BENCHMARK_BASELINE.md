# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-07T08:01:56.508426+00:00`
- Git commit: `78941353402a04f6a11ae0cce4b30fefe7881598`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `18100.235` |
| Uplink latency p50 (us) | `44.000` |
| Uplink latency p95 (us) | `121.000` |
| CRC-16 throughput (MB/s) | `75.101` |
| COBS roundtrip throughput (MB/s) | `771.388` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `18100.235` | `N/A` |
| Uplink latency p95 (us) | `121.000` | `N/A` |
| CRC-16 throughput (MB/s) | `75.101` | `N/A` |
| COBS roundtrip throughput (MB/s) | `771.388` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
