#!/usr/bin/env python3
"""
release_pedigree.py

Validate release pedigree preconditions for a public DELTA-V release and generate
current release manifest artifacts.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


PASS = "PASS"
FAIL = "FAIL"


def run_cmd(cmd: list[str], cwd: Path, *, allow_fail: bool = False) -> str:
    try:
        out = subprocess.check_output(
            cmd, cwd=str(cwd), stderr=subprocess.STDOUT, text=True
        )
        return out.strip()
    except Exception:
        if allow_fail:
            return ""
        raise


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def digest256_file(path: Path) -> str:
    digest = hashlib.blake2b(digest_size=32)
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def display_path(path: Path, workspace: Path) -> str:
    resolved = path.resolve()
    workspace_resolved = workspace.resolve()
    try:
        return str(resolved.relative_to(workspace_resolved))
    except ValueError:
        return str(resolved)


def require_file(path: Path) -> None:
    if not path.exists():
        raise FileNotFoundError(f"missing required artifact: {path}")


def git_info(workspace: Path) -> dict[str, Any]:
    commit = run_cmd(["git", "rev-parse", "HEAD"], workspace)
    branch = run_cmd(["git", "rev-parse", "--abbrev-ref", "HEAD"], workspace)
    status = run_cmd(["git", "status", "--porcelain"], workspace, allow_fail=True)
    tag = run_cmd(
        ["git", "describe", "--tags", "--exact-match"], workspace, allow_fail=True
    )
    return {
        "branch": branch or "unavailable",
        "commit": commit or "unavailable",
        "exact_tag": tag or "untagged",
        "dirty_worktree": bool(status),
    }


def git_status_entries(workspace: Path, limit: int = 25) -> tuple[list[dict[str, str]], int]:
    status = run_cmd(["git", "status", "--short"], workspace, allow_fail=True)
    lines = [line.rstrip() for line in status.splitlines() if line.strip()]
    entries: list[dict[str, str]] = []
    for line in lines[:limit]:
        code = line[:2].strip() or "??"
        path = line[3:] if len(line) > 3 else line
        entries.append({"code": code, "path": path})
    return entries, len(lines)


def parse_cmake_cache(build_dir: Path) -> dict[str, str]:
    cache_path = build_dir / "CMakeCache.txt"
    if not cache_path.exists():
        return {}

    values: dict[str, str] = {}
    for line in cache_path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith(("//", "#")) or "=" not in line or ":" not in line:
            continue
        key_type, value = line.split("=", 1)
        key, _ = key_type.split(":", 1)
        values[key] = value
    return values


def validate_qualification(qualification_payload: dict[str, Any]) -> None:
    gates = qualification_payload.get("gates", [])
    if not isinstance(gates, list) or not gates:
        raise ValueError("qualification report contains no gate results")
    failing = [
        gate.get("name", "?")
        for gate in gates
        if str(gate.get("status", "")).upper() != PASS
    ]
    if failing:
        raise ValueError(
            "qualification report has failing gates: " + ", ".join(sorted(failing))
        )


def sync_artifact(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def write_markdown(path: Path, report: dict[str, Any]) -> None:
    lines: list[str] = []
    lines.append("# DELTA-V Release Pedigree")
    lines.append("")
    lines.append(f"- Generated (UTC): `{report['generated_utc']}`")
    lines.append(f"- Workspace: `{report['workspace']}`")
    lines.append(f"- Status: `{report['status']}`")
    lines.append("")
    lines.append("## Preconditions")
    lines.append("")
    lines.append("| Check | Status | Evidence |")
    lines.append("|---|---|---|")
    for check in report["checks"]:
        lines.append(f"| {check['name']} | {check['status']} | {check['evidence']} |")
    lines.append("")
    lines.append("## Source and Toolchain Provenance")
    lines.append("")
    lines.append("| Field | Value |")
    lines.append("|---|---|")
    lines.append(f"| Git branch | `{report['git']['branch']}` |")
    lines.append(f"| Git commit | `{report['git']['commit']}` |")
    lines.append(f"| Exact tag | `{report['git']['exact_tag']}` |")
    lines.append(f"| Dirty worktree | `{report['git']['dirty_worktree']}` |")
    lines.append(f"| Build type | `{report['toolchain']['build_type']}` |")
    lines.append(f"| C compiler | `{report['toolchain']['c_compiler']}` |")
    lines.append(f"| C++ compiler | `{report['toolchain']['cxx_compiler']}` |")
    lines.append(f"| Python | `{report['host']['python']}` |")
    lines.append(f"| CMake | `{report['host']['cmake']}` |")
    lines.append(f"| Host OS | `{report['host']['platform']}` |")
    lines.append("")
    lines.append("## Controlled Baseline Documents")
    lines.append("")
    for item in report["baseline_docs"]:
        lines.append(f"- `{item}`")
    lines.append("")
    lines.append("## Artifact Hashes")
    lines.append("")
    lines.append("| Artifact | Digest-256 | Notes |")
    lines.append("|---|---|---|")
    for artifact in report["artifacts"]:
        lines.append(
            f"| `{artifact['path']}` | `{artifact['digest256']}` | {artifact['notes']} |"
        )
    lines.append("")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_manifest(path: Path, report: dict[str, Any]) -> None:
    lines: list[str] = []
    lines.append("# Release Manifest")
    lines.append("")
    lines.append(f"Date: {report['generated_utc'][:10]}")
    lines.append("Mission: DELTA-V reference mission baseline")
    lines.append(
        f"Release: {report['git']['exact_tag']} ({report['git']['commit']})"
        if report["git"]["exact_tag"] != "untagged"
        else f"Release: {report['git']['commit']}"
    )
    lines.append("")
    lines.append("## Source and Build")
    lines.append("")
    lines.append(f"- Git branch: `{report['git']['branch']}`")
    lines.append(f"- Git commit: `{report['git']['commit']}`")
    lines.append(f"- Exact tag: `{report['git']['exact_tag']}`")
    lines.append(f"- Dirty worktree at build time: `{report['git']['dirty_worktree']}`")
    lines.append(f"- Build profile: `{report['toolchain']['build_type']}`")
    lines.append(
        f"- Toolchain versions: `C={report['toolchain']['c_compiler']}`, "
        f"`CXX={report['toolchain']['cxx_compiler']}`, "
        f"`CMake={report['host']['cmake']}`, `Python={report['host']['python']}`"
    )
    lines.append(f"- Host OS: `{report['host']['platform']}`")
    lines.append("")
    lines.append("## Artifacts")
    lines.append("")
    lines.append("| Artifact | Path | Hash | Notes |")
    lines.append("|---|---|---|---|")
    for artifact in report["artifacts"]:
        lines.append(
            f"| {artifact['name']} | `{artifact['path']}` | `{artifact['digest256']}` | {artifact['notes']} |"
        )
    lines.append("")
    lines.append("## Constraints and Limits")
    lines.append("")
    lines.append("- Framework baseline only; not a standalone flight qualification claim.")
    lines.append("- Civilian/scientific/educational scope only.")
    lines.append("- No protected-uplink additions are included in the baseline release.")
    lines.append("- Mission hardware qualification, authority sign-off, and operational support remain outside this release.")
    lines.append("")
    lines.append("## Approval")
    lines.append("")
    lines.append("- Configuration control board: see `docs/process/CCB_RELEASE_SIGNOFF_*.md`")
    lines.append("- Independent review: see `docs/process/INDEPENDENT_REVIEW_RECORD_*.md`")
    lines.append("- Deployment screening baseline: see `docs/process/DEPLOYMENT_SCREENING_LOG_*.md`")
    lines.append("")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_preflight(path: Path, report: dict[str, Any]) -> None:
    lines: list[str] = []
    lines.append("# DELTA-V Release Preflight")
    lines.append("")
    lines.append(f"- Generated (UTC): `{report['generated_utc']}`")
    lines.append(f"- Workspace: `{report['workspace']}`")
    lines.append(f"- Status: `{report['status']}`")
    lines.append("")
    lines.append("## Current Checks")
    lines.append("")
    lines.append("| Check | Status | Evidence |")
    lines.append("|---|---|---|")
    for check in report["checks"]:
        lines.append(f"| {check['name']} | {check['status']} | {check['evidence']} |")
    lines.append("")
    lines.append("## Release Blockers")
    lines.append("")
    if report["release_blockers"]:
        for blocker in report["release_blockers"]:
            lines.append(f"- {blocker}")
    else:
        lines.append("- None. The current baseline is ready for `release_candidate`.")
    lines.append("")
    lines.append("## Dirty Worktree Excerpt")
    lines.append("")
    if report["dirty_entries"]:
        lines.append("| Status | Path |")
        lines.append("|---|---|")
        for entry in report["dirty_entries"]:
            lines.append(f"| `{entry['code']}` | `{entry['path']}` |")
        if report["dirty_entry_total"] > len(report["dirty_entries"]):
            remaining = report["dirty_entry_total"] - len(report["dirty_entries"])
            lines.append("")
            lines.append(f"- Additional unlisted entries: `{remaining}`")
    else:
        lines.append("- Worktree is clean.")
    lines.append("")
    lines.append("## Next Steps")
    lines.append("")
    for step in report["next_steps"]:
        lines.append(f"- {step}")
    lines.append("")
    lines.append("## Commands")
    lines.append("")
    lines.append("```bash")
    lines.append("git status --short")
    lines.append("cmake --build build --target release_preflight")
    lines.append('git tag -a vX.Y.Z -m "DELTA-V vX.Y.Z"')
    lines.append("cmake --build build --target release_candidate")
    lines.append("```")
    lines.append("")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate DELTA-V public release pedigree and write manifest artifacts."
    )
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--sync-docs", action="store_true")
    parser.add_argument("--sync-preflight-docs", action="store_true")
    parser.add_argument("--require-clean-worktree", action="store_true")
    parser.add_argument("--require-exact-tag", action="store_true")
    args = parser.parse_args()

    workspace = args.workspace.resolve()
    build_dir = args.build_dir.resolve()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    required_docs = [
        workspace / "docs" / "process" / "SOFTWARE_CLASSIFICATION_BASELINE.md",
        workspace / "docs" / "process" / "PSAC_DELTAV_BASELINE.md",
        workspace / "docs" / "process" / "SCMP_DELTAV_BASELINE.md",
        workspace / "docs" / "process" / "SQAP_DELTAV_BASELINE.md",
        workspace / "docs" / "process" / "SVVP_DELTAV_BASELINE.md",
        workspace / "docs" / "process" / "TOOL_GOVERNANCE_BASELINE.md",
        workspace / "docs" / "process" / "REFERENCE_MISSION_PROFILE.md",
    ]
    for path in required_docs:
        require_file(path)

    required_artifacts = [
        build_dir / "flight_software",
        build_dir / "run_tests",
        build_dir / "requirements_trace_matrix.json",
        build_dir / "requirements_trace_matrix.md",
        build_dir / "qualification" / "qualification_report.json",
        build_dir / "qualification" / "qualification_report.md",
        workspace / "docs" / "SOFTWARE_FINAL_STATUS.md",
    ]
    for path in required_artifacts:
        require_file(path)

    qualification = load_json(build_dir / "qualification" / "qualification_report.json")
    validate_qualification(qualification)

    git = git_info(workspace)
    dirty_entries, dirty_entry_total = git_status_entries(workspace)
    checks = [
        {
            "name": "Qualification bundle present",
            "status": PASS,
            "evidence": "build/qualification/qualification_report.json",
        },
        {
            "name": "Software-final docs synchronized",
            "status": PASS,
            "evidence": "docs/SOFTWARE_FINAL_STATUS.md",
        },
        {
            "name": "Worktree clean",
            "status": PASS if not git["dirty_worktree"] else FAIL,
            "evidence": "git status --porcelain clean"
            if not git["dirty_worktree"]
            else "working tree has uncommitted or untracked changes",
        },
        {
            "name": "Exact release tag present",
            "status": PASS if git["exact_tag"] != "untagged" else FAIL,
            "evidence": git["exact_tag"]
            if git["exact_tag"] != "untagged"
            else "git describe --tags --exact-match returned no tag",
        },
    ]

    if args.require_clean_worktree and git["dirty_worktree"]:
        raise SystemExit("[release-pedigree] ERROR: worktree is dirty; release_candidate requires a clean tree.")
    if args.require_exact_tag and git["exact_tag"] == "untagged":
        raise SystemExit("[release-pedigree] ERROR: no exact git tag found; release_candidate requires a tagged commit.")

    release_blockers: list[str] = []
    next_steps: list[str] = []
    if git["dirty_worktree"]:
        release_blockers.append("Worktree is dirty. Commit or intentionally set aside local changes before cutting a release tag.")
        next_steps.append("Review `git status --short`, then commit the intended release set on a clean worktree.")
    else:
        next_steps.append("Worktree is clean.")

    if git["exact_tag"] == "untagged":
        release_blockers.append("Current commit does not have an exact release tag.")
        next_steps.append("Create an annotated semantic-version tag on the exact release commit.")
    else:
        next_steps.append(f"Exact tag `{git['exact_tag']}` is present.")

    if not release_blockers:
        next_steps.append("Run `cmake --build build --target release_candidate` to generate the public release pedigree and manifest.")
    else:
        next_steps.append("Rerun `cmake --build build --target release_preflight` after cleanup to confirm all blockers are closed.")

    cache = parse_cmake_cache(build_dir)
    cmake_version = run_cmd(["cmake", "--version"], workspace, allow_fail=True)
    toolchain = {
        "build_type": cache.get("CMAKE_BUILD_TYPE", "unknown"),
        "c_compiler": cache.get("CMAKE_C_COMPILER", "unknown"),
        "cxx_compiler": cache.get("CMAKE_CXX_COMPILER", "unknown"),
    }
    host = {
        "platform": platform.platform(),
        "python": platform.python_version(),
        "cmake": cmake_version.splitlines()[0] if cmake_version else "unavailable",
    }

    artifact_specs = [
        ("Flight binary", build_dir / "flight_software", "Primary SITL executable."),
        ("Test binary", build_dir / "run_tests", "Unit-test executable used by qualification."),
        (
            "Requirements matrix JSON",
            build_dir / "requirements_trace_matrix.json",
            "Traceability source synchronized by software_final.",
        ),
        (
            "Requirements matrix Markdown",
            build_dir / "requirements_trace_matrix.md",
            "Reviewer-readable traceability view.",
        ),
        (
            "Qualification report JSON",
            build_dir / "qualification" / "qualification_report.json",
            "Machine-readable qualification summary.",
        ),
        (
            "Qualification report Markdown",
            build_dir / "qualification" / "qualification_report.md",
            "Reviewer-readable qualification summary.",
        ),
        (
            "Software final status",
            workspace / "docs" / "SOFTWARE_FINAL_STATUS.md",
            "Synchronized software closeout result.",
        ),
    ]
    artifacts = [
        {
            "name": name,
            "path": display_path(path, workspace),
            "digest256": digest256_file(path),
            "notes": notes,
        }
        for name, path, notes in artifact_specs
    ]

    overall_status = PASS if all(item["status"] == PASS for item in checks) else FAIL
    report = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "workspace": workspace.name,
        "status": overall_status,
        "git": git,
        "checks": checks,
        "dirty_entries": dirty_entries,
        "dirty_entry_total": dirty_entry_total,
        "release_blockers": release_blockers,
        "next_steps": next_steps,
        "toolchain": toolchain,
        "host": host,
        "baseline_docs": [display_path(path, workspace) for path in required_docs],
        "artifacts": artifacts,
    }

    pedigree_md = output_dir / "release_pedigree.md"
    pedigree_json = output_dir / "release_pedigree.json"
    manifest_md = output_dir / "release_manifest.md"
    manifest_json = output_dir / "release_manifest.json"
    preflight_md = output_dir / "release_preflight.md"
    preflight_json = output_dir / "release_preflight.json"
    write_markdown(pedigree_md, report)
    pedigree_json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    write_manifest(manifest_md, report)
    manifest_json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    write_preflight(preflight_md, report)
    preflight_json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if args.sync_docs:
        process_dir = workspace / "docs" / "process"
        sync_artifact(pedigree_md, process_dir / "RELEASE_PEDIGREE_CURRENT.md")
        sync_artifact(pedigree_json, process_dir / "RELEASE_PEDIGREE_CURRENT.json")
        sync_artifact(manifest_md, process_dir / "RELEASE_MANIFEST_CURRENT.md")
        sync_artifact(manifest_json, process_dir / "RELEASE_MANIFEST_CURRENT.json")

    if args.sync_preflight_docs:
        process_dir = workspace / "docs" / "process"
        sync_artifact(preflight_md, process_dir / "RELEASE_PREFLIGHT_CURRENT.md")
        sync_artifact(preflight_json, process_dir / "RELEASE_PREFLIGHT_CURRENT.json")

    print(f"[release-pedigree] STATUS: {overall_status}")
    print(f"[release-pedigree] OK: artifacts written to {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
