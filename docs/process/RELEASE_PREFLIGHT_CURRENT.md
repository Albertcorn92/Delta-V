# DELTA-V Release Preflight

- Generated (UTC): `2026-03-14T22:44:47.975368+00:00`
- Workspace: `DELTA-V Framework`
- Status: `FAIL`

## Current Checks

| Check | Status | Evidence |
|---|---|---|
| Qualification bundle present | PASS | build/qualification/qualification_report.json |
| Software-final docs synchronized | PASS | docs/SOFTWARE_FINAL_STATUS.md |
| Worktree clean | FAIL | working tree has uncommitted or untracked changes |
| Exact release tag present | FAIL | git describe --tags --exact-match returned no tag |

## Release Blockers

- Worktree is dirty. Commit or intentionally set aside local changes before cutting a release tag.
- Current commit does not have an exact release tag.

## Dirty Worktree Excerpt

| Status | Path |
|---|---|
| `M` | `github/workflows/ci.yml` |
| `M` | `CMakeLists.txt` |
| `M` | `README.md` |
| `M` | `dictionary.json` |
| `M` | `docs/ARCHITECTURE.md` |
| `M` | `docs/BENCHMARK_BASELINE.json` |
| `M` | `docs/BENCHMARK_BASELINE.md` |
| `M` | `docs/COVERAGE_POLICY.md` |
| `M` | `docs/CUBESAT_READINESS_STATUS.json` |
| `M` | `docs/CUBESAT_READINESS_STATUS.md` |
| `M` | `docs/DEVELOPER_GUIDE.md` |
| `M` | `docs/HIGH_ASSURANCE_ROADMAP.md` |
| `M` | `docs/ICD.md` |
| `M` | `docs/MISSION_ASSURANCE_CHECKLIST.md` |
| `M` | `docs/QUICKSTART_10_MIN.md` |
| `M` | `docs/README.md` |
| `M` | `docs/RELEASE_POLICY.md` |
| `M` | `docs/SAFETY_ASSURANCE.md` |
| `M` | `docs/SITL_SOAK.md` |
| `M` | `docs/SOFTWARE_FINAL_STATUS.md` |
| `M` | `docs/process/CERTIFICATION_BASELINE.md` |
| `M` | `docs/process/FLIGHT_ENV_TEST_MATRIX_20260307.md` |
| `M` | `docs/qualification_report.json` |
| `M` | `docs/qualification_report.md` |
| `M` | `docs/safety_case/README.md` |

- Additional unlisted entries: `50`

## Next Steps

- Review `git status --short`, then commit the intended release set on a clean worktree.
- Create an annotated semantic-version tag on the exact release commit.
- Rerun `cmake --build build --target release_preflight` after cleanup to confirm all blockers are closed.

## Commands

```bash
git status --short
cmake --build build --target release_preflight
git tag -a vX.Y.Z -m "DELTA-V vX.Y.Z"
cmake --build build --target release_candidate
```

