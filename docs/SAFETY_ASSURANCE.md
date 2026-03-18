# Safety Assurance

Build targets and generated records used to check the DELTA-V software
baseline.

For a first pass through the repository, start with:

- `README.md`
- `docs/QUICKSTART_10_MIN.md`
- `docs/DEVELOPER_GUIDE.md`

Then return here when you want to understand the assurance and release targets.

## Blocking Targets

These targets are part of the enforced software path.

1. `autocoder_check`
   Confirms that `topology.yaml`, `dictionary.json`, `src/Types.hpp`, and
   `src/TopologyManager.hpp` are synchronized.

2. `vnv_stress`
   Runs the GTest suite in shuffled repeated mode.

3. `tidy_safety`
   Runs the blocking static-analysis profile. Findings are treated as errors.

4. `traceability`
   Checks alignment between `src/Requirements.hpp`,
   `docs/REQUIREMENTS_TRACE.yaml`, and the mapped tests.

5. `safety_case_check`
   Confirms the hazard, mitigation, and verification-link records in
   `docs/safety_case/` stay internally consistent and reference valid
   repository requirements.

6. `flight_readiness`
   Runs generated-file checks, legal checks, unit tests, system tests, static
   analysis, traceability validation, and the final host build.

7. `qualification_bundle`
   Builds the qualification report artifacts in `build/qualification/`.

8. `software_final`
   Synchronizes the generated software evidence files in `docs/`.

9. `release_preflight`
   Generates the current release-preparation report. This records blockers but
   does not require a clean tag.

10. `release_candidate`
   Release-only gate. Requires a clean worktree and an exact git tag, then
   writes the current release pedigree and release manifest.

11. `review_bundle`
   Stages the current review package and zip from the synced docs and build
   artifacts.

## Supporting Targets

These targets are useful during development, but they are not always part of
the default closeout path.

- `quickstart_10min`: short local validation path
- `run_system_tests`: host system-test suite
- `tidy`: broader static-analysis profile
- `benchmark_baseline`: refresh benchmark artifacts
- `benchmark_guard`: threshold-based performance checks
- `benchmark_trend_guard`: performance drift checks
- `sitl_smoke`: short host runtime startup check
- `sitl_soak`: extended host runtime soak
- `sitl_fault_campaign`: malformed/replay/valid uplink fault exercise
- `sitl_soak_1h`, `sitl_soak_6h`, `sitl_soak_12h`, `sitl_soak_24h`: archived
  long host/SITL campaigns
- `sitl_long_soak_status`: generated summary of archived soak evidence
- `coverage_guard`: coverage thresholds in the GCC coverage build
- `cubesat_readiness`: consolidated mission-readiness summary

## Common Commands

```bash
cmake --build build --target quickstart_10min
cmake --build build --target autocoder_check
cmake --build build --target run_system_tests
cmake --build build --target tidy_safety
cmake --build build --target traceability
cmake --build build --target safety_case_check
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
```

## Generated Records

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

`review_bundle` writes:

- `build/review_bundle/`
- `build/review_bundle.zip`

## Related Records

Reference mission process files are in:

- `docs/process/`
- `docs/safety_case/`

Use those directories when you need the mission profile, safety case, or
release/process records behind the generated software evidence.
