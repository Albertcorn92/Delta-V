#!/usr/bin/env python3
"""
sitl_soak.py

Run an extended local SITL soak and validate runtime stability markers.
Emit deterministic log/JSON artifacts for CI archival.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


REQUIRED_MARKERS = [
    "[Topology] All ports connected.",
]

RUNTIME_MARKERS = [
    "[RGE] All rate groups running.",
    "[RGE] Embedded cooperative scheduler running.",
    "[RGE] Tiers ready - FAST=",
]

FAIL_MARKERS = [
    "Segmentation fault",
    "AddressSanitizer",
    "FATAL: unhandled exception",
    "stack overflow",
    "Guru Meditation",
    "assert failed",
    "[HeapGuard] FATAL",
]


def tail_excerpt(text: str, lines: int = 60) -> str:
    split = text.splitlines()
    if not split:
        return "<no output captured>"
    return "\n".join(split[-lines:])


def write_artifact(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run DELTA-V extended SITL soak.")
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--duration", type=float, default=600.0)
    parser.add_argument(
        "--output-json",
        type=Path,
        help="Path to soak result JSON artifact (default: <build-dir>/sitl/sitl_soak_result.json)",
    )
    parser.add_argument(
        "--output-log",
        type=Path,
        help="Path to captured soak log (default: <build-dir>/sitl/sitl_soak.log)",
    )
    args = parser.parse_args()

    start = time.monotonic()
    build_dir = args.build_dir.resolve()
    output_json = (args.output_json or (build_dir / "sitl" / "sitl_soak_result.json")).resolve()
    output_log = (args.output_log or (build_dir / "sitl" / "sitl_soak.log")).resolve()
    flight_bin = build_dir / "flight_software"

    result = {
        "gate": "sitl_soak",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "FAIL",
        "build_dir": str(build_dir),
        "flight_binary": str(flight_bin),
        "duration_requested_s": float(args.duration),
        "duration_actual_s": 0.0,
        "early_exit": False,
        "process_exit_code": None,
        "missing_markers": [],
        "failure_markers": [],
        "log_path": str(output_log),
        "reason": "",
        "return_code": 1,
    }

    if not flight_bin.exists():
        print(f"[sitl-soak] ERROR: missing binary: {flight_bin}", file=sys.stderr)
        result["reason"] = f"missing binary: {flight_bin}"
        result["return_code"] = 2
        result["duration_actual_s"] = round(time.monotonic() - start, 3)
        write_artifact(output_json, result)
        return 2

    output_log.parent.mkdir(parents=True, exist_ok=True)
    with output_log.open("w", encoding="utf-8") as log:
        proc = subprocess.Popen(
            [str(flight_bin)],
            cwd=str(build_dir),
            stdout=log,
            stderr=subprocess.STDOUT,
            text=True,
        )

    early_exit = False
    return_code = 0
    t_end = time.monotonic() + max(args.duration, 1.0)
    while time.monotonic() < t_end:
        rc = proc.poll()
        if rc is not None:
            early_exit = True
            break
        time.sleep(0.5)

    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                print("[sitl-soak] ERROR: process did not terminate.", file=sys.stderr)
                result["reason"] = "process did not terminate after kill"
                return_code = 5

    output = output_log.read_text(encoding="utf-8", errors="ignore")

    if return_code == 0 and early_exit:
        print(
            f"[sitl-soak] ERROR: early process exit before soak duration (code={proc.returncode}).",
            file=sys.stderr,
        )
        print(tail_excerpt(output), file=sys.stderr)
        result["reason"] = "early process exit before requested soak duration"
        return_code = 6

    missing = [m for m in REQUIRED_MARKERS if m not in output]
    if not any(marker in output for marker in RUNTIME_MARKERS):
        missing.append("RGE runtime marker")
    if return_code == 0 and missing:
        print("[sitl-soak] ERROR: missing required runtime markers:", file=sys.stderr)
        for marker in missing:
            print(f"  - {marker}", file=sys.stderr)
        print(tail_excerpt(output), file=sys.stderr)
        result["reason"] = "missing required runtime markers"
        return_code = 3

    failures = [m for m in FAIL_MARKERS if m in output]
    if return_code == 0 and failures:
        print("[sitl-soak] ERROR: failure markers detected:", file=sys.stderr)
        for marker in failures:
            print(f"  - {marker}", file=sys.stderr)
        print(tail_excerpt(output), file=sys.stderr)
        result["reason"] = "fatal marker detected in runtime output"
        return_code = 4

    result["duration_actual_s"] = round(time.monotonic() - start, 3)
    result["early_exit"] = early_exit
    result["process_exit_code"] = proc.returncode
    result["missing_markers"] = missing
    result["failure_markers"] = failures
    result["return_code"] = return_code
    if return_code == 0:
        result["status"] = "PASS"
        result["reason"] = "markers present and no fatal signatures"
    write_artifact(output_json, result)

    if return_code == 0:
        print(f"[sitl-soak] PASS: ran {args.duration:.1f}s with no fatal signatures.")
        print(f"[sitl-soak] Artifacts: {output_json} , {output_log}")
    return return_code


if __name__ == "__main__":
    raise SystemExit(main())
