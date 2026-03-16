# Documentation Guide

This directory contains two kinds of documentation:

- user and developer guides for working in the repository
- generated or controlled records used for release, assurance, and review

If this is the first time in the repo, start with the first section and ignore
the deeper process records until you need them.

## Start Here

Read these first:

1. `README.md`
2. `docs/QUICKSTART_10_MIN.md`
3. `examples/cubesat_attitude_control/README.md`
4. `docs/ARCHITECTURE.md`
5. `docs/COMPONENT_MODEL.md`
6. `docs/PORTS_AND_MESSAGES.md`
7. `docs/SCHEDULER_AND_EXECUTION.md`
8. `docs/ICD.md`

## First-Time User Path

Use this order for the first successful local run:

1. `README.md`: repository overview and basic commands
2. `docs/QUICKSTART_10_MIN.md`: first validation run
3. `examples/cubesat_attitude_control/README.md`: one concrete mission-shaped entry point
4. `docs/ARCHITECTURE.md`: runtime structure and generated topology flow
5. `docs/COMPONENT_MODEL.md`: passive vs active runtime blocks
6. `docs/PORTS_AND_MESSAGES.md`: packet and queue model
7. `docs/SCHEDULER_AND_EXECUTION.md`: rate groups and execution ownership

## Daily Development

- `docs/DEVELOPER_GUIDE.md`: setup, component work, topology updates, and build targets
- `docs/ICD.md`: packet layout, command IDs, and interface rules
- `docs/COMPONENT_MODEL.md`: how components expose inputs, outputs, and health
- `docs/PORTS_AND_MESSAGES.md`: transport semantics and terminology
- `docs/SCHEDULER_AND_EXECUTION.md`: scheduling and active-component behavior
- `docs/SAFETY_ASSURANCE.md`: enforced gates and generated outputs
- `docs/COVERAGE_POLICY.md`: coverage build and thresholds
- `docs/SOFTWARE_PORTABILITY_MATRIX.md`: host portability results
- `docs/MIGRATION_GUIDE.md`: release migration notes template

## Runtime And System Understanding

- `docs/ARCHITECTURE.md`: runtime model and major components
- `docs/COMPONENT_MODEL.md`: component responsibilities and health behavior
- `docs/PORTS_AND_MESSAGES.md`: command, telemetry, and event transport model
- `docs/SCHEDULER_AND_EXECUTION.md`: rate groups and execution strategy
- `examples/cubesat_attitude_control/README.md`: concrete example mission path
- `docs/REFERENCE_MISSION_WALKTHROUGH.md`: one end-to-end path through the repo
- `docs/ASSESS_DELTA_V.md`: review-oriented reading order

## Generated Status And Evidence

These files are updated by build targets. Treat them as outputs, not hand-written
guides.

- `docs/SOFTWARE_FINAL_STATUS.md`
- `docs/qualification_report.md`
- `docs/REQUIREMENTS_TRACE_MATRIX.md`
- `docs/CUBESAT_READINESS_STATUS.md`
- `docs/CUBESAT_READINESS_STATUS_SCOPE.md`
- `docs/process/RELEASE_PREFLIGHT_CURRENT.md`

## Release And Process Records

Use these when preparing a release, reviewing the framework, or tracing one
reference mission through the process package.

- `docs/process/`: planning, safety, release, and audit records
- `docs/safety_case/`: hazards, mitigations, FMEA, FTA, and change impact
- `build/review_bundle/` or `build/review_bundle.zip`: curated review package

Key records:

- `docs/process/REFERENCE_MISSION_PROFILE.md`
- `docs/process/REFERENCE_MISSION_REQUIREMENTS_ALLOCATION.md`
- `docs/process/REFERENCE_MISSION_INTERFACE_CONTROL.md`
- `docs/process/RISK_REGISTER_BASELINE.md`
- `docs/process/CONFIGURATION_AUDIT_BASELINE.md`
- `docs/process/SOFTWARE_SAFETY_PLAN_BASELINE.md`

## ESP32 And Bring-Up

- `docs/ESP32_BRINGUP.md`
- `docs/ESP32_SENSORLESS_BASELINE.md`
- `docs/ESP32_GOLDEN_IMAGE_BOOTCHAIN.md`
- `docs/CUBESAT_TEAM_READINESS.md`
- `docs/TEAM_READY_MASTER_CHECKLIST.md`
- `docs/process/OPERATIONS_RUNBOOK.md`

## Legal And Scope

- `docs/CIVILIAN_USE_POLICY.md`
- `docs/EXPORT_CONTROL_NOTE.md`
- `docs/LEGAL_FAQ.md`
- `docs/LEGAL_SCOPE_CHECKLIST.md`
- `docs/MAINTAINER_BOUNDARY_POLICY.md`

## Minimal Set

For a normal local clone, this is enough:

1. `README.md`
2. `docs/QUICKSTART_10_MIN.md`
3. `examples/cubesat_attitude_control/README.md`
4. `docs/ARCHITECTURE.md`
5. `docs/COMPONENT_MODEL.md`
6. `docs/PORTS_AND_MESSAGES.md`
7. `docs/SCHEDULER_AND_EXECUTION.md`
8. `docs/ICD.md`
9. `docs/CIVILIAN_USE_POLICY.md`
