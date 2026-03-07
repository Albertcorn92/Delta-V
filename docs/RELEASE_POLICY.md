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

## Required Gates Before Tagging

```bash
python3 tools/legal_compliance_check.py
cmake --build build --target run_system_tests
cmake --build build --target vnv_stress
cmake --build build --target benchmark_baseline
cmake --build build --target benchmark_guard
cmake --build build --target sitl_smoke
cmake --build build --target sitl_soak
cmake --build build_cov --target coverage_guard
python3 tools/coverage_trend.py --workspace . --build-dir build_cov
cmake --build build --target software_final
```

## Mandatory Release Artifacts

- `docs/SOFTWARE_FINAL_STATUS.md`
- `docs/qualification_report.md`
- `docs/REQUIREMENTS_TRACE_MATRIX.md`
- `docs/BENCHMARK_BASELINE.md`
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
