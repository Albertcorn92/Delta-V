# Changelog

## v4.0.0-rc (2026-03-06)

### Added
- `flight_readiness` and `qualification_bundle` build targets.
- Qualification report generator (`tools/qualification_report.py`).
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

### Verification Snapshot
- Unit tests: 119 passing.
- Requirements traceability: 31/31 with direct test evidence.
- Blocking safety gate: passing (`tidy_safety`).
- Qualification bundle generation: passing.

