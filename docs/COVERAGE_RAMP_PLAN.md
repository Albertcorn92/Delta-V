# Coverage Ramp Plan

Date: 2026-03-17

This plan defines controlled threshold increases for software-only coverage
gates.

## Stage B (Active)

- Line: `>= 75%`
- Branch: `>= 52%`
- Function: `>= 85%`

## Stage C (Planned)

- Line: `>= 80%`
- Branch: `>= 55%`
- Function: `>= 90%`

## Promotion Criteria

Move from Stage B to Stage C only after all criteria are met:

1. Coverage gate passes for at least 10 consecutive CI runs on `main`.
2. Coverage trend artifacts show no single-run drop greater than:
   - 2.0 points (line),
   - 2.0 points (function),
   - 3.0 points (branch).
3. No active investigation for test-flake or coverage-tool instability.

## 2026-03-17 Raise Basis

- Expanded failure-path unit tests increased exercised branches in command,
  telemetry, playback, time-sync, CFDP, and mode-management paths.
- Latest local GCC coverage run reported:
  - Line: `80.09%`
  - Branch: `54.84%`
  - Function: `92.95%`
- The active gate was raised conservatively to preserve headroom while keeping
  Stage C promotion tied to branch stability evidence.

## Evidence Artifact

Each CI coverage job produces:

- `build_cov/coverage_trend.json`

Archive retention target: 30 days minimum.
