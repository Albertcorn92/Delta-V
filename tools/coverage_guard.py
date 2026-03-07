#!/usr/bin/env python3
"""
coverage_guard.py

Validate lcov coverage summary against minimum thresholds.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


LCOV_KEYS = ("LF", "LH", "BRF", "BRH", "FNF", "FNH")


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


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


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate DELTA-V lcov coverage percentages against thresholds."
    )
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument(
        "--info-file",
        type=Path,
        default=None,
        help="Path to lcov info file (defaults to <build>/cov.info).",
    )
    parser.add_argument(
        "--thresholds",
        type=Path,
        default=None,
        help="Path to thresholds JSON (defaults to docs/COVERAGE_THRESHOLDS.json).",
    )
    args = parser.parse_args()

    workspace = args.workspace.resolve()
    build_dir = args.build_dir.resolve()

    info_file = (
        args.info_file.resolve()
        if args.info_file is not None
        else build_dir / "cov.info"
    )
    thresholds_path = (
        args.thresholds.resolve()
        if args.thresholds is not None
        else workspace / "docs" / "COVERAGE_THRESHOLDS.json"
    )

    if not info_file.exists():
        print(f"[coverage-guard] ERROR: coverage info missing: {info_file}")
        return 2
    if not thresholds_path.exists():
        print(f"[coverage-guard] ERROR: thresholds file missing: {thresholds_path}")
        return 3

    thresholds = load_json(thresholds_path).get("minimums", {})
    min_line = float(thresholds.get("line_pct", 0.0))
    min_branch = float(thresholds.get("branch_pct", 0.0))
    min_function = float(thresholds.get("function_pct", 0.0))

    totals = parse_lcov_info(info_file)
    line_found = int(totals["LF"])
    line_hit = int(totals["LH"])
    branch_found = int(totals["BRF"])
    branch_hit = int(totals["BRH"])
    fn_found = int(totals["FNF"])
    fn_hit = int(totals["FNH"])

    if line_found == 0 or fn_found == 0:
        print("[coverage-guard] ERROR: invalid coverage counters (line/function found == 0).")
        return 4

    line_pct = pct(line_hit, line_found)
    branch_pct = pct(branch_hit, branch_found) if branch_found > 0 else 100.0
    fn_pct = pct(fn_hit, fn_found)

    failures: list[str] = []
    if line_pct < min_line:
        failures.append(
            f"line coverage below threshold: {line_pct:.2f}% < {min_line:.2f}%"
        )
    if branch_pct < min_branch:
        failures.append(
            f"branch coverage below threshold: {branch_pct:.2f}% < {min_branch:.2f}%"
        )
    if fn_pct < min_function:
        failures.append(
            f"function coverage below threshold: {fn_pct:.2f}% < {min_function:.2f}%"
        )

    print(
        "[coverage-guard] Summary: "
        f"lines={line_pct:.2f}% ({line_hit}/{line_found}), "
        f"branches={branch_pct:.2f}% ({branch_hit}/{branch_found}), "
        f"functions={fn_pct:.2f}% ({fn_hit}/{fn_found})"
    )

    if failures:
        print("[coverage-guard] FAIL: coverage thresholds not met.")
        for item in failures:
            print(f"  - {item}")
        return 1

    print("[coverage-guard] PASS: coverage thresholds satisfied.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
