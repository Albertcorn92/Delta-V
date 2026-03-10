#!/usr/bin/env python3
"""
benchmark_trend_guard.py

Validate benchmark trend drift against a baseline profile.

This guard fails only on sustained regression:
- throughput-style metrics (`direction=min`) fail only when both median and p95
  are below the allowed floor.
- latency-style metrics (`direction=max`) fail only when both median and p95
  are above the allowed ceiling.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def resolve_metric_values(report: dict[str, Any], key: str) -> tuple[float, float] | None:
    run_summary = report.get("run_summary", [])
    for item in run_summary:
        if not isinstance(item, dict):
            continue
        if item.get("key") != key:
            continue
        median_value = item.get("median")
        p95_value = item.get("p95", median_value)
        if is_number(median_value) and is_number(p95_value):
            return float(median_value), float(p95_value)
        return None

    node: Any = report.get("metrics", {})
    for part in key.split("."):
        if not isinstance(node, dict) or part not in node:
            return None
        node = node[part]
    if not is_number(node):
        return None
    value = float(node)
    return value, value


def format_num(value: float) -> str:
    return f"{value:.3f}"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate DELTA-V benchmark trend drift against baseline profile."
    )
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument(
        "--baseline",
        type=Path,
        default=None,
        help="Path to benchmark trend baseline JSON (defaults to docs/BENCHMARK_TREND_BASELINE.json).",
    )
    args = parser.parse_args()

    workspace = args.workspace.resolve()
    build_dir = args.build_dir.resolve()
    baseline_path = (
        args.baseline.resolve()
        if args.baseline is not None
        else (workspace / "docs" / "BENCHMARK_TREND_BASELINE.json")
    )
    report_path = build_dir / "benchmark" / "benchmark_baseline.json"

    if not report_path.exists():
        print(f"[bench-trend] ERROR: missing benchmark report: {report_path}")
        return 2
    if not baseline_path.exists():
        print(f"[bench-trend] ERROR: missing trend baseline: {baseline_path}")
        return 3

    report = load_json(report_path)
    baseline = load_json(baseline_path)
    checks = baseline.get("checks", [])
    if not isinstance(checks, list) or not checks:
        print("[bench-trend] ERROR: baseline profile has no checks.")
        return 4

    failures: list[str] = []
    warnings: list[str] = []
    evaluated = 0

    for raw_check in checks:
        if not isinstance(raw_check, dict):
            failures.append("invalid check entry: expected object")
            continue

        key = str(raw_check.get("key", "")).strip()
        label = str(raw_check.get("label", key)).strip() or key
        direction = str(raw_check.get("direction", "")).strip().lower()

        baseline_value_raw = raw_check.get("baseline")
        max_regression_raw = raw_check.get("max_regression_pct", 0.0)
        if not key:
            failures.append("invalid check entry: missing key")
            continue
        if direction not in {"min", "max"}:
            failures.append(f"{label}: invalid direction '{direction}'")
            continue
        if not is_number(baseline_value_raw):
            failures.append(f"{label}: baseline is not numeric")
            continue
        if not is_number(max_regression_raw):
            failures.append(f"{label}: max_regression_pct is not numeric")
            continue

        baseline_value = float(baseline_value_raw)
        max_regression = float(max_regression_raw)
        if baseline_value <= 0.0:
            failures.append(f"{label}: baseline must be > 0")
            continue
        if max_regression < 0.0 or max_regression >= 1.0:
            failures.append(
                f"{label}: max_regression_pct must be in [0.0, 1.0), got {max_regression}"
            )
            continue

        values = resolve_metric_values(report, key)
        if values is None:
            failures.append(f"{label}: metric key not found in benchmark report ({key})")
            continue
        median_value, p95_value = values
        evaluated += 1

        if direction == "min":
            floor = baseline_value * (1.0 - max_regression)
            median_bad = median_value < floor
            p95_bad = p95_value < floor
            if median_bad and p95_bad:
                failures.append(
                    f"{label} sustained regression: median={format_num(median_value)}, "
                    f"p95={format_num(p95_value)}, min_allowed={format_num(floor)}"
                )
            elif median_bad or p95_bad:
                warnings.append(
                    f"{label} transient dip: median={format_num(median_value)}, "
                    f"p95={format_num(p95_value)}, min_allowed={format_num(floor)}"
                )
        else:
            ceiling = baseline_value * (1.0 + max_regression)
            median_bad = median_value > ceiling
            p95_bad = p95_value > ceiling
            if median_bad and p95_bad:
                failures.append(
                    f"{label} sustained regression: median={format_num(median_value)}, "
                    f"p95={format_num(p95_value)}, max_allowed={format_num(ceiling)}"
                )
            elif median_bad or p95_bad:
                warnings.append(
                    f"{label} transient spike: median={format_num(median_value)}, "
                    f"p95={format_num(p95_value)}, max_allowed={format_num(ceiling)}"
                )

    for warning in warnings:
        print(f"[bench-trend] WARN: {warning}")

    if failures:
        print("[bench-trend] FAIL: trend regression gate failed.")
        for failure in failures:
            print(f"  - {failure}")
        return 1

    profile = baseline.get("profile", "unknown")
    print(
        f"[bench-trend] PASS: sustained trend checks passed "
        f"({evaluated} metrics, profile={profile})."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
