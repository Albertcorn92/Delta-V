#!/usr/bin/env python3
"""
sitl_long_soak_status.py

Summarize archived host/SITL soak evidence for the DELTA-V public baseline.
The report is informational: it records which long-duration runs are archived
without treating missing long campaigns as a software-final failure.
"""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


PASS = "PASS"
FAIL = "FAIL"
MISSING = "MISSING"


PROFILES: list[dict[str, Any]] = [
    {
        "id": "default-gate",
        "label": "Default software_final soak gate",
        "duration_s": 600.0,
        "result_relpath": Path("sitl/sitl_soak_result.json"),
        "log_relpath": Path("sitl/sitl_soak.log"),
        "target": "sitl_soak",
    },
    {
        "id": "soak-1h",
        "label": "1-hour host/SITL soak",
        "duration_s": 3600.0,
        "result_relpath": Path("sitl/archive/sitl_soak_1h_result.json"),
        "log_relpath": Path("sitl/archive/sitl_soak_1h.log"),
        "target": "sitl_soak_1h",
    },
    {
        "id": "soak-6h",
        "label": "6-hour host/SITL soak",
        "duration_s": 21600.0,
        "result_relpath": Path("sitl/archive/sitl_soak_6h_result.json"),
        "log_relpath": Path("sitl/archive/sitl_soak_6h.log"),
        "target": "sitl_soak_6h",
    },
    {
        "id": "soak-12h",
        "label": "12-hour host/SITL soak",
        "duration_s": 43200.0,
        "result_relpath": Path("sitl/archive/sitl_soak_12h_result.json"),
        "log_relpath": Path("sitl/archive/sitl_soak_12h.log"),
        "target": "sitl_soak_12h",
    },
    {
        "id": "soak-24h",
        "label": "24-hour host/SITL soak",
        "duration_s": 86400.0,
        "result_relpath": Path("sitl/archive/sitl_soak_24h_result.json"),
        "log_relpath": Path("sitl/archive/sitl_soak_24h.log"),
        "target": "sitl_soak_24h",
    },
]


def display_path(path: Path, workspace: Path) -> str:
    try:
        return str(path.resolve().relative_to(workspace.resolve()))
    except ValueError:
        return str(path.resolve())


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def evaluate_profile(
    workspace: Path, build_dir: Path, profile: dict[str, Any]
) -> dict[str, Any]:
    result_path = build_dir / profile["result_relpath"]
    log_path = build_dir / profile["log_relpath"]
    duration_s = float(profile["duration_s"])

    status = MISSING
    reason = f"no archived artifact at {display_path(result_path, workspace)}"
    actual_duration_s = 0.0
    generated_utc = None

    if result_path.exists():
        payload = load_json(result_path)
        actual_duration_s = float(payload.get("duration_actual_s", 0.0))
        generated_utc = payload.get("generated_utc")
        run_status = str(payload.get("status", "")).upper()
        if run_status != PASS:
            status = FAIL
            reason = f"artifact status is {run_status or 'UNKNOWN'}"
        elif actual_duration_s < duration_s:
            status = FAIL
            reason = (
                f"duration {actual_duration_s:.1f}s is shorter than "
                f"required {duration_s:.1f}s"
            )
        else:
            status = PASS
            reason = "archived run passed duration and marker checks"

    evidence = display_path(result_path, workspace)
    if log_path.exists():
        evidence = f"{evidence}, {display_path(log_path, workspace)}"

    return {
        "id": profile["id"],
        "label": profile["label"],
        "target": profile["target"],
        "duration_s": duration_s,
        "status": status,
        "reason": reason,
        "generated_utc": generated_utc,
        "actual_duration_s": actual_duration_s,
        "evidence": evidence,
    }


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def write_markdown(path: Path, payload: dict[str, Any]) -> None:
    lines = [
        "# SITL Long Soak Status",
        "",
        f"- Generated (UTC): `{payload['generated_utc']}`",
        "- Scope: host/SITL archival status for the public DELTA-V baseline",
        "- Rule: missing long-duration runs do not fail `software_final`, but they remain open review gaps until archived.",
        "",
        "## Archived Profiles",
        "",
        "| Profile | Target | Required Duration | Status | Evidence |",
        "|---|---|---|---|---|",
    ]

    for profile in payload["profiles"]:
        lines.append(
            f"| {profile['label']} | `{profile['target']}` | "
            f"{int(profile['duration_s'])} s | {profile['status']} | "
            f"{profile['evidence']} ({profile['reason']}) |"
        )

    lines.extend(
        [
            "",
            "## Notes",
            "",
            "- The default `sitl_soak` gate is part of the normal software closeout path.",
            "- The 1h/6h/12h/24h profiles are review-strength evidence for long host/SITL stability.",
            "- These runs are software-only evidence and do not replace sensor-attached HIL or mission hardware qualification.",
            "",
        ]
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate DELTA-V long-duration SITL soak archival status."
    )
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument(
        "--output-md",
        type=Path,
        default=Path("docs/process/SITL_LONG_SOAK_STATUS.md"),
    )
    parser.add_argument(
        "--output-json",
        type=Path,
        default=Path("docs/process/SITL_LONG_SOAK_STATUS.json"),
    )
    args = parser.parse_args()

    workspace = args.workspace.resolve()
    build_dir = args.build_dir.resolve()
    profiles = [evaluate_profile(workspace, build_dir, profile) for profile in PROFILES]

    payload = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "workspace": workspace.name,
        "build_dir": str(build_dir),
        "profiles": profiles,
    }

    output_md = (workspace / args.output_md).resolve() if not args.output_md.is_absolute() else args.output_md
    output_json = (
        (workspace / args.output_json).resolve()
        if not args.output_json.is_absolute()
        else args.output_json
    )

    write_markdown(output_md, payload)
    write_json(output_json, payload)
    print(f"[sitl-long-soak-status] Wrote {output_md} and {output_json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
