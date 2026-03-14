# DELTA-V PSAC Baseline

Date: 2026-03-14
Scope: public DELTA-V framework baseline

This document is the repository's working Plan for Software Aspects of
Certification baseline. It defines the software planning posture used for the
public framework. Mission programs may reuse it only after tailoring it to
their own authority, hardware, and hazard classifications.

## 1. Program Identification

- Mission: DELTA-V reference mission baseline
- Vehicle: civilian 3U CubeSat reference profile
- Software item: DELTA-V flight software framework and support tooling
- Baseline version: repository mainline with semantic-version release tags

## 2. Applicable Standards and References

- C++20 project baseline
- CCSDS packet framing as implemented in `TelemetryBridge`
- NASA-style software lifecycle planning reference:
  `NPR 7150.2`, `NASA-STD-8739.8`, and the NASA Systems Engineering Handbook
- Internal project constraints:
  civilian-only scope, no command-path encryption, public-source publication

The repository does not claim compliance or certification by reference alone.

## 3. Software Items in Scope

- Flight runtime and generated topology headers in `src/`
- GDS support application in `gds/`
- Topology and generation tooling in `topology.yaml` and `tools/autocoder.py`
- Assurance and release-report tooling in `tools/`
- ESP32 reference port in `ports/esp32/`

## 4. Lifecycle Model

- Development model: Git-based incremental development with CI-enforced gates
- Verification model: requirements-based tests, system tests, static analysis,
  coverage, benchmark guards, SITL smoke/soak, and selected on-target evidence
- Configuration management: semantic tags, generated-file controls, release
  manifests, and configuration-control records in `docs/process/`
- Quality assurance: blocking gates via `flight_readiness`, `software_final`,
  and the release-candidate pedigree gate

## 5. Criticality and Planning Basis

- Flight-core runtime paths are treated as mission-critical / safety-significant
  software in the public baseline.
- Ground support and reporting tools are support software with controlled review
  and documented tool limitations.
- Mission adopters must reclassify software for the actual mission and authority
  chain in use.

## 6. Verification Strategy

- Requirements-based unit and system tests mapped through the trace matrix
- Static analysis with blocking `tidy_safety`
- Stress and soak execution in host/SITL
- Coverage thresholds in the GCC coverage build
- Generated-file determinism and freshness checks
- Qualification bundle generation with artifact hashes
- Release pedigree generation for tagged public releases

## 7. Tool Strategy

- Build and verification tools are listed in
  `docs/process/TOOL_GOVERNANCE_BASELINE.md`.
- The repository does not claim formal tool qualification.
- Mission teams must either qualify mission-relevant tools or record accepted
  deviations and manual controls.

## 8. Problem Reporting and Corrective Action

- Defects are recorded through repository issues and change history.
- Safety-relevant changes require updated tests, traceability, and process
  records before release.
- Blocking gate failures stop the release baseline until corrected.

## 9. Certification and Authority Boundary

- This repository has no standing certification liaison or delegated mission
  authority.
- Public releases are engineering baselines only.
- Mission operators own final classification, authority coordination,
  environment qualification, and operational approval.
