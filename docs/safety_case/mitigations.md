# Mitigation Register (Reference Mission Baseline)

Date: 2026-03-14

| Mitigation ID | Hazard ID | Control Description | Requirement ID | Implementation Link | Verification Method | Status |
|---|---|---|---|---|---|---|
| MT-001 | HZ-001 | Reject replayed commands and persist last accepted sequence to survive restart and recontact. | DV-COMM-04, DV-COMM-06 | `src/TelemetryBridge.hpp` | Unit tests + SITL fault campaign + qualification gate | Closed (baseline) |
| MT-002 | HZ-002 | Reject invalid sync/APID/length and reject truncated/oversized command frames before dispatch. | DV-SEC-01, DV-SEC-03 | `src/TelemetryBridge.hpp` | Unit tests + SITL fault campaign + qualification gate | Closed (baseline) |
| MT-003 | HZ-003 | Return NACK (not ACK) when route queue is full; increment error counter so operators are not given a false success. | DV-FDIR-08 | `src/CommandHub.hpp` | Unit test + stress gate | Closed (baseline) |
| MT-004 | HZ-004 | Emit one-time warning near MET wrap threshold and maintain a 64-bit MET accessor for long-uptime reasoning. | DV-TIME-02 | `src/TimeService.hpp` | Unit test + code review | Closed (baseline) |
| MT-005 | HZ-005 | Maintain CRC integrity checks and persistent parameter storage with explicit load/save behavior. | DV-DATA-01 | `src/ParamDb.hpp` | Unit tests + qualification gate | Closed (baseline) |
| MT-006 | HZ-006 | Classify payload capture as `RESTRICTED` and require local payload enable before sample capture. | DV-SEC-02, DV-FDIR-08 | `src/CommandHub.hpp`, `src/PayloadMonitorComponent.hpp` | Unit tests + SITL fault campaign | Closed (baseline) |

## Maintenance Rules

- Use DELTA-V requirement IDs where available.
- If no requirement exists, add one before closing mitigation status.
- Re-open mitigation status when code or assumptions change.
