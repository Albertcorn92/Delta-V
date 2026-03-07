# Hazard Register (Baseline Record)

Date: 2026-03-07

This is the DELTA-V framework baseline hazard set for civilian, non-operational
open-source release. Mission teams must extend this with mission-specific
hazards before any operational deployment.

| Hazard ID | Description | Severity | Likelihood | Operational Context | Primary Requirement IDs | Owner | Status |
|---|---|---|---|---|---|---|---|
| HZ-001 | Replay or duplicate uplink command is accepted and re-applied, producing unintended repeated command effects. | Hazardous | Remote | Uplink command path | DV-COMM-04, DV-COMM-06 | Flight SW | Mitigated in baseline |
| HZ-002 | Malformed/non-canonical uplink frame bypasses parsing and dispatches invalid command data. | Catastrophic | Extremely Remote | Uplink parser + command bridge | DV-SEC-01, DV-SEC-03 | Flight SW | Mitigated in baseline |
| HZ-003 | Command routing failure acknowledges success when the route queue is saturated. | Major | Remote | Command hub routing | DV-FDIR-08 | Flight SW | Mitigated in baseline |
| HZ-004 | Time base approaches uint32 wrap and downstream logic assumes non-wrapping MET. | Major | Extremely Remote | Long-duration mission timing | DV-TIME-02 | Flight SW | Mitigated in baseline |
| HZ-005 | Parameter persistence corruption causes invalid startup thresholds/parameters. | Hazardous | Extremely Remote | Parameter DB persistence + restore | DV-DATA-01 | Flight SW | Mitigated in baseline |

## Maintenance Rules

- Keep IDs stable across releases.
- Update linked mitigations in `mitigations.md` when hazards change.
- Record delta impacts in `change_impact.md`.
