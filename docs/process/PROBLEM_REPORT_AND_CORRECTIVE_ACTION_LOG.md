# Problem Report And Corrective Action Log

Date: 2026-03-14
Scope: DELTA-V framework baseline

This log records baseline process defects, their corrective actions, and the
verification evidence used to close them. It is the repository-level problem
report and corrective-action record for the public reference baseline.

## Workflow

States: `OPEN` -> `ACTIONED` -> `VERIFIED` -> `CLOSED`

Each entry must include:

- the defect or process gap,
- the affected area,
- the corrective action,
- the verification evidence,
- and the closure status.

## Current Log

| PR/CA ID | Problem Statement | Affected Area | Corrective Action | Verification Evidence | Status |
|---|---|---|---|---|---|
| PRCA-001 | Generated artifacts could drift from `topology.yaml` without failing the normal build path. | Build/release integrity | Added `autocoder_check`, stale-generated-file enforcement, and CI validation. | `cmake --build build --target autocoder_check`, `cmake --build build --target software_final` | CLOSED |
| PRCA-002 | Blocking static analysis covered too little of the header-heavy runtime code. | Verification coverage | Added `tests/tidy_headers.cpp` and expanded `tidy_safety` coverage. | `cmake --build build --target tidy_safety` | CLOSED |
| PRCA-003 | Telemetry hub capacity/registration failures were silent, weakening fault visibility. | Runtime fault observability | Made capacity failures explicit and tightened topology verification. | `docs/qualification_report.md`, `src/TelemHub.hpp`, `src/TopologyManager.hpp` | CLOSED |
| PRCA-004 | Live SITL fault injection was unreliable because the bridge used fixed default UDP ports and the campaign depended on buffered live stdout markers. | Operations rehearsal and fault-injection evidence | Added controlled SITL port overrides and changed the campaign to use isolated ports with post-run log validation. | `cmake --build build --target sitl_fault_campaign`, `build/sitl/sitl_fault_campaign_result.json` | CLOSED |
| PRCA-005 | The assurance package was still framework-generic and lacked a mission-shaped allocation, risk, audit, and rehearsal baseline. | Process evidence completeness | Added the reference-mission allocation, risk, assumptions, audit, interface, and rehearsal records. | `python3 tools/software_final_check.py`, `docs/process/*_BASELINE.md`, `docs/process/OPERATIONS_REHEARSAL_20260314.md` | CLOSED |

## Entry Rules

- Re-open an entry when a later change invalidates the closure basis.
- Link release-significant entries into `docs/process/CCB_RELEASE_SIGNOFF_*.md`
  for tagged releases.
- Mission-specific defect logs should extend this file rather than replace it.
