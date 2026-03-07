# Mitigation Register (Baseline Record)

Date: 2026-03-07

| Mitigation ID | Hazard ID | Control Description | Requirement ID | Implementation Link | Verification Method | Status |
|---|---|---|---|---|---|---|
| MT-001 | HZ-001 | Reject replayed commands and persist last accepted sequence to survive restart. | DV-COMM-04, DV-COMM-06 | `src/TelemetryBridge.hpp` | Unit tests + qualification gate | Closed (baseline) |
| MT-002 | HZ-002 | Reject invalid sync/APID/length and reject truncated/oversized command frames. | DV-SEC-01, DV-SEC-03 | `src/TelemetryBridge.hpp` | Unit tests + qualification gate | Closed (baseline) |
| MT-003 | HZ-003 | Return NACK (not ACK) when route queue is full; increment error counter. | DV-FDIR-08 | `src/CommandHub.hpp` | Unit test + stress gate | Closed (baseline) |
| MT-004 | HZ-004 | Emit one-time warning near MET wrap threshold; maintain 64-bit MET accessor for long uptime tracking. | DV-TIME-02 | `src/TimeService.hpp` | Unit test + code review | Closed (baseline) |
| MT-005 | HZ-005 | Maintain CRC integrity checks and persistent parameter storage with explicit load/save behavior. | DV-DATA-01 | `src/ParamDb.hpp` | Unit tests + qualification gate | Closed (baseline) |

## Maintenance Rules

- Use DELTA-V requirement IDs where available.
- If no requirement exists, add one before closing mitigation status.
- Re-open mitigation status when code or assumptions change.
