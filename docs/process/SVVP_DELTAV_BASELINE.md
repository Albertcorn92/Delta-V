# DELTA-V SVVP Baseline

Date: 2026-03-14
Scope: public DELTA-V framework baseline

## 1. Verification Scope

This verification baseline covers the DELTA-V software framework, generated
configuration artifacts, host/SITL runtime, and selected ESP32 reference-board
evidence. It does not cover mission hardware qualification or operational
authority approval.

## 2. Verification Methods

- Requirements-based unit tests
- Requirements-based system tests
- Static analysis (`tidy_safety`)
- Generated-file determinism and topology validation
- Stress execution (`vnv_stress`)
- Host/SITL smoke and soak runs
- Coverage measurement in the GCC coverage build
- Benchmark regression checks
- Qualification bundle generation and release-pedigree checks

## 3. Independence Baseline

- The public framework baseline uses gate separation, documented review records,
  and controlled release artifacts.
- This is not a substitute for independent human verification in an operational
  mission program.
- Mission adopters must establish real reviewer independence and approval
  authority outside the repository maintainer.

## 4. Coverage Objectives

Current minimums:

- Line coverage: `>= 70%`
- Branch coverage: `>= 50%`
- Function coverage: `>= 80%`

Near-term ramp objective:

- Line coverage: `>= 75%`
- Branch coverage: `>= 55%`
- Function coverage: `>= 85%`

High-risk decision logic should also receive targeted decision-path testing even
when formal MC/DC is not claimed.

## 5. Test Environments

- macOS and Ubuntu host builds
- Clang and GCC compile paths
- Dedicated GCC coverage build
- Host/SITL runtime with local UDP and serial-KISS options
- ESP32-S3 reference port for runtime guard, reboot campaign, and soak evidence

## 6. Entry Criteria

- Generated files are current.
- Requirements trace files parse without errors.
- Legal scope documents are present and current.
- Process baseline documents are present.

## 7. Exit Criteria

- All blocking gates pass.
- Requirements traceability remains complete.
- Qualification bundle artifacts are generated.
- `software_final` passes.
- Public releases additionally require `release_candidate` on a clean tagged baseline.

## 8. Verification Data Items

- `requirements_trace_matrix.md/.json`
- `qualification_report.md/.json`
- `SOFTWARE_FINAL_STATUS.md`
- benchmark and coverage reports
- SITL smoke/soak outputs
- release manifest and release pedigree outputs for tagged releases
