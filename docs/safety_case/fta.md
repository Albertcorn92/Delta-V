# Fault Tree Analysis (Reference Mission Baseline)

Date: 2026-03-14

Top-event analysis for DELTA-V reference mission software faults.

## Top Event TE-001

`Unsafe or unintended command effect reaches mission logic during a scheduled pass`

### Major Contributors

1. `TE-001-A`: Replay/duplicate command accepted.
2. `TE-001-B`: Malformed/non-canonical uplink frame accepted.
3. `TE-001-C`: Command route saturation acknowledged as success.

### Controls

- `TE-001-A` controlled by replay rejection and persisted sequence state.
  - Requirements: `DV-COMM-04`, `DV-COMM-06`
  - Evidence: `TelemetryBridge.RejectsReplaySequence`,
    `TelemetryBridge.PersistsReplaySequenceAcrossRestart`,
    `build/sitl/sitl_fault_campaign_result.json`
- `TE-001-B` controlled by strict uplink frame validation.
  - Requirements: `DV-SEC-01`, `DV-SEC-03`
  - Evidence: `TelemetryBridge.RejectsInvalidHeaderFields`,
    `TelemetryBridge.RejectsNonCanonicalFrameLength`,
    `TelemetryBridge.RejectsTruncatedFrame`,
    `build/sitl/sitl_fault_campaign_result.json`
- `TE-001-C` controlled by NACK-on-route-failure behavior.
  - Requirement: `DV-FDIR-08`
  - Evidence: `CommandHubFixture.NackWhenRouteQueueIsFull`

## Top Event TE-002

`Timing or persistence fault degrades deterministic safety behavior`

### Major Contributors

1. `TE-002-A`: MET wrap not surfaced in operations telemetry.
2. `TE-002-B`: Startup parameters restored from corrupted state.

### Controls

- `TE-002-A` controlled by wrap warning + 64-bit MET access.
  - Requirement: `DV-TIME-02`
  - Evidence: `TimeService.OverflowWarningThreshold`
- `TE-002-B` controlled by parameter integrity checks and validated persistence.
  - Requirement: `DV-DATA-01`
  - Evidence: `ParamDb.IntegrityAfterWrite`

## Notes

- This is a software baseline fault tree, not a full mission/system FTA.
- Mission teams must add hardware, environment, and operations branches.
