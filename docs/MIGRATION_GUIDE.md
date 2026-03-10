# DELTA-V Migration Guide Template

Date: 2026-03-10

Use this template when publishing version-to-version migration notes.

## 1) Version Pair

- From: `vX.Y.Z`
- To: `vA.B.C`
- Compatibility level: `patch|minor|major`

## 2) Summary

- Why this release matters.
- Who should upgrade now vs later.
- Any high-risk areas.

## 3) Breaking Changes

List only behavior/API/config changes that can break existing users.

| Area | Old behavior | New behavior | Required action |
|---|---|---|---|
| Example: topology command metadata | optional `op_class` in legacy files | enforced command policy mapping | add `op_class` to each `commands[]` entry |

If none:

- `None`

## 4) New Capabilities (Non-Breaking)

- Feature additions that do not require migration work.

## 5) Step-by-Step Upgrade

```bash
# 1) Sync code
git fetch --tags
git checkout vA.B.C

# 2) Regenerate artifacts from topology source-of-truth
python3 tools/autocoder.py

# 3) Rebuild + local validation gates
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target quickstart_10min

# 4) Optional full software gate
cmake --build build --target software_final
```

## 6) Validation Checklist

- [ ] `run_tests` passes.
- [ ] `run_system_tests` passes.
- [ ] `benchmark_guard` passes.
- [ ] `sitl_smoke` and `sitl_soak` pass.
- [ ] `docs/CUBESAT_READINESS_STATUS.md` reviewed for remaining gaps.

## 7) Rollback Plan

- Previous known-good tag: `vX.Y.Z`
- Rollback command: `git checkout vX.Y.Z`
- Data/format compatibility note (if applicable).

## 8) Scope and Compliance Reminder

- Civilian/open baseline only.
- No command-path cryptography/encryption additions in baseline releases.
- No weaponized or military-targeting behavior.

## 9) Maintainer Notes

- Owner:
- Reviewers:
- Related PRs/issues:
- Release date:
