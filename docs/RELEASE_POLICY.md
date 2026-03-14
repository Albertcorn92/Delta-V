# Release Policy

Date: 2026-03-06

## Versioning

DELTA-V uses semantic versioning:

- `MAJOR`: breaking API/behavior changes
- `MINOR`: backward-compatible features
- `PATCH`: bug fixes and assurance/documentation updates

## Release Cadence

- Patch releases: as needed for blocking defects.
- Minor releases: target monthly cadence.
- Major releases: only when migration notes are ready.

## Required Steps For A Public Release

```bash
python3 tools/legal_compliance_check.py
cmake --build build --target autocoder_check
cmake --build build --target run_system_tests
cmake --build build --target vnv_stress
cmake --build build --target benchmark_baseline
cmake --build build --target benchmark_guard
cmake --build build --target sitl_smoke
cmake --build build --target sitl_soak
cmake --build build_cov --target coverage_guard
python3 tools/coverage_trend.py --workspace . --build-dir build_cov
cmake --build build --target software_final
cmake --build build --target release_preflight
```

`release_preflight` updates `docs/process/RELEASE_PREFLIGHT_CURRENT.md` and
lists the current blockers for a clean tagged release. Use it before tagging.

After the candidate commit is ready, create an annotated semantic-version tag
on that exact clean commit and run:

```bash
git tag -a vX.Y.Z -m "DELTA-V vX.Y.Z"
cmake --build build --target release_candidate
```

`release_candidate` fails unless the worktree is clean and the current commit
has an exact tag.

## Mandatory Release Artifacts

- `docs/SOFTWARE_FINAL_STATUS.md`
- `docs/qualification_report.md`
- `docs/REQUIREMENTS_TRACE_MATRIX.md`
- `docs/BENCHMARK_BASELINE.md`
- `docs/process/RELEASE_PREFLIGHT_CURRENT.md`
- `docs/process/RELEASE_MANIFEST_CURRENT.md`
- `docs/process/RELEASE_PEDIGREE_CURRENT.md`
- `docs/safety_case/` package (hazards + mitigations + verification links)

## Changelog Discipline

- Every release must have a changelog section with:
  - Added
  - Changed
  - Fixed
  - Verification snapshot

## Scope Guardrails

- Civilian/non-weaponized scope only.
- No command-path cryptography/encryption additions in baseline.
- No blanket legal/exemption claims in release notes.
