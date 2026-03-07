# Coverage Ramp Plan

Date: 2026-03-07

This plan defines controlled threshold increases for software-only coverage
gates.

## Stage A (Active)

- Line: `>= 65%`
- Branch: `>= 45%`
- Function: `>= 75%`

## Stage B (Planned)

- Line: `>= 70%`
- Branch: `>= 50%`
- Function: `>= 80%`

## Promotion Criteria

Move from Stage A to Stage B only after all criteria are met:

1. Coverage gate passes for at least 10 consecutive CI runs on `main`.
2. Coverage trend artifacts show no single-run drop greater than:
   - 2.0 points (line),
   - 2.0 points (function),
   - 3.0 points (branch).
3. No active investigation for test-flake or coverage-tool instability.

## Evidence Artifact

Each CI coverage job produces:

- `build_cov/coverage_trend.json`

Archive retention target: 30 days minimum.
