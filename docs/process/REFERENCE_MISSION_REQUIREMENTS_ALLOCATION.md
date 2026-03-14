# Reference Mission Requirements Allocation

Date: 2026-03-14
Scope: DELTA-V reference 3U CubeSat mission baseline

This document allocates the DELTA-V software requirements to the reference
mission introduced in
`docs/process/REFERENCE_MISSION_PROFILE.md`. It is not a flight-program
approval artifact. It gives the repository one concrete mission-shaped
allocation baseline instead of leaving the process package completely generic.

## Applicability

| Category | Applicability | Notes |
|---|---|---|
| `DV-ARCH-01` through `DV-ARCH-03` | Applicable as-is | Deterministic execution model is part of the public baseline. |
| `DV-MEM-01` through `DV-MEM-03` | Applicable as-is | Memory-safety controls are baseline software constraints. |
| `DV-FDIR-01` through `DV-FDIR-09` | Applicable with mission thresholds | Battery thresholds and restart policy must be reviewed against the adopting vehicle. |
| `DV-COMM-01` through `DV-COMM-06` | Applicable as-is for local/public baseline | Live mission communications security remains mission-owned. |
| `DV-TIME-01` and `DV-TIME-02` | Applicable as-is | Timing model is baseline; target timing margins still need hardware evidence. |
| `DV-SEC-01` through `DV-SEC-03` | Applicable as-is | Validation and command gating are required for the reference mission. |
| `DV-DATA-01` through `DV-DATA-03` | Applicable as-is | Parameter and TMR integrity are baseline controls. |
| `DV-LOG-01` and `DV-LOG-02` | Applicable as-is | Logging remains part of the public baseline. |
| `DV-TMR-01` and `DV-TMR-02` | Applicable as-is | TMR behavior is baseline software functionality. |
| `DV-OPS-01` through `DV-OPS-04` | Applicable with mission-specific authorization | OTA, dwell/patch, playback, and time sync require mission-owner enablement. |

## Allocation Matrix

| Mission Function | Software Allocation | External Allocation | Evidence / Closure Basis |
|---|---|---|---|
| Deterministic scheduler execution | `RateGroupExecutive`, `Scheduler`, `Component`, `Os::Thread`; `DV-ARCH-01`, `DV-ARCH-02`, `DV-TIME-01` | Target board, RTOS/POSIX layer, and clock source must preserve timing assumptions | `docs/qualification_report.md`, `docs/BENCHMARK_BASELINE.md` |
| Flight state isolation and fail-safe behavior | `MissionFsm`, `WatchdogComponent`, `PowerComponent`; `DV-FDIR-01` through `DV-FDIR-09` | EPS hardware limits, battery sensing accuracy, and vehicle recovery authority remain mission-owned | `docs/qualification_report.md`, `docs/safety_case/`, `docs/process/OPERATIONS_REHEARSAL_20260314.md` |
| Uplink command validation and dispatch | `TelemetryBridge`, `CommandHub`, `MissionFsm`; `DV-COMM-03` through `DV-COMM-06`, `DV-SEC-01` through `DV-SEC-03` | Ground procedures own command approval, release discipline, and operator authorization | `build/sitl/sitl_fault_campaign_result.json`, `docs/qualification_report.md` |
| Telemetry and event downlink | `TelemetryBridge`, `TelemHub`, `EventHub`, `LoggerComponent`; `DV-COMM-01`, `DV-COMM-02`, `DV-LOG-01`, `DV-LOG-02` | Radio hardware, RF link budget, and ground ingest/display remain mission-owned | `docs/ICD.md`, `docs/process/REFERENCE_MISSION_INTERFACE_CONTROL.md` |
| Parameter integrity and startup configuration | `ParamDb`, `TmrStore`, `TmrRegistry`; `DV-DATA-01` through `DV-DATA-03`, `DV-TMR-01`, `DV-TMR-02` | Mission configuration control board owns approved parameter sets and on-vehicle loads | `docs/qualification_report.md`, `docs/process/CONFIGURATION_AUDIT_BASELINE.md` |
| Mission operations services | `CommandSequencerComponent`, `FileTransferComponent`, `MemoryDwellComponent`, `TimeSyncComponent`, `PlaybackComponent`, `OtaComponent`; `DV-OPS-01` through `DV-OPS-04` | Ground segment owns enabling criteria, pass planning, and procedural approvals | `docs/process/OPERATIONS_RUNBOOK.md`, `docs/process/OPERATIONS_REHEARSAL_20260314.md` |
| Attitude/sensor interface | `SensorComponent` and `ImuComponent`; telemetry routed through `TelemHub` | Calibration, sensor acceptance, and flight pointing performance remain mission-owned | `docs/process/REFERENCE_MISSION_INTERFACE_CONTROL.md` |
| Payload interface | `PayloadMonitorComponent`; `PAYLOAD_SET_ENABLE`, `PAYLOAD_CAPTURE_SAMPLE`, `PAYLOAD_SET_GAIN` | Actual payload hardware, release constraints, and instrument qualification remain mission-owned | `docs/process/REFERENCE_PAYLOAD_PROFILE.md`, `docs/process/REFERENCE_MISSION_INTERFACE_CONTROL.md`, `tests/unit_tests.cpp` |

## External Responsibilities That Stay Open

- RF security controls, encryption, and key management are outside the public
  DELTA-V baseline.
- Hardware qualification, TVAC, vibration, EMI/EMC, and radiation evidence are
  outside this repository.
- Independent verification authority and release approval remain mission-owned.
- Payload-specific command authorization, data products, and flight
  qualification remain mission-owned beyond the reference profile.
