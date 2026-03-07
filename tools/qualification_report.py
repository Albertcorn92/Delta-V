#!/usr/bin/env python3
"""
qualification_report.py

Generate a qualification evidence bundle from DELTA-V flight readiness artifacts.
This script is intended to run after the `flight_readiness` gate succeeds.
"""

from __future__ import annotations

import argparse
import json
import platform
import re
import subprocess
import sys
import zlib
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


TEST_RE = re.compile(r"\bTEST(?:_F)?\s*\(\s*([^,]+?)\s*,\s*([^)]+?)\s*\)")


def run_cmd(cmd: list[str], cwd: Path) -> str:
    try:
        out = subprocess.check_output(
            cmd, cwd=str(cwd), stderr=subprocess.STDOUT, text=True
        )
        return out.strip()
    except Exception:
        return "unavailable"


def crc32_file(path: Path) -> str:
    checksum = 0
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            checksum = zlib.crc32(chunk, checksum)
    return f"{checksum & 0xFFFFFFFF:08x}"


def parse_test_count(tests_path: Path) -> int:
    text = tests_path.read_text(encoding="utf-8")
    return len(TEST_RE.findall(text))


def build_git_info(workspace: Path) -> dict[str, Any]:
    commit = run_cmd(["git", "rev-parse", "HEAD"], workspace)
    branch = run_cmd(["git", "rev-parse", "--abbrev-ref", "HEAD"], workspace)
    status = run_cmd(["git", "status", "--porcelain"], workspace)
    dirty = bool(status and status != "unavailable")
    return {
        "branch": branch,
        "commit": commit,
        "dirty_worktree": dirty,
    }


def load_trace_summary(trace_json: Path) -> dict[str, Any]:
    payload = json.loads(trace_json.read_text(encoding="utf-8"))
    summary = payload.get("summary", {})
    rows = payload.get("rows", [])
    dal_counts: dict[str, int] = {}
    for row in rows:
        dal = str(row.get("dal", "?"))
        dal_counts[dal] = dal_counts.get(dal, 0) + 1

    return {
        "summary": summary,
        "dal_counts": dal_counts,
        "rows_count": len(rows),
    }


def validate_artifacts(paths: list[Path]) -> list[str]:
    missing: list[str] = []
    for p in paths:
        if not p.exists():
            missing.append(str(p))
    return missing


def display_path(path: Path, workspace: Path) -> str:
    resolved = path.resolve()
    workspace_resolved = workspace.resolve()
    try:
        return str(resolved.relative_to(workspace_resolved))
    except ValueError:
        return str(resolved)


