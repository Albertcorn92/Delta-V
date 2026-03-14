# DELTA-V Public Security Posture Baseline

Date: 2026-03-14
Scope: public DELTA-V framework baseline

This document records the security posture of the public DELTA-V repository.
It exists to make the public baseline explicit and reviewable. It is not an
operational security accreditation.

## 1. Baseline Security Scope

The public baseline includes:

- CCSDS header validation
- frame-length and parser checks
- anti-replay sequence checks
- mission-state command gating
- local-only SITL unauthenticated uplink controls through environment settings

The public baseline intentionally excludes:

- command-path encryption
- cryptographic authentication
- key management
- protected uplink operations over exposed live links

## 2. Threat Assumptions

- the repository is public and should be evaluated as public source code
- default host/SITL operation is local and controlled
- exposed operational uplinks require mission-owned security controls outside
  this repository
- mission-specific payload abuse cases and mission networks are outside the
  reference payload baseline

## 3. Current Technical Controls

- invalid CCSDS/APID/length traffic is rejected before dispatch
- replayed command sequence numbers are rejected
- accepted sequence state can be persisted through `DELTAV_REPLAY_SEQ_FILE`
- local-only SITL ingest can be restricted to `127.0.0.1`
- safety and qualification gates exercise malformed/replay traffic in
  `sitl_fault_campaign`

## 4. Public-Baseline Rules

- keep the repository civilian/scientific/educational only
- do not add weapons, targeting, or military mission logic
- do not add command-path crypto/auth to the baseline
- do not describe the public baseline as an operationally secure uplink stack
- keep deployment screening and maintainer-boundary records current

## 5. Known Limits

- the public baseline is not suitable as-is for exposed operational uplinks
- mission-specific RF security controls remain mission-owned
- ground-segment identity, approval workflow, and secret handling are outside
  the current repository baseline

## 6. Review Artifacts

- `docs/process/RISK_REGISTER_BASELINE.md`
- `docs/process/REFERENCE_MISSION_REQUIREMENTS_ALLOCATION.md`
- `docs/process/DEPLOYMENT_SCREENING_PROCEDURE.md`
- `tools/legal_compliance_check.py`
