#!/usr/bin/env python3
"""
portability_matrix.py

Validate DELTA-V software portability profiles by configuring (and optionally
building) multiple host build profiles, then emit JSON/Markdown artifacts.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


@dataclass(frozen=True)
class Profile:
    profile_id: str
    description: str
    build_type: str
    defines: tuple[str, ...]


PROFILES: dict[str, Profile] = {
    "host-debug-default": Profile(
        profile_id="host-debug-default",
        description="Default host/SITL debug configuration",
        build_type="Debug",
        defines=(),
    ),
    "host-release-default": Profile(
        profile_id="host-release-default",
        description="Default host/SITL release configuration",
        build_type="Release",
        defines=(),
    ),
    "host-debug-local-only": Profile(
        profile_id="host-debug-local-only",
        description="Host build with local-only uplink path forced",
        build_type="Debug",
        defines=("DELTAV_LOCAL_ONLY",),
    ),
    "host-debug-network-disabled": Profile(
        profile_id="host-debug-network-disabled",
        description="Host build with network stack disabled compile path",
        build_type="Debug",
        defines=("DELTAV_DISABLE_NETWORK_STACK",),
    ),
    "host-debug-heapguard-off": Profile(
        profile_id="host-debug-heapguard-off",
        description="Host build with heap-guard runtime lock compiled out",
        build_type="Debug",
        defines=("DELTAV_DISABLE_HOST_HEAP_GUARD",),
    ),
}


def run_command(cmd: list[str], cwd: Path, log_path: Path) -> tuple[int, float]:
    start = time.monotonic()
    proc = subprocess.run(
        cmd,
        cwd=str(cwd),
        capture_output=True,
        text=True,
        check=False,
    )
    duration = round(time.monotonic() - start, 3)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.write_text(
        f"$ {' '.join(cmd)}\n"
        f"[exit={proc.returncode} duration_s={duration}]\n\n"
        f"--- stdout ---\n{proc.stdout}\n\n--- stderr ---\n{proc.stderr}\n",
        encoding="utf-8",
    )
    return proc.returncode, duration


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def write_markdown(path: Path, payload: dict[str, Any]) -> None:
    lines: list[str] = []
    lines.append("# DELTA-V Software Portability Matrix (Generated)")
    lines.append("")
    lines.append(f"- Generated (UTC): `{payload['generated_utc']}`")
    lines.append(f"- Workspace: `{payload['workspace']}`")
    lines.append(f"- Configure only: `{payload['configure_only']}`")
    lines.append("")
    lines.append("| Profile | Description | Build Type | Defines | Configure | Build | Status |")
    lines.append("|---|---|---|---|---|---|---|")
    for row in payload["profiles"]:
        defines = ", ".join(row["defines"]) if row["defines"] else "none"
        build_cell = (
            str(row["build"]["rc"]) if row["build"] is not None else "skipped"
        )
        lines.append(
            f"| `{row['profile_id']}` | {row['description']} | `{row['build_type']}` | "
            f"`{defines}` | `{row['configure']['rc']}` | `{build_cell}` | `{row['status']}` |"
        )
    lines.append("")
    lines.append("## Summary")
    lines.append("")
    lines.append(f"- Total profiles: `{payload['summary']['total']}`")
    lines.append(f"- Passed: `{payload['summary']['pass']}`")
    lines.append(f"- Failed: `{payload['summary']['fail']}`")
    lines.append("")
    lines.append("## Artifacts")
    lines.append("")
    lines.append(f"- JSON: `{payload['output_json']}`")
    lines.append(f"- Logs dir: `{payload['logs_dir']}`")
    lines.append("")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_profiles(raw: str | None) -> list[Profile]:
    if not raw:
        return [PROFILES[k] for k in PROFILES]
    requested = [item.strip() for item in raw.split(",") if item.strip()]
    unknown = [item for item in requested if item not in PROFILES]
    if unknown:
        raise ValueError(f"unknown profile(s): {', '.join(unknown)}")
    return [PROFILES[item] for item in requested]


def configure_cmd(workspace: Path, build_dir: Path, profile: Profile) -> list[str]:
    cmd = [
        "cmake",
        "-S",
        str(workspace),
        "-B",
        str(build_dir),
        f"-DCMAKE_BUILD_TYPE={profile.build_type}",
    ]
    if profile.defines:
        defs = " ".join(f"-D{item}" for item in profile.defines)
        cmd.append(f"-DCMAKE_CXX_FLAGS={defs}")
    return cmd


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate DELTA-V host portability profiles and emit matrix artifacts."
    )
    parser.add_argument("--workspace", type=Path, default=Path("."))
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help="Output directory (default: <workspace>/build/portability)",
    )
    parser.add_argument(
        "--profiles",
        default="",
        help="Comma-separated profile IDs to run (default: all).",
    )
    parser.add_argument(
        "--configure-only",
        action="store_true",
        help="Only run CMake configure for each profile (skip build).",
    )
    parser.add_argument(
        "--allow-failures",
        action="store_true",
        help="Exit 0 even if one or more profiles fail.",
    )
    args = parser.parse_args()

    workspace = args.workspace.resolve()
    if not (workspace / "CMakeLists.txt").exists():
        print(f"[portability] ERROR: missing CMakeLists.txt in workspace {workspace}", file=sys.stderr)
        return 2

    try:
        profiles = parse_profiles(args.profiles or None)
    except ValueError as exc:
        print(f"[portability] ERROR: {exc}", file=sys.stderr)
        return 2

    output_dir = (
        args.output_dir.resolve()
        if args.output_dir is not None
        else (workspace / "build" / "portability").resolve()
    )
    logs_dir = output_dir / "logs"
    output_json = output_dir / "portability_matrix.json"
    output_md = output_dir / "portability_matrix.md"

    results: list[dict[str, Any]] = []
    fail_count = 0

    for profile in profiles:
        profile_build_dir = output_dir / profile.profile_id
        cfg_log = logs_dir / f"{profile.profile_id}_configure.log"
        bld_log = logs_dir / f"{profile.profile_id}_build.log"

        cfg_cmd = configure_cmd(workspace, profile_build_dir, profile)
        cfg_rc, cfg_sec = run_command(cfg_cmd, workspace, cfg_log)
        bld: dict[str, Any] | None = None

        status = "PASS"
        if cfg_rc != 0:
            status = "FAIL"
        elif not args.configure_only:
            build_cmd = [
                "cmake",
                "--build",
                str(profile_build_dir),
                "--target",
                "flight_software",
            ]
            bld_rc, bld_sec = run_command(build_cmd, workspace, bld_log)
            bld = {
                "rc": bld_rc,
                "duration_s": bld_sec,
                "log_path": str(bld_log),
                "cmd": build_cmd,
            }
            if bld_rc != 0:
                status = "FAIL"

        if status != "PASS":
            fail_count += 1

        results.append(
            {
                "profile_id": profile.profile_id,
                "description": profile.description,
                "build_type": profile.build_type,
                "defines": list(profile.defines),
                "configure": {
                    "rc": cfg_rc,
                    "duration_s": cfg_sec,
                    "log_path": str(cfg_log),
                    "cmd": cfg_cmd,
                },
                "build": bld,
                "status": status,
            }
        )
        print(f"[portability] {profile.profile_id}: {status}")

    payload: dict[str, Any] = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "workspace": str(workspace),
        "configure_only": bool(args.configure_only),
        "profiles": results,
        "summary": {
            "total": len(results),
            "pass": len(results) - fail_count,
            "fail": fail_count,
        },
        "output_json": str(output_json),
        "output_md": str(output_md),
        "logs_dir": str(logs_dir),
    }
    write_json(output_json, payload)
    write_markdown(output_md, payload)

    print(f"[portability] json: {output_json}")
    print(f"[portability] md:   {output_md}")

    if fail_count and not args.allow_failures:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
