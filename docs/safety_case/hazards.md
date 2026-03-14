# Hazard Register (Reference Mission Baseline)

Date: 2026-03-14

This is the DELTA-V reference-mission hazard set for a civilian 3U CubeSat with
scheduled-pass operations and low-rate payload windows. It remains a software
baseline and must be extended before operational deployment.

| Hazard ID | Mission Phase | Description | Mission Consequence | Severity | Likelihood | Primary Requirement IDs | Owner | Status |
|---|---|---|---|---|---|---|---|
| HZ-001 | Command pass | Replay or duplicate uplink command is accepted and re-applied, producing unintended repeated command effects. | Repeated actuation or repeated state change during a scheduled pass. | Hazardous | Remote | DV-COMM-04, DV-COMM-06 | Flight SW | Mitigated in baseline |
| HZ-002 | Command pass | Malformed/non-canonical uplink frame bypasses parsing and dispatches invalid command data. | Undefined command effect reaches mission logic during operator contact. | Catastrophic | Extremely Remote | DV-SEC-01, DV-SEC-03 | Flight SW | Mitigated in baseline |
| HZ-003 | Command pass | Command routing failure acknowledges success when the route queue is saturated. | Operator believes a command succeeded when it did not, delaying recovery action. | Major | Remote | DV-FDIR-08 | Flight SW | Mitigated in baseline |
| HZ-004 | Long-duration mission | Time base approaches `uint32_t` wrap and downstream logic assumes non-wrapping MET. | Timing-dependent behavior and replay reasoning become misleading late in mission life. | Major | Extremely Remote | DV-TIME-02 | Flight SW | Mitigated in baseline |
| HZ-005 | Startup / recovery | Parameter persistence corruption causes invalid startup thresholds or configuration values. | Incorrect thresholds can degrade safe-mode and recovery behavior after restart. | Hazardous | Extremely Remote | DV-DATA-01 | Flight SW | Mitigated in baseline |
| HZ-006 | Payload window | Payload sample capture is accepted outside intended operations context or while payload is disabled. | Unplanned payload activity or misleading payload data during a pass. | Major | Remote | DV-SEC-02, DV-FDIR-08 | Flight SW | Mitigated in baseline |

## Maintenance Rules

- Keep IDs stable across releases.
- Update linked mitigations in `mitigations.md` when hazards change.
- Record delta impacts in `change_impact.md`.
