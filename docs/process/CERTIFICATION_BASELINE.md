# Certification Baseline (Framework Scope)

Date: 2026-03-06

This folder provides **program-level templates** to help a mission team build
DO-178C evidence around DELTA-V.

## What DELTA-V Provides

- Safety-oriented architecture and coding baseline.
- Automated quality gates (`flight_readiness`).
- Requirements traceability mapping and report generation.
- Qualification evidence bundle generation (`qualification_bundle`).
- CubeSat mission-readiness status snapshot (`cubesat_readiness`).

## What Mission Teams Must Still Provide

- Mission-specific hazards, requirements allocation, and safety case.
- Independent review and verification records per criticality level.
- On-target qualification data (timing, memory margins, HIL fault results).
- Configuration/audit records for released binaries and toolchains.

## Recommended Artifact Set Per Release

1. `qualification_report.md` and `qualification_report.json`.
2. Requirements trace matrix (`requirements_trace_matrix.md/.json`).
3. Signed review package for changed safety-critical files.
4. HIL campaign results (fault injection, soak run, restart/recovery evidence).
5. Release manifest with hashes for binaries and source commit.
6. Safety-case package (`docs/safety_case/`) with hazards, mitigations, and verification links.
7. Mission operations/environment records from `docs/process/*_TEMPLATE.md`.
