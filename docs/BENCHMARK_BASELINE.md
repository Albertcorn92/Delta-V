# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-10T07:29:09.517734+00:00`
- Git commit: `70d94a7c191e862fb830c894cd8ab7ac4395b426`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit`
- Benchmark runs: `3`
- Aggregation: `per-metric median across benchmark runs`
- Raw run artifact: `build/benchmark/benchmark_runs.json`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `25414.283` |
| Uplink latency p50 (us) | `36.042` |
| Uplink latency p95 (us) | `53.292` |
| CRC-16 throughput (MB/s) | `116.014` |
| COBS roundtrip throughput (MB/s) | `821.172` |
| Command router throughput (cmd/s) | `8049368.386` |
| Command router latency p95 (us) | `0.125` |
| Telem fanout throughput (pkt/s) | `18291283.198` |
| Telem fanout latency p95 (us) | `0.042` |
| Startup time to ready (s) | `3.000` |
| Runtime sampling supported | `False` |
| Runtime sample count | `0` |
| Runtime CPU p95 (%) | `0.000` |
| Runtime RSS p95 (MB) | `0.000` |

## Run Stability Snapshot

| Metric | Median | p95 | Min | Max |
|---|---|---|---|---|
| Uplink throughput (cmd/s) | `25414.283` | `25414.283` | `24883.530` | `27122.588` |
| Uplink latency p95 (us) | `53.292` | `53.292` | `44.875` | `54.250` |
| CRC-16 throughput (MB/s) | `116.014` | `116.014` | `115.373` | `117.306` |
| COBS roundtrip throughput (MB/s) | `821.172` | `821.172` | `735.649` | `832.930` |
| Command router throughput (cmd/s) | `8049368.386` | `8049368.386` | `7890713.616` | `8173273.396` |
| Command router latency p95 (us) | `0.125` | `0.125` | `0.125` | `0.125` |
| Telem fanout throughput (pkt/s) | `18291283.198` | `18291283.198` | `17176596.887` | `19068049.100` |
| Telem fanout latency p95 (us) | `0.042` | `0.042` | `0.042` | `0.042` |
| Startup time to ready (s) | `3.000` | `3.000` | `3.000` | `3.000` |
| Runtime CPU p95 (%) | `0.000` | `0.000` | `0.000` | `0.000` |
| Runtime RSS p95 (MB) | `0.000` | `0.000` | `0.000` | `0.000` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `25414.283` | `N/A` |
| Uplink latency p95 (us) | `53.292` | `N/A` |
| CRC-16 throughput (MB/s) | `116.014` | `N/A` |
| COBS roundtrip throughput (MB/s) | `821.172` | `N/A` |
| Command router throughput (cmd/s) | `8049368.386` | `N/A` |
| Command router latency p95 (us) | `0.125` | `N/A` |
| Telem fanout throughput (pkt/s) | `18291283.198` | `N/A` |
| Telem fanout latency p95 (us) | `0.042` | `N/A` |
| Startup time to ready (s) | `3.000` | `N/A` |
| Runtime CPU p95 (%) | `0.000` | `N/A` |
| Runtime RSS p95 (MB) | `0.000` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
