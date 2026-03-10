# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-10T06:36:58.238406+00:00`
- Git commit: `e8e7b8e2825a0073ff1dc75bf668fe2a6a598d08`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `23497.438` |
| Uplink latency p50 (us) | `37.334` |
| Uplink latency p95 (us) | `55.167` |
| CRC-16 throughput (MB/s) | `112.653` |
| COBS roundtrip throughput (MB/s) | `799.871` |
| Command router throughput (cmd/s) | `7980447.903` |
| Command router latency p95 (us) | `0.125` |
| Telem fanout throughput (pkt/s) | `18754389.699` |
| Telem fanout latency p95 (us) | `0.042` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `23497.438` | `N/A` |
| Uplink latency p95 (us) | `55.167` | `N/A` |
| CRC-16 throughput (MB/s) | `112.653` | `N/A` |
| COBS roundtrip throughput (MB/s) | `799.871` | `N/A` |
| Command router throughput (cmd/s) | `7980447.903` | `N/A` |
| Command router latency p95 (us) | `0.125` | `N/A` |
| Telem fanout throughput (pkt/s) | `18754389.699` | `N/A` |
| Telem fanout latency p95 (us) | `0.042` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
