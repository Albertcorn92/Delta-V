# DELTA-V Benchmark Baseline

- Generated (UTC): `2026-03-14T22:53:13.788393+00:00`
- Git commit: `b7d2386fd715ceb1822d5e67227a821c6ebce264`
- Git branch: `main`
- Dirty worktree: `True`
- Host: `macOS-26.2-arm64-arm-64bit-Mach-O`
- Benchmark runs: `3`
- Aggregation: `per-metric median across benchmark runs`
- Raw run artifact: `build/benchmark/benchmark_runs.json`

## DELTA-V Software Metrics

| Metric | Value |
|---|---|
| Uplink throughput (cmd/s) | `18953.864` |
| Uplink latency p50 (us) | `49.375` |
| Uplink latency p95 (us) | `72.417` |
| CRC-16 throughput (MB/s) | `113.414` |
| COBS roundtrip throughput (MB/s) | `813.601` |
| Command router throughput (cmd/s) | `7786770.200` |
| Command router latency p95 (us) | `0.125` |
| Telem fanout throughput (pkt/s) | `18247492.566` |
| Telem fanout latency p95 (us) | `0.042` |
| Startup time to ready (s) | `3.000` |
| Runtime sampling supported | `False` |
| Runtime sample count | `0` |
| Runtime CPU p95 (%) | `0.000` |
| Runtime RSS p95 (MB) | `0.000` |

## Run Stability Snapshot

| Metric | Median | p95 | Min | Max |
|---|---|---|---|---|
| Uplink throughput (cmd/s) | `18953.864` | `18953.864` | `15348.110` | `19175.203` |
| Uplink latency p95 (us) | `72.417` | `72.417` | `66.583` | `98.417` |
| CRC-16 throughput (MB/s) | `113.414` | `113.414` | `113.251` | `113.863` |
| COBS roundtrip throughput (MB/s) | `813.601` | `813.601` | `787.570` | `818.326` |
| Command router throughput (cmd/s) | `7786770.200` | `7786770.200` | `7519622.455` | `7946362.056` |
| Command router latency p95 (us) | `0.125` | `0.125` | `0.125` | `0.125` |
| Telem fanout throughput (pkt/s) | `18247492.566` | `18247492.566` | `18026820.303` | `18774212.868` |
| Telem fanout latency p95 (us) | `0.042` | `0.042` | `0.042` | `0.042` |
| Startup time to ready (s) | `3.000` | `3.000` | `3.000` | `3.000` |
| Runtime CPU p95 (%) | `0.000` | `0.000` | `0.000` | `0.000` |
| Runtime RSS p95 (MB) | `0.000` | `0.000` | `0.000` | `0.000` |

## Comparison Template (Fill with External Baselines)

| Metric | DELTA-V | F Prime (external data) |
|---|---|---|
| Uplink throughput (cmd/s) | `18953.864` | `N/A` |
| Uplink latency p95 (us) | `72.417` | `N/A` |
| CRC-16 throughput (MB/s) | `113.414` | `N/A` |
| COBS roundtrip throughput (MB/s) | `813.601` | `N/A` |
| Command router throughput (cmd/s) | `7786770.200` | `N/A` |
| Command router latency p95 (us) | `0.125` | `N/A` |
| Telem fanout throughput (pkt/s) | `18247492.566` | `N/A` |
| Telem fanout latency p95 (us) | `0.042` | `N/A` |
| Startup time to ready (s) | `3.000` | `N/A` |
| Runtime CPU p95 (%) | `0.000` | `N/A` |
| Runtime RSS p95 (MB) | `0.000` | `N/A` |

## Reproducibility

- Build command: `cmake --build build --target run_benchmarks`
- Run command: `cmake --build build --target benchmark_baseline`
- Method reference: `docs/BENCHMARK_PROTOCOL.md`

Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.
