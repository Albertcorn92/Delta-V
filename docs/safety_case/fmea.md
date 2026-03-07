# Failure Modes and Effects Analysis (Baseline)

Date: 2026-03-07

This FMEA is for framework-level software behavior only. It is not a complete
mission FMEA and must be extended with vehicle, payload, and operations inputs.

| FMEA ID | Function | Failure Mode | Local Effect | End Effect | Severity | Detection | Control / Mitigation | Requirement IDs | Evidence |
|---|---|---|---|---|---|---|---|---|---|
| FMEA-001 | Uplink replay protection | Replay command accepted | Duplicate command execution | Unintended repeated state/action | Hazardous | Sequence-number check | Reject replay + persist accepted sequence | DV-COMM-04, DV-COMM-06 | `tests/unit_tests.cpp`, `docs/qualification_report.md` |
| FMEA-002 | Uplink parser validation | Non-canonical frame accepted | Invalid command data enters dispatch | Undefined command handling path | Catastrophic | Header/length checks | Reject invalid/truncated/oversized frame | DV-SEC-01, DV-SEC-03 | `tests/unit_tests.cpp`, `docs/qualification_report.md` |
| FMEA-003 | Command routing | Route queue full but ACK emitted | False success telemetry | Lost command with no retry trigger | Major | Route send status check | NACK on route saturation + error counter increment | DV-FDIR-08 | `tests/unit_tests.cpp` |
| FMEA-004 | Timekeeping | MET wrap handling omitted | Aging mission counters drift assumptions | Incorrect timing-dependent behavior | Major | Wrap-threshold warning | One-time warning + 64-bit MET accessor | DV-TIME-02 | `tests/unit_tests.cpp` |
| FMEA-005 | Parameter persistence | Corrupted persisted parameters | Invalid startup values | Unsafe threshold/config behavior | Hazardous | Integrity verify call | CRC integrity check + controlled load/save path | DV-DATA-01 | `tests/unit_tests.cpp` |

## Notes

- Residual mission risk remains and must be assessed by mission owners.
- This document is process evidence, not legal or certification approval.
