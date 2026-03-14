# SITL Long Soak Status

- Generated (UTC): `2026-03-14T22:52:43.971633+00:00`
- Scope: host/SITL archival status for the public DELTA-V baseline
- Rule: missing long-duration runs do not fail `software_final`, but they remain open review gaps until archived.

## Archived Profiles

| Profile | Target | Required Duration | Status | Evidence |
|---|---|---|---|---|
| Default software_final soak gate | `sitl_soak` | 600 s | PASS | build/sitl/sitl_soak_result.json, build/sitl/sitl_soak.log (archived run passed duration and marker checks) |
| 1-hour host/SITL soak | `sitl_soak_1h` | 3600 s | MISSING | build/sitl/archive/sitl_soak_1h_result.json (no archived artifact at build/sitl/archive/sitl_soak_1h_result.json) |
| 6-hour host/SITL soak | `sitl_soak_6h` | 21600 s | MISSING | build/sitl/archive/sitl_soak_6h_result.json (no archived artifact at build/sitl/archive/sitl_soak_6h_result.json) |
| 12-hour host/SITL soak | `sitl_soak_12h` | 43200 s | MISSING | build/sitl/archive/sitl_soak_12h_result.json (no archived artifact at build/sitl/archive/sitl_soak_12h_result.json) |
| 24-hour host/SITL soak | `sitl_soak_24h` | 86400 s | MISSING | build/sitl/archive/sitl_soak_24h_result.json (no archived artifact at build/sitl/archive/sitl_soak_24h_result.json) |

## Notes

- The default `sitl_soak` gate is part of the normal software closeout path.
- The 1h/6h/12h/24h profiles are review-strength evidence for long host/SITL stability.
- These runs are software-only evidence and do not replace sensor-attached HIL or mission hardware qualification.
