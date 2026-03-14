# DELTA-V SCMP Baseline

Date: 2026-03-14
Scope: public DELTA-V framework baseline

## 1. Configuration Identification

- Repository: `DELTA-V Framework`
- Controlled source items:
  `src/`, `tests/`, `tools/`, `gds/`, `ports/esp32/`, `topology.yaml`,
  generated headers, and assurance documentation in `docs/`
- Controlled generated items:
  `src/TopologyManager.hpp`, `src/Types.hpp`, `dictionary.json`
- Controlled release evidence:
  requirements trace matrix, qualification bundle, software-final status, and
  release pedigree/manifest artifacts

## 2. Branch and Tag Policy

- `main`: release baseline branch
- `develop`: integration branch when used
- short-lived feature branches: development only
- release tags: `vMAJOR.MINOR.PATCH`

Public release pedigree is tied to an exact tag on a clean worktree.

## 3. Change Control

- All baseline changes must be committed to git.
- Safety-relevant changes must include updated verification evidence and change
  impact notes in the pull request or review record.
- Generated-file changes are not accepted without matching `topology.yaml`
  source changes or a documented generation reason.
- Emergency fixes may bypass normal cadence, but not the required release gates.

## 4. Baseline Definition

A DELTA-V public release baseline consists of:

- an exact git tag,
- a clean worktree,
- passing `software_final`,
- passing `release_candidate`,
- synchronized release artifacts in `docs/` and `docs/process/`.

## 5. Reproducibility Controls

- `topology.yaml` is the source of truth for generated runtime wiring.
- `autocoder_check` and CI determinism checks enforce generated-file freshness.
- qualification and release artifacts include artifact hashes and toolchain
  provenance.
- release manifests are generated from build outputs, not handwritten after the fact.

## 6. Status Accounting

Required status artifacts for each public release:

- `docs/SOFTWARE_FINAL_STATUS.md`
- `docs/qualification_report.md`
- `docs/REQUIREMENTS_TRACE_MATRIX.md`
- `docs/process/RELEASE_MANIFEST_CURRENT.md`
- `docs/process/RELEASE_PEDIGREE_CURRENT.md`

## 7. Audits

- Functional configuration audit:
  verify requirements traceability, gate results, and safety-case linkage.
- Physical configuration audit:
  verify exact source commit, release tag, generated artifacts, toolchain
  provenance, and artifact hashes.

Mission teams may add stricter records, signatures, or archival controls.
