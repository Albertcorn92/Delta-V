# Configuration Control Board Record (Framework Baseline)

Date: 2026-03-07
Release Candidate: DELTA-V civilian baseline update
Branch: `main`
Commit (HEAD): `1b7036a`

## Change Package

- Command-routing NACK correctness and test coverage.
- Uplink safety defaults and source allowlist controls for host/SITL.
- ESP replay-state and parameter persistence hardening.
- Time-service wrap/atomic robustness updates.
- Safety-case/process evidence updates for civilian release governance.

## Configuration Evidence

- Build and gate status: `docs/SOFTWARE_FINAL_STATUS.md` (PASS)
- Qualification report: `docs/qualification_report.md` (PASS)
- Requirements trace matrix: `docs/REQUIREMENTS_TRACE_MATRIX.md` (PASS)
- Legal scope check: `tools/legal_compliance_check.py` (PASS)

## Board Roles and Sign-off

| Role | Decision | Notes |
|---|---|---|
| Release Owner | APPROVED | Open-source baseline publication scope only |
| Safety Evidence Owner | APPROVED | Safety-case artifacts updated and linked |
| Legal Scope Owner | APPROVED | Civilian-only/no-weapon/no-command-crypto constraints retained |

## Boundary Conditions

- This sign-off is for framework publication readiness, not mission flight approval.
- Operational deployment requires mission-specific board records and hardware evidence.
