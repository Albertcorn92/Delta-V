# DELTA-V Release Preflight

- Generated (UTC): `2026-03-14T23:00:54.288791+00:00`
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
| `M` | `ocs/SOFTWARE_FINAL_STATUS.md` |
| `M` | `docs/qualification_report.json` |
| `M` | `docs/qualification_report.md` |

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

