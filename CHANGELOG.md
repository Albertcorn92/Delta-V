# Changelog

## v4.0.1-rc (2026-03-08)

### Changed
- GDS runtime logs now write to `gds/runtime/` instead of the repo root.
- GDS sidebar now includes a first-time setup panel with direct boot-menu pathing.
- Developer/docs references updated for `gds/runtime/{live_telem.csv,events.log}`.
- CubeSat readiness docs now define explicit scope waivers and the `WAIVED` semantics.

### Verification Snapshot
- Unit + system tests: passing.
- V&V stress gate: passing.
- Requirements traceability: `37/37` with direct test evidence.
- Blocking safety gate (`tidy_safety`): passing.
- Qualification bundle and software final gates: passing.

## v4.0.0-rc (2026-03-06)

### Added
- `flight_readiness` and `qualification_bundle` build targets.
- `software_final` release gate target for software-only finalization.
- Qualification report generator (`tools/qualification_report.py`).
- Software finalization validator/synchronizer (`tools/software_final_check.py`).
- Requirements traceability matrix artifacts and mapping updates.
- Process templates under `docs/process/` (PSAC/SVVP/SCMP/SQAP baseline).
- Open-source release checklist.
- GitHub issue templates and PR template.
- Contributing, security, and code of conduct files.

### Changed
- Watchdog battery thresholds now support TMR-backed configuration with fail-safe fallback.
- Topology autocoder now generates mission-state source and threshold-source verification checks.
- `main.cpp` now uses topology-driven registration and exception-safe top-level handling.
- Telemetry bridge now checks `sendto` result integrity and safer header parsing.
- Logger tests now use unique file paths to avoid cross-run collisions.
- Documentation updated for `gds/gds_dash.py` path and qualification workflow.
- CI macOS runner updated to supported image and PEP 668-compatible pip invocations.
- Coverage gate now binds lcov to compiler-matching gcov tool.
- Safety gate fixed for clang-analyzer stack-address escape in `main.cpp`.

### Verification Snapshot
- Unit tests: 119 passing.
- Requirements traceability: 33/33 with direct test evidence.
- Blocking safety gate: passing (`tidy_safety`).
- Qualification bundle generation: passing.
