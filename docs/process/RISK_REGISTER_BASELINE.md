# Risk Register Baseline

Date: 2026-03-14
Scope: DELTA-V reference mission public baseline

| Risk ID | Risk Statement | Likelihood | Consequence | Mitigation / Control | Owner | Status |
|---|---|---|---|---|---|---|
| RSK-001 | Sensor-attached HIL evidence is still absent, so software behavior with real sensors and buses is not closed. | High | Severe | Keep flight-readiness false until mission hardware evidence is archived. | Mission integrator | OPEN |
| RSK-002 | Independent assurance authority is not established for the public single-maintainer baseline. | Medium | Major | Keep independent review records explicit, and require mission-owned reviewer independence for operational use. | Mission program | OPEN |
| RSK-003 | The public baseline intentionally excludes command-path crypto/auth, so it is not suitable as-is for exposed operational uplinks. | Medium | Severe | Keep the repo in public-source/civilian scope and require mission-owned security controls outside the baseline. | Mission integrator | ACCEPTED FOR PUBLIC BASELINE |
| RSK-004 | The public payload profile is only a bounded reference implementation and does not replace mission payload qualification or mission-specific safety constraints. | Medium | Moderate | Keep the reference payload profile documented, but require mission-owned payload ICD, hazards, and qualification before operational use. | Payload team | CONTROLLED |
| RSK-005 | Long-duration host/SITL rehearsal beyond the default 10-minute software gate is not yet fully archived. | Medium | Moderate | Run and archive `sitl_soak_1h`, `sitl_soak_6h`, `sitl_soak_12h`, and `sitl_soak_24h`; track status in `docs/process/SITL_LONG_SOAK_STATUS.md`. | Flight SW | OPEN |
| RSK-006 | Tagged release pedigree is not present until a clean tagged release is cut. | Low | Moderate | Use `release_candidate` on the exact public release commit before external publication. | Maintainer | CONTROLLED |

## Review Notes

- Risks marked `OPEN` block mission-deployable claims but do not block the
  public reference baseline.
- `ACCEPTED FOR PUBLIC BASELINE` means the risk is intentionally kept outside
  the scope of this public repository and must not be hidden.
