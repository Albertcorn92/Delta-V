# DELTA-V Release Preflight

- Generated (UTC): `2026-03-14T23:01:29.458497+00:00`
- Workspace: `DELTA-V Framework`
- Status: `FAIL`

## Current Checks

| Check | Status | Evidence |
|---|---|---|
| Qualification bundle present | PASS | build/qualification/qualification_report.json |
| Software-final docs synchronized | PASS | docs/SOFTWARE_FINAL_STATUS.md |
| Worktree clean | PASS | git status --porcelain clean |
| Exact release tag present | FAIL | git describe --tags --exact-match returned no tag |

## Release Blockers

- Current commit does not have an exact release tag.

## Dirty Worktree Excerpt

- Worktree is clean.

## Next Steps

- Worktree is clean.
- Create an annotated semantic-version tag on the exact release commit.
- Rerun `cmake --build build --target release_preflight` after cleanup to confirm all blockers are closed.

## Commands

```bash
git status --short
cmake --build build --target release_preflight
git tag -a vX.Y.Z -m "DELTA-V vX.Y.Z"
cmake --build build --target release_candidate
```

