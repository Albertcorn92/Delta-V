# DELTA-V Public Security Posture Baseline

Date: 2026-03-14
Scope: public DELTA-V framework baseline

This record defines the security posture of the public DELTA-V repository. It
describes what the public baseline does, what it does not do, and where the
boundary moves to the adopting mission.

## Included In The Public Baseline

- CCSDS header validation
- frame-length and parser checks
- anti-replay sequence checks
- mission-state command gating
- local-only SITL uplink controls through environment settings

## Not Included In The Public Baseline

- command-path encryption
- cryptographic authentication
- key management
- protected uplink operations over exposed live links

## Assumptions

- the repository is public source code
- the default host/SITL setup is local and controlled
- any exposed operational uplink requires mission-owned security controls
- mission-specific payload abuse cases and network controls are outside the
  reference payload baseline

## Current Technical Controls

- invalid CCSDS/APID/length traffic is rejected before dispatch
- replayed command sequence numbers are rejected
- accepted sequence state can be persisted through `DELTAV_REPLAY_SEQ_FILE`
- local-only SITL ingest can be restricted to `127.0.0.1`
- malformed and replay traffic is exercised in `sitl_fault_campaign`

## Rules For The Public Repo

- keep the repository civilian, scientific, and educational in scope
- do not add weapons, targeting, or military mission logic
- do not add command-path crypto/auth to the public baseline
- do not describe the repo as a complete operational uplink security stack
- keep deployment screening and maintainer-boundary records current

## Known Limits

- the public baseline is not enough for exposed operational uplinks
- mission-specific RF security controls remain mission-owned
- ground identity, approval workflow, and secret handling are outside the
  current public baseline

## Related Records

- `docs/process/RISK_REGISTER_BASELINE.md`
- `docs/process/REFERENCE_MISSION_REQUIREMENTS_ALLOCATION.md`
- `docs/process/DEPLOYMENT_SCREENING_PROCEDURE.md`
- `tools/legal_compliance_check.py`
