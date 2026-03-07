#!/usr/bin/env python3
"""
benchmark_guard.py

Validate software benchmark metrics against conservative regression thresholds.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate DELTA-V benchmark metrics against thresholds."
    )
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument(
        "--thresholds",
        type=Path,
        default=None,
        help="Path to benchmark thresholds JSON (defaults to docs/BENCHMARK_THRESHOLDS.json).",
    )
    args = parser.parse_args()

    workspace = args.workspace.resolve()
    build_dir = args.build_dir.resolve()
    thresholds_path = (
        args.thresholds.resolve()
        if args.thresholds is not None
        else (workspace / "docs" / "BENCHMARK_THRESHOLDS.json")
    )
    metrics_path = build_dir / "benchmark" / "benchmark_metrics.json"

    if not metrics_path.exists():
        print(f"[bench-guard] ERROR: missing metrics file: {metrics_path}")
        return 2
    if not thresholds_path.exists():
        print(f"[bench-guard] ERROR: missing thresholds file: {thresholds_path}")
        return 3

    metrics = load_json(metrics_path)
    thresholds = load_json(thresholds_path).get("metrics", {})

    uplink = metrics.get("uplink", {})
    crc16 = metrics.get("crc16", {})
    cobs = metrics.get("cobs_roundtrip", {})

    failures: list[str] = []

    frames = int(uplink.get("frames", 0))
    accepted = int(uplink.get("accepted", 0))
    consumed = int(uplink.get("consumed", 0))
    if accepted != frames:
        failures.append(f"uplink accepted mismatch: accepted={accepted}, frames={frames}")
    if consumed != accepted:
        failures.append(f"uplink consumed mismatch: consumed={consumed}, accepted={accepted}")

    def check_min(label: str, value: float, threshold: float) -> None:
        if value < threshold:
            failures.append(
                f"{label} below threshold: value={value:.3f}, min={threshold:.3f}"
            )

    def check_max(label: str, value: float, threshold: float) -> None:
        if value > threshold:
            failures.append(
                f"{label} above threshold: value={value:.3f}, max={threshold:.3f}"
            )

    check_min(
        "uplink throughput (cmd/s)",
        float(uplink.get("throughput_cmd_per_s", 0.0)),
        float(thresholds.get("uplink_throughput_cmd_per_s_min", 0.0)),
    )
    check_max(
        "uplink latency p95 (us)",
        float(uplink.get("latency_p95_us", 0.0)),
        float(thresholds.get("uplink_latency_p95_us_max", float("inf"))),
    )
    check_min(
        "crc16 throughput (MB/s)",
        float(crc16.get("throughput_mb_per_s", 0.0)),
        float(thresholds.get("crc16_throughput_mb_per_s_min", 0.0)),
    )
    check_min(
        "cobs throughput (MB/s)",
        float(cobs.get("throughput_mb_per_s", 0.0)),
        float(thresholds.get("cobs_roundtrip_throughput_mb_per_s_min", 0.0)),
    )

    if failures:
        print("[bench-guard] FAIL: benchmark regression gate failed.")
        for item in failures:
            print(f"  - {item}")
        return 1

    print("[bench-guard] PASS: benchmark metrics satisfy regression thresholds.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
