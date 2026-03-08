# Comms Link Validation

Date: 2026-03-07
Mission: DELTA-V framework baseline
Release: main (software gates dated 2026-03-07)
Status: COMPLETE (local UDP/SITL validation baseline)

This record captures comms validation that can be executed directly in the
open-source framework using local UDP and automated tests.

## Link Configuration

- Ground station ID: local host simulation (`127.0.0.1`)
- Radio profile: TelemetryBridge UDP baseline
- Command path mode: local-only baseline with replay protection
- Time sync source: host system clock (UTC display in GDS)

## Validation Runs

| Run ID | Scenario | Uplink Commands Sent | Ack Rate | Packet Loss | Latency p95 | Result | Evidence Path |
|---|---|---:|---:|---:|---:|---|---|
| RUN-001 | Nominal local command path | 20000 | 100% accepted/consumed | 0% | 49.209 us | PASS | `docs/BENCHMARK_BASELINE.json`, `docs/evidence/SIM_OPERATIONS_EVIDENCE_20260307.md` |
| RUN-002 | Safe-mode command gate enforcement | 1 operational command attempt in SAFE_MODE | 100% correctly blocked | 0% | n/a (policy gate test) | PASS | `tests/system_tests.cpp` (`SafeModeBlocksOperationalCommand`) |
| RUN-003 | Replay/drop recovery behavior | randomized malformed/replay corpus | 100% replay rejects where expected | 0% silent corruption observed | n/a (property tests) | PASS | `docs/REQUIREMENTS_TRACE_MATRIX.md` (`DV-COMM-04`, `DV-COMM-06`) |

## Safety Checks

- Command approval workflow verified for framework simulation scope: YES
- Safe-mode command set verified in tests: YES
- Replay/sequence behavior verified in tests: YES

## Notes

- This baseline is intentionally local/SITL and validates comms software behavior.
- Mission RF-chain validation (antennas, link budgets, field interference) must
  be executed by mission operators with mission hardware.
