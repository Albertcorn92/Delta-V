# Safety Assurance Gates

Date: 2026-03-06

## Blocking Gates

These gates are part of the enforced software path:

1. `autocoder_check`
   Verifies that `topology.yaml`, `dictionary.json`, `src/Types.hpp`, and `src/TopologyManager.hpp` are in sync.

2. `vnv_stress`
   Runs the GTest suite in shuffled repeated mode (`--gtest_shuffle --gtest_repeat=25` by default).

3. `flight_readiness`
   Runs generated-file checks, legal checks, unit tests, system tests, blocking static analysis, traceability validation, and the final host build.

4. `tidy_safety`
   Runs curated `clang-analyzer`, `bugprone`, and `cert` checks. Findings are treated as errors.

5. `traceability`
   Validates coverage between `src/Requirements.hpp`, `docs/REQUIREMENTS_TRACE.yaml`, and the mapped tests.

6. `software_final`
   Depends on `qualification_bundle`. It synchronizes the generated software evidence files in `docs/`.

7. `release_preflight`
   Release-prep report. Uses the current qualification/software evidence, records release blockers, and writes the current preflight status without requiring a clean tag.

8. `release_candidate`
   Release-only gate. Requires a clean worktree and an exact git tag, then writes the current release pedigree and release manifest.

9. `review_bundle`
   Review-only package. Depends on `release_preflight`, then stages a curated reviewer bundle and zip from the synced docs and current build artifacts.

## Advisory Gates

- `tidy`: broader static analysis profile
- `benchmark_baseline`: refreshes host/SITL benchmark artifacts
- `benchmark_guard`: threshold-based performance regression checks
- `benchmark_trend_guard`: sustained performance drift checks
- `sitl_smoke`: short startup and fatal-signature check
- `sitl_soak`: extended host runtime soak
- `sitl_fault_campaign`: live malformed/replay/valid UDP uplink campaign
- `sitl_fault_campaign` also checks event-log evidence for unknown-target NACKs,
  invalid replay-state recovery, and reference payload command handling
- `sitl_soak_1h`, `sitl_soak_6h`, `sitl_soak_12h`, `sitl_soak_24h`: archived long-duration host/SITL campaigns
- `sitl_long_soak_status`: generated summary of archived long-duration host/SITL evidence
- `coverage_guard`: line, branch, and function coverage thresholds in the GCC coverage build
- `quickstart_10min`: local validation shortcut
- `cubesat_readiness`: consolidated readiness report
- `release_preflight`: current release blocker report
- `release_candidate`: tagged public-release pedigree gate
- `review_bundle`: curated reviewer package for technical evaluation

## Commands

```bash
cmake --build build --target autocoder_check
cmake --build build --target run_system_tests
cmake --build build --target tidy_safety
cmake --build build --target traceability
cmake --build build --target vnv_stress
cmake --build build --target flight_readiness
cmake --build build --target qualification_bundle
cmake --build build --target software_final
cmake --build build --target release_preflight
cmake --build build --target release_candidate
cmake --build build --target review_bundle
cmake --build build --target cubesat_readiness
cmake --build build --target benchmark_baseline
cmake --build build --target benchmark_guard
cmake --build build --target benchmark_trend_guard
cmake --build build --target sitl_smoke
cmake --build build --target sitl_soak
cmake --build build --target sitl_fault_campaign
cmake --build build --target sitl_soak_1h
cmake --build build --target sitl_long_soak_status
cmake --build build_cov --target coverage_guard
cmake --build build --target quickstart_10min
```

## Generated Outputs

`qualification_bundle` writes:

- `build/qualification/qualification_report.md`
- `build/qualification/qualification_report.json`

`software_final` updates:

- `docs/REQUIREMENTS_TRACE_MATRIX.md`
- `docs/REQUIREMENTS_TRACE_MATRIX.json`
- `docs/qualification_report.md`
- `docs/qualification_report.json`
- `docs/SOFTWARE_FINAL_STATUS.md`
- `docs/process/SITL_LONG_SOAK_STATUS.md`
- `docs/process/SITL_LONG_SOAK_STATUS.json`

`release_preflight` updates:

- `docs/process/RELEASE_PREFLIGHT_CURRENT.md`
- `docs/process/RELEASE_PREFLIGHT_CURRENT.json`

`release_candidate` updates:

- `docs/process/RELEASE_PEDIGREE_CURRENT.md`
- `docs/process/RELEASE_PEDIGREE_CURRENT.json`
- `docs/process/RELEASE_MANIFEST_CURRENT.md`
- `docs/process/RELEASE_MANIFEST_CURRENT.json`

`review_bundle` updates:

- `build/review_bundle/`
- `build/review_bundle.zip`

Reference-mission safety-case and process baseline files are in `docs/safety_case/`
and `docs/process/`.
