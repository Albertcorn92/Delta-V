#!/usr/bin/env python3
"""
coverage_trend.py

Generate a machine-readable coverage trend snapshot from lcov output.
This complements coverage_guard with historical tracking in CI artifacts.
"""

from __future__ import annotations

import argparse
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


LCOV_KEYS = ("LF", "LH", "BRF", "BRH", "FNF", "FNH")


def parse_lcov_info(path: Path) -> dict[str, int]:
    totals = {key: 0 for key in LCOV_KEYS}
    for raw_line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        if ":" not in raw_line:
            continue
        key, value = raw_line.split(":", 1)
        key = key.strip()
        if key not in totals:
            continue
        try:
            totals[key] += int(value.strip())
        except ValueError:
            continue
    return totals


def pct(hit: int, found: int) -> float:
    if found <= 0:
        return 0.0
    return (100.0 * float(hit)) / float(found)


def git_value(workspace: Path, *args: str) -> str:
    try:
        out = subprocess.check_output(
            ["git", *args], cwd=str(workspace), stderr=subprocess.DEVNULL, text=True
        )
        return out.strip()
    except Exception:
        return "unavailable"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate DELTA-V coverage trend snapshot from lcov output."
    )
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--info-file", type=Path, default=None)
    parser.add_argument(
        "--thresholds",
        type=Path,
        default=None,
        help="Optional thresholds JSON (defaults to docs/COVERAGE_THRESHOLDS.json).",
    )
    parser.add_argument(
        "--output-json",
        type=Path,
        default=None,
        help="Output JSON path (defaults to <build>/coverage_trend.json).",
    )
    args = parser.parse_args()

    workspace = args.workspace.resolve()
    build_dir = args.build_dir.resolve()
    info_file = args.info_file.resolve() if args.info_file else build_dir / "cov.info"
    thresholds_file = (
        args.thresholds.resolve()
        if args.thresholds
        else workspace / "docs" / "COVERAGE_THRESHOLDS.json"
    )
    output_json = (
        args.output_json.resolve()
        if args.output_json
        else build_dir / "coverage_trend.json"
    )

    if not info_file.exists():
        print(f"[coverage-trend] ERROR: missing lcov file: {info_file}")
        return 2

    totals = parse_lcov_info(info_file)
    line_found = int(totals["LF"])
    line_hit = int(totals["LH"])
    branch_found = int(totals["BRF"])
    branch_hit = int(totals["BRH"])
    fn_found = int(totals["FNF"])
    fn_hit = int(totals["FNH"])

    thresholds: dict[str, Any] = {}
    if thresholds_file.exists():
        try:
            thresholds = json.loads(thresholds_file.read_text(encoding="utf-8"))
        except Exception:
            thresholds = {}

    payload = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "git": {
            "branch": git_value(workspace, "rev-parse", "--abbrev-ref", "HEAD"),
            "commit": git_value(workspace, "rev-parse", "HEAD"),
        },
        "coverage": {
            "line_pct": round(pct(line_hit, line_found), 3),
            "branch_pct": round(pct(branch_hit, branch_found), 3)
            if branch_found > 0
            else 100.0,
            "function_pct": round(pct(fn_hit, fn_found), 3),
            "line_hit": line_hit,
            "line_found": line_found,
            "branch_hit": branch_hit,
            "branch_found": branch_found,
            "function_hit": fn_hit,
            "function_found": fn_found,
        },
        "thresholds": thresholds.get("minimums", {}),
        "inputs": {
            "lcov_file": str(info_file),
            "thresholds_file": str(thresholds_file),
        },
    }

    output_json.parent.mkdir(parents=True, exist_ok=True)
    output_json.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    print(
        "[coverage-trend] OK: "
        f"line={payload['coverage']['line_pct']:.2f}% "
        f"branch={payload['coverage']['branch_pct']:.2f}% "
        f"function={payload['coverage']['function_pct']:.2f}%"
    )
    print(f"[coverage-trend] json: {output_json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
