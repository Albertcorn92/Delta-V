# DELTA-V Roadmap 2026 (Software Focus)

Date: 2026-03-08

## Q2 2026

- Ship `v4.0.0` civilian software-final baseline.
- Stabilize 10-minute onboarding flow and quickstart script. (Completed in v4.0.x)
- Publish benchmark protocol and first baseline metrics.
- Add system test target and require it in flight-readiness gate.
- Add coverage-threshold policy, CI enforcement (`coverage_guard`), and trend artifact tracking.
- Add non-UDP radio transport path (`serial_kiss`) for UART integration without rewriting TelemetryBridge.
- Ship baseline operations apps: command sequencer, file transfer manager, memory dwell service.
- Add OTA manager scaffold with CRC32 verification and staged reboot request signaling.
- Add ground-driven UTC time sync and store-and-forward playback baseline components.
- Add ATS/RTS sequencing, dynamic limit checking, CFDP-style chunk tracking, and
  mode-management orchestration in the standard civilian ops bundle.
- Add ESP32 golden-image bootchain kit (partition profile + CRC validation tooling).

## Q3 2026

- Expand integration/system tests for command/event/telemetry failure paths.
- Add deterministic SITL smoke artifacts in CI.
- Add deterministic SITL soak artifacts in CI.
- Add migration guide format for minor/major releases.

## Q4 2026

- Add multi-board software portability matrix (software config validation).
- Increase benchmark coverage (startup, RSS, CPU trend windows).
- Tighten CI quality bars for regression thresholds.
- Prepare mission-team handoff docs for civilian users. (Completed)

## Out of Scope

- Military/weaponized capability additions.
- Command-path cryptography/encryption in baseline framework.
- Claims of certification/legal clearance from software artifacts alone.
