# DELTA-V Software Classification Baseline

Date: 2026-03-14
Scope: public DELTA-V framework baseline

This document is the repository's internal planning baseline for software
classification work under a stricter external assurance model. It is not an
official program classification and does not replace mission-level
classification by project technical authority.

## External-Alignment References

This planning baseline is intended to stay readable against stricter external
frameworks such as:

- NASA `NPR 7150.2` software engineering requirements
- NASA software classification and safety-criticality guidance (`SWE-020`,
  `SWE-205`)
- comparable mission-assurance flows where software class and criticality drive
  the required level of review, verification, and independent assurance

The repository does not claim those authorities have classified DELTA-V.

## Reference Mission Assumptions

- Uncrewed civilian 3U CubeSat in LEO.
- Mission profile: routine telemetry, time-tagged commanding, housekeeping,
  payload scheduling, and safe-mode recovery.
- DELTA-V owns command gating, telemetry/event routing, watchdog/FDIR, and
  parameter integrity for the reference mission.
- Ground software is a support system, not a flight-authority substitute.

## Baseline Criticality Partition

| Item | Scope | Baseline criticality | Rationale |
|---|---|---|---|
| Flight runtime core (`RateGroupExecutive`, `TelemetryBridge`, `CommandHub`, `MissionFsm`, `WatchdogComponent`, `ParamDb`) | Flight | Mission-critical / safety-significant | Faults can cause loss of commandability, loss of telemetry integrity, or uncontrolled mission state transitions. |
| Generated topology and dictionaries (`topology.yaml`, `tools/autocoder.py`, generated headers, `dictionary.json`) | Development + flight configuration | Configuration-critical | Incorrect generation can inject latent defects into command routing, telemetry, and type wiring. |
| GDS dashboard (`gds/gds_dash.py`) | Ground support | Mission-support | Can cause operator error or loss of observability, but is not treated as the sole safety barrier. |
| Assurance/reporting scripts (`tools/*_check.py`, coverage/benchmark/readiness scripts) | Verification support | Verification-critical | Faults can produce false confidence or incomplete release evidence. |
| ESP32 port and HAL adapters | On-target support | Mission-critical when used in mission builds | Hardware interaction faults can invalidate timing, watchdog, or device I/O behavior. |

## Baseline Consequences

- Flight-core requirements are treated as high-assurance software requirements.
- Generated artifacts are controlled configuration items, not convenience files.
- Verification evidence must cover command gating, state transitions, fault
  response, data integrity, and startup configuration.
- Independent human review is required before any operational use, even though
  this public baseline approximates independence with gates and records.
- Mission-specific classification, hazard severity, and tailoring must still be
  done by the adopting project.

## Provisional Planning Posture

For external-assurance planning only, the public baseline assumes:

- the flight runtime core would be treated as safety-significant software in a
  real mission context
- configuration-generation tooling would be treated as configuration-critical
  support software
- verification/reporting tooling would require mission-owned review before its
  outputs were accepted as release evidence

These statements are intentionally planning assumptions, not official assigned
software classes.

## Explicit Non-Claims

- No official external program software class is claimed by this repository
  alone.
- No certification, flight acceptance, or mission authority approval is implied.
- Hardware qualification, environmental testing, and launch-provider acceptance
  remain outside this document.
