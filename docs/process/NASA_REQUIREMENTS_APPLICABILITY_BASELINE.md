# DELTA-V NASA-Style Requirements Applicability Baseline

Date: 2026-03-14
Scope: internal planning baseline for public DELTA-V review

This matrix records how the public DELTA-V baseline maps to selected NASA-style
software and assurance expectations. It is an internal applicability record for
review and gap analysis. It is not an official NASA tailoring decision and it
does not claim compliance approval.

## Applicability Summary

| Topic | Repository Position | Status | Primary Evidence |
|---|---|---|---|
| Software classification | Internal baseline classification exists; adopting mission must still classify under its own authority. | PARTIAL | `docs/process/SOFTWARE_CLASSIFICATION_BASELINE.md` |
| Software plans | Project planning baselines exist for certification, config management, quality, verification, and safety. | PARTIAL | `docs/process/PSAC_DELTAV_BASELINE.md`, `SCMP_DELTAV_BASELINE.md`, `SQAP_DELTAV_BASELINE.md`, `SVVP_DELTAV_BASELINE.md`, `SOFTWARE_SAFETY_PLAN_BASELINE.md` |
| Requirements traceability | Repository requirements are traced to direct verification evidence. | MET FOR PUBLIC BASELINE | `docs/REQUIREMENTS_TRACE_MATRIX.md` |
| Verification and validation | Unit, system, stress, static-analysis, benchmark, and SITL evidence are part of the closeout path. | MET FOR PUBLIC BASELINE | `docs/qualification_report.md`, `docs/SOFTWARE_FINAL_STATUS.md` |
| Configuration management | Controlled release pedigree exists for tagged public releases; current workspace evidence may still be untagged/dirty until release ceremony. | PARTIAL | `docs/process/SCMP_DELTAV_BASELINE.md`, `docs/process/CONFIGURATION_AUDIT_BASELINE.md`, `tools/release_pedigree.py` |
| Software assurance independence | Process records exist, but independent human assurance authority is mission-owned and not closed by the public baseline. | OPEN / MISSION-OWNED | `docs/process/INDEPENDENT_REVIEW_RECORD_20260307.md` |
| Software safety planning | A baseline safety plan and reference-mission safety case exist. | PARTIAL | `docs/process/SOFTWARE_SAFETY_PLAN_BASELINE.md`, `docs/safety_case/` |
| Mission hardware qualification | Explicitly outside repository scope and must be closed on mission hardware. | OPEN / MISSION-OWNED | `docs/process/FLIGHT_ENV_TEST_MATRIX_20260307.md` |
| Operational security controls | Public baseline intentionally excludes protected uplink crypto/auth and keeps deployment security mission-owned. | OPEN / MISSION-OWNED | `docs/process/PUBLIC_SECURITY_POSTURE_BASELINE.md` |
| Operations readiness | Reference-mission rehearsal exists in simulation scope only. | PARTIAL | `docs/process/OPERATIONS_REHEARSAL_20260314.md`, `docs/process/OPERATIONS_READINESS_20260307.md` |

## What This Matrix Is For

- make the public repository reviewable against a stricter external benchmark
- separate software/process closure from mission-flight closure
- keep open mission-owned items explicit instead of implied

## What This Matrix Does Not Do

- it does not assign an official NASA software class
- it does not tailor requirements on behalf of a real program authority
- it does not convert public repository evidence into flight approval
