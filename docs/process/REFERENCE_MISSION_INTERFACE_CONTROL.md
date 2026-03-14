# Reference Mission Interface Control Supplement

Date: 2026-03-14
Scope: DELTA-V reference 3U CubeSat mission baseline

This document supplements `docs/ICD.md` with mission-shaped subsystem
interfaces for the reference mission. It is still a public baseline, not a
flight acceptance ICD.

## Interface Summary

| Interface | Software Endpoint | External Owner | Current Baseline Status |
|---|---|---|---|
| Ground segment | `TelemetryBridge`, `dictionary.json`, GDS | Ground software/operator | Defined |
| Communications transport | UDP or serial-KISS | Radio/modem integration | Defined for host/lab baseline |
| EPS / battery | `PowerComponent` (`ID 200`) | EPS hardware/integration | Defined at software command/telemetry level |
| ADCS / sensors | `SensorComponent` (`ID 100`), `ImuComponent` (`ID 300`) | Sensor/ADCS integration | Defined at software command/telemetry level |
| Payload | `PayloadMonitorComponent` (`ID 400`) | Payload team | Defined at public reference-profile level |

## 1. Ground Segment Interface

- Transport and packet structure are defined in `docs/ICD.md`.
- Default SITL ports are `9001` downlink and `9002` uplink.
- Local SITL port overrides are supported through:
  - `DELTAV_DOWNLINK_PORT`
  - `DELTAV_UPLINK_PORT`
- Host command ingest remains disabled by default unless
  `DELTAV_ENABLE_UNAUTH_UPLINK=1`.
- Accepted host uplink source defaults to loopback and can be restricted with
  `DELTAV_UPLINK_ALLOW_IP`.

Ground responsibilities:

- command preparation and approval,
- dictionary/GDS synchronization,
- pass planning and procedure ownership.

## 2. EPS Interface

| Direction | Data / Command | Contract |
|---|---|---|
| Flight -> ground | Battery telemetry from component `ID 200` | Routed through `TelemHub` and downlinked through `TelemetryBridge` |
| Ground -> flight | `RESET_BATTERY` (`target_id=200`, `opcode=1`) | Restores reference SOC in simulation/lab baseline |
| Ground -> flight | `SET_DRAIN_RATE` (`target_id=200`, `opcode=2`) | Sets simulated battery drain for rehearsal/fault campaigns |

Mission-owned closure items:

- sensor calibration,
- EPS analog accuracy,
- hard undervoltage cutoff,
- power-bus qualification.

## 3. ADCS / Sensor Interface

| Endpoint | Interface | Contract |
|---|---|---|
| `SensorComponent` (`ID 100`) | Uplink command | `SET_STAR_AMPLITUDE` (`opcode=1`) adjusts simulated star-tracker amplitude |
| `ImuComponent` (`ID 300`) | Uplink command | `IMU_RECALIBRATE` (`opcode=1`) triggers recalibration |
| Both | Telemetry | Routed through `TelemHub`; payload format follows `docs/ICD.md` telemetry contract |

Mission-owned closure items:

- sensor timing budgets,
- calibration procedures,
- alignment and pointing performance,
- on-target bus behavior.

## 4. Communications Transport Interface

Two transport profiles are supported for the same CCSDS payloads:

- UDP for host/SITL rehearsal
- serial-KISS for radio-oriented integration

The public baseline closes:

- framing,
- validation,
- replay protection,
- live malformed/replay traffic rejection in SITL.

The public baseline does not close:

- RF link budget,
- modem interoperability,
- over-the-air security controls,
- regulatory licensing or spectrum coordination.

## 5. Payload Interface

| Direction | Data / Command | Contract |
|---|---|---|
| Flight -> ground | Payload telemetry from component `ID 400` | `data_payload` carries the current reference payload sample or `0.0` when disabled |
| Ground -> flight | `PAYLOAD_SET_ENABLE` (`target_id=400`, `opcode=1`) | Enables or disables the reference payload profile |
| Ground -> flight | `PAYLOAD_CAPTURE_SAMPLE` (`target_id=400`, `opcode=2`) | Captures one bounded payload sample during nominal operations windows |
| Ground -> flight | `PAYLOAD_SET_GAIN` (`target_id=400`, `opcode=3`) | Adjusts bounded payload gain for the reference profile |

Mission-owned closure items:

- replace the reference payload profile with real instrument behavior,
- define payload timing and data-quality acceptance criteria,
- extend hazards/operating limits for the actual payload,
- qualify payload interfaces on mission hardware.

## 6. Verification References

- `docs/ICD.md`
- `docs/process/REFERENCE_MISSION_REQUIREMENTS_ALLOCATION.md`
- `docs/process/REFERENCE_PAYLOAD_PROFILE.md`
- `docs/process/OPERATIONS_REHEARSAL_20260314.md`
- `build/sitl/sitl_fault_campaign_result.json`
