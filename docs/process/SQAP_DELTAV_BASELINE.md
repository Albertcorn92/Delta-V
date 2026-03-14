# DELTA-V SQAP Baseline

Date: 2026-03-14
Scope: public DELTA-V framework baseline

## 1. Quality Objectives

- Maintain deterministic runtime behavior in the flight-core architecture.
- Keep requirements traceability complete for the published software baseline.
- Prevent stale generated artifacts, silent routing drift, and undocumented
  release changes.
- Preserve civilian-only scope and public-source publication posture.

## 2. Process Assurance

- Legal scope checks are run as a blocking gate.
- CI must pass build, test, generated-file, portability, and safety gates.
- Safety-relevant code changes require direct test evidence and updated
  documentation.
- Public releases require a clean tagged baseline through `release_candidate`.

## 3. Product Assurance

Required gates for the baseline:

- `autocoder_check`
- `flight_readiness`
- `qualification_bundle`
- `software_final`
- `coverage_guard` in the GCC coverage build
- `release_candidate` for public release publication

Advisory but expected supporting evidence:

- `benchmark_guard`
- `benchmark_trend_guard`
- `sitl_smoke`
- `sitl_soak`
- `cubesat_readiness`

## 4. Non-Conformance Handling

- Any failing blocking gate is a release blocker.
- Traceability mismatches, generated-file drift, and stale legal/process records
  are treated as non-conformances.
- Release-candidate failure on tag or pedigree is a publication blocker.

## 5. Records

The repository keeps quality records in:

- CI logs and uploaded artifacts
- `docs/qualification_report.*`
- `docs/SOFTWARE_FINAL_STATUS.md`
- `docs/process/`
- `docs/safety_case/`

The public repository is the baseline record store for this framework release
profile. Mission adopters may require additional retention or signed records.
