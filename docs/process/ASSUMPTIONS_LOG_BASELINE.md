# Assumptions Log Baseline

Date: 2026-03-14
Scope: DELTA-V reference mission public baseline

| Assumption ID | Assumption | Why It Matters | If False | Owner | Status |
|---|---|---|---|---|---|
| ASM-001 | The reference mission is a civilian 3U CubeSat in LEO with scheduled-pass operations. | Sets the operational tempo and mission consequence model used by the process package. | The allocation, operations rehearsal, and hazard wording must be reworked for the real mission. | Maintainer | ACTIVE |
| ASM-002 | The public baseline remains public-source only, with no private operational support from the maintainer. | Keeps the project in a lower-risk civilian/export posture. | Maintainer legal/scope posture changes and requires renewed review. | Maintainer | ACTIVE |
| ASM-003 | EPS or equivalent hardware still owns hard undervoltage protection outside DELTA-V software. | Software thresholds alone are not treated as the only power safety barrier. | Battery/power hazards become more severe and require deeper software closure. | Mission integrator | ACTIVE |
| ASM-004 | The live SITL fault campaign and host soak are representative only of host integration behavior, not flight hardware timing or RF conditions. | Prevents over-claiming host evidence as hardware evidence. | Readiness reports become misleading. | Flight SW | ACTIVE |
| ASM-005 | The public baseline will not ship command-path crypto/auth in the repository. | Keeps the published baseline aligned with the current civilian/legal posture. | The interface, legal, and deployment package must be reclassified. | Maintainer | ACTIVE |
| ASM-006 | The public payload profile is a low-rate reference payload only, not a qualified mission instrument. | Prevents the repo from overstating payload closure while still allowing a concrete payload interface baseline. | Mission payload hazards, timing, and acceptance criteria must still be added before operational use. | Payload team | ACTIVE |

## Use

- Review these assumptions whenever the reference mission profile changes.
- Re-open impacted risks and hazards when an assumption is invalidated.