def write_markdown(path: Path, report: dict[str, Any]) -> None:
    lines: list[str] = []
    lines.append("# DELTA-V Qualification Report")
    lines.append("")
    lines.append(f"- Generated (UTC): `{report['generated_utc']}`")
    lines.append(f"- Workspace: `{report['workspace']}`")
    lines.append("")
    lines.append("## Build Provenance")
    lines.append("")
    lines.append("| Field | Value |")
    lines.append("|---|---|")
    lines.append(f"| Git branch | `{report['git']['branch']}` |")
    lines.append(f"| Git commit | `{report['git']['commit']}` |")
    lines.append(f"| Dirty worktree | `{report['git']['dirty_worktree']}` |")
    lines.append(f"| Host OS | `{report['host']['platform']}` |")
    lines.append(f"| Python | `{report['host']['python']}` |")
    lines.append(f"| CMake | `{report['host']['cmake']}` |")
    lines.append("")
    lines.append("## Qualification Gates")
    lines.append("")
    lines.append("| Gate | Status | Evidence |")
    lines.append("|---|---|---|")
    for gate in report["gates"]:
        lines.append(
            f"| {gate['name']} | {gate['status']} | {gate['evidence']} |"
        )
    lines.append("")
    lines.append("## Requirements Coverage")
    lines.append("")
    lines.append(
        f"- Total requirements: `{report['trace']['summary']['total_requirements']}`"
    )
    lines.append(
        f"- With direct test evidence: `{report['trace']['summary']['pass_with_tests']}`"
    )
    lines.append(f"- Mapping errors: `{report['trace']['summary']['errors']}`")
    lines.append(f"- Unit test count: `{report['unit_test_count']}`")
    lines.append(f"- DAL distribution: `{report['trace']['dal_counts']}`")
    lines.append("")
    lines.append("## Artifact Checksums (CRC-32)")
    lines.append("")
    lines.append("| Artifact | CRC-32 |")
    lines.append("|---|---|")
    for item in report["hashes"]:
        lines.append(f"| `{item['path']}` | `{item['crc32']}` |")
    lines.append("")
    lines.append("## Manual Evidence Remaining")
    lines.append("")
    lines.append(
        "- On-target HIL campaign evidence (ESP32 runtime, fault injection, soak)."
    )
    lines.append("- Timing/WCET and stack margin evidence on target (`tools/esp32_runtime_guard.py`).")
    lines.append("- Reboot-cycle stability evidence on target (`tools/esp32_reboot_campaign.py`).")
    lines.append(
        "- Program-level DO-178C process records (review signatures, independence, audits)."
    )
    lines.append("")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate a DELTA-V qualification evidence bundle."
    )
    parser.add_argument("--workspace", required=True, type=Path)
    parser.add_argument("--trace-json", required=True, type=Path)
    parser.add_argument("--trace-md", required=True, type=Path)
    parser.add_argument("--tests-file", required=True, type=Path)
    parser.add_argument("--flight-bin", required=True, type=Path)
    parser.add_argument("--tests-bin", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()

    required_artifacts = [
        args.trace_json,
        args.trace_md,
        args.tests_file,
        args.flight_bin,
        args.tests_bin,
    ]
    missing = validate_artifacts(required_artifacts)
    if missing:
        for artifact in missing:
            print(f"[qualification] ERROR: missing artifact: {artifact}", file=sys.stderr)
        return 1

    trace = load_trace_summary(args.trace_json)
    summary = trace["summary"]
    total = int(summary.get("total_requirements", 0))
    with_tests = int(summary.get("pass_with_tests", 0))
    trace_errors = int(summary.get("errors", 0))
    if trace_errors != 0 or total == 0 or with_tests != total:
        print(
            "[qualification] ERROR: traceability summary indicates incomplete coverage.",
            file=sys.stderr,
        )
        return 1

    args.output_dir.mkdir(parents=True, exist_ok=True)
    output_md = args.output_dir / "qualification_report.md"
    output_json = args.output_dir / "qualification_report.json"

    now_utc = datetime.now(tz=timezone.utc).isoformat()
    workspace = args.workspace.resolve()
    git_info = build_git_info(workspace)
    host_info = {
        "platform": platform.platform(),
        "python": platform.python_version(),
        "cmake": run_cmd(["cmake", "--version"], workspace).splitlines()[0],
    }

    hashes = []
    for p in required_artifacts:
        hashes.append(
            {
                "path": display_path(p, workspace),
                "crc32": crc32_file(p),
            }
        )

    report = {
        "generated_utc": now_utc,
        "workspace": str(workspace.name),
        "git": git_info,
        "host": host_info,
        "unit_test_count": parse_test_count(args.tests_file),
        "trace": trace,
        "gates": [
            {
                "name": "Build/Test Gate",
                "status": "PASS",
                "evidence": "flight_readiness target execution",
            },
            {
                "name": "V&V Stress Gate",
                "status": "PASS",
                "evidence": "vnv_stress target in flight_readiness",
            },
            {
                "name": "Static Safety Gate",
                "status": "PASS",
                "evidence": "tidy_safety in flight_readiness",
            },
            {
                "name": "Requirements Traceability Gate",
                "status": "PASS",
                "evidence": display_path(args.trace_json, workspace),
            },
        ],
        "hashes": hashes,
    }

    write_markdown(output_md, report)
    output_json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    print(f"[qualification] OK: report written to {output_md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
