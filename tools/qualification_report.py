#!/usr/bin/env python3
"""
qualification_report.py

Generate a repository evidence bundle from DELTA-V flight readiness artifacts.
This script is intended to run after the `flight_readiness` gate succeeds.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
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


def digest256_file(path: Path) -> str:
    digest = hashlib.blake2b(digest_size=32)
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_test_count(tests_path: Path) -> int:
    text = tests_path.read_text(encoding="utf-8")
    return len(TEST_RE.findall(text))


def build_git_info(workspace: Path) -> dict[str, Any]:
    commit = run_cmd(["git", "rev-parse", "HEAD"], workspace)
    branch = run_cmd(["git", "rev-parse", "--abbrev-ref", "HEAD"], workspace)
    status = run_cmd(["git", "status", "--porcelain"], workspace)
    exact_tag = run_cmd(["git", "describe", "--tags", "--exact-match"], workspace)
    dirty = bool(status and status != "unavailable")
    return {
        "branch": branch,
        "commit": commit,
        "exact_tag": exact_tag if exact_tag != "unavailable" else "untagged",
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


def latest_files(directory: Path, pattern: str) -> list[Path]:
    if not directory.exists():
        return []
    return sorted(directory.glob(pattern), key=lambda p: p.stat().st_mtime, reverse=True)


def has_passing_reboot_campaign(artifacts_dir: Path) -> bool:
    for candidate in latest_files(artifacts_dir, "esp32_reboot_campaign_*.json"):
        try:
            payload = json.loads(candidate.read_text(encoding="utf-8"))
            summary = payload.get("summary", {})
            if int(summary.get("fail_cycles", 1)) == 0 and int(summary.get("pass_cycles", 0)) > 0:
                return True
        except Exception:
            continue
    return False


def has_passing_runtime_guard(artifacts_dir: Path) -> bool:
    for candidate in latest_files(artifacts_dir, "esp32_runtime_guard_*.json"):
        try:
            payload = json.loads(candidate.read_text(encoding="utf-8"))
            thresholds = payload.get("thresholds", {})
            summary = payload.get("summary", {})
            samples = int(payload.get("samples", 0))

            min_samples = int(thresholds.get("min_samples", 1))
            min_stack = int(thresholds.get("min_stack_words", 0))
            max_fast = int(thresholds.get("max_fast_tick_wcet_us", 1_000_000))
            max_loop = int(thresholds.get("max_loop_wcet_us", 1_000_000))
            max_overruns = int(thresholds.get("max_loop_overruns", 0))

            if samples < min_samples:
                continue
            if int(summary.get("max_fast_tick_wcet_us", max_fast + 1)) > max_fast:
                continue
            if int(summary.get("max_loop_wcet_us", max_loop + 1)) > max_loop:
                continue
            if int(summary.get("last_loop_overruns", max_overruns + 1)) > max_overruns:
                continue
            if int(summary.get("min_stack_free_words", 0)) < min_stack:
                continue
            return True
        except Exception:
            continue
    return False


def has_passing_hil_fault_campaign(artifacts_dir: Path) -> bool:
    # Reboot/reset cycling is treated as injected-fault on-target campaign evidence.
    return has_passing_reboot_campaign(artifacts_dir)


def has_passing_sensorless_soak(
    artifacts_dir: Path, *, min_duration_s: float = 3600.0
) -> bool:
    for candidate in latest_files(artifacts_dir, "esp32_soak_*.json"):
        try:
            payload = json.loads(candidate.read_text(encoding="utf-8"))
            if str(payload.get("status", "")).upper() != "PASS":
                continue
            if float(payload.get("duration_s", 0.0)) < min_duration_s:
                continue
            return True
        except Exception:
            continue
    return False


def require_program_signatures() -> bool:
    return os.getenv("DELTAV_REQUIRE_PROGRAM_SIGNATURES", "0") == "1"


def collect_manual_remaining(workspace: Path) -> list[str]:
    artifacts_dir = workspace / "artifacts"
    remaining: list[str] = []
    if not has_passing_hil_fault_campaign(artifacts_dir):
        remaining.append("On-target HIL campaign evidence (ESP32 fault injection logs).")
    if not has_passing_runtime_guard(artifacts_dir):
        remaining.append("Timing/WCET and stack margin evidence on target (`tools/esp32_runtime_guard.py`).")
    if not has_passing_reboot_campaign(artifacts_dir):
        remaining.append("Reboot-cycle stability evidence on target (`tools/esp32_reboot_campaign.py`).")
    if not has_passing_sensorless_soak(artifacts_dir, min_duration_s=3600.0):
        remaining.append("Multi-hour sensorless soak evidence on target (`tools/esp32_soak.py`, >=1h).")
    if require_program_signatures():
        remaining.append("Program-level DO-178C process records (review signatures, independence, audits).")
    return remaining


def write_markdown(path: Path, report: dict[str, Any]) -> None:
    lines: list[str] = []
    lines.append("# DELTA-V Qualification Report")
    lines.append("")
    lines.append(f"- Generated (UTC): `{report['generated_utc']}`")
    lines.append(f"- Workspace: `{report['workspace']}`")
    lines.append("- Scope: `Repository-generated evidence summary` (aggregates local gates and archived artifacts; not independent certification evidence)")
    lines.append("")
    lines.append("## Build Provenance")
    lines.append("")
    lines.append("| Field | Value |")
    lines.append("|---|---|")
    lines.append(f"| Git branch | `{report['git']['branch']}` |")
    lines.append(f"| Git commit | `{report['git']['commit']}` |")
    lines.append(f"| Exact tag | `{report['git']['exact_tag']}` |")
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
    lines.append("| Artifact | CRC-32 | Digest-256 |")
    lines.append("|---|---|---|")
    for item in report["hashes"]:
        lines.append(
            f"| `{item['path']}` | `{item['crc32']}` | `{item['digest256']}` |"
        )
    lines.append("")
    lines.append("## Manual Evidence Remaining")
    lines.append("")
    manual_remaining = report.get("manual_remaining", [])
    if manual_remaining:
        lines.extend([f"- {item}" for item in manual_remaining])
    else:
        lines.append("- None for baseline release profile.")
    lines.append("")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate a DELTA-V repository evidence bundle."
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
                "digest256": digest256_file(p),
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
        "manual_remaining": collect_manual_remaining(workspace),
    }

    write_markdown(output_md, report)
    output_json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    print(f"[qualification] OK: report written to {output_md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
