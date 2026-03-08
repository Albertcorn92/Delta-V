# Simulation Operations Evidence

Date: 2026-03-07
Scope: DELTA-V civilian framework validation in local/SITL baseline

## Executed Gate Command

```bash
cmake --build build --target software_final
```

Result: PASS

Observed gate outcomes:

- Legal compliance gate: PASS
- Unit + system tests (`ctest`): PASS (2/2 suites)
- V&V stress gate: PASS (131 tests repeated with shuffle)
- Static safety gate (`tidy_safety`): PASS
- Traceability gate: PASS (33/33 requirements with direct tests)
- Benchmark guard: PASS
- SITL soak in gate profile: PASS

## Key Performance Snapshot

From `docs/BENCHMARK_BASELINE.json`:

- Uplink frames accepted: `20000/20000`
- Uplink p95 latency: `49.209 us`
- Uplink throughput: `24831.34 cmd/s`

## Scope Notes

- This evidence is simulation/local-integration scope and supports framework
  operations readiness.
- Sensor-attached evidence and 1-hour ESP32 soak are intentionally excluded from
  this closeout scope and tracked as explicit waivers in scope reports.
