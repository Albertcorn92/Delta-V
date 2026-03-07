# Mitigation Register (Template)

Date: 2026-03-07

| Mitigation ID | Hazard ID | Control Description | Requirement ID | Implementation Link | Verification Method | Status |
|---|---|---|---|---|---|---|
| MT-001 | HZ-001 | Example: MissionFsm gate blocks restricted commands in SAFE_MODE/EMERGENCY | DV-SEC-02 | `src/CommandHub.hpp` + `src/MissionFsm.hpp` | Unit/System test + code review | Open |

## Guidance

- Use DELTA-V requirement IDs where available.
- If no requirement exists, add one before closing mitigation status.
