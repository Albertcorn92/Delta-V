# Release Manifest Template

Date: YYYY-MM-DD
Mission: `<mission_name>`
Release: `<git_tag_or_commit>`

Use this manifest to capture final release configuration for mission operations.

## Source and Build

- Git branch: `<branch>`
- Git commit: `<commit>`
- Dirty worktree at build time: `true/false`
- Toolchain versions: `<compiler/cmake/python/esp-idf>`
- Build profile: `<Debug/Release + options>`

## Artifacts

| Artifact | Path | Hash | Notes |
|---|---|---|---|
| Flight binary | `<path>` | `<sha256>` | `<notes>` |
| Test binary | `<path>` | `<sha256>` | `<notes>` |
| Requirements matrix | `<path>` | `<sha256>` | `<notes>` |
| Qualification report | `<path>` | `<sha256>` | `<notes>` |
| Runtime evidence bundle | `<path>` | `<sha256>` | `<notes>` |

## Constraints and Limits

- Known limitations: `<list>`
- Operational constraints: `<list>`
- Open items with mitigation: `<list>`

## Approval

- Configuration control board: `<name / date>`
- Mission authority: `<name / date>`
