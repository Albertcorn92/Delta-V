#!/usr/bin/env python3
"""
sitl_smoke.py

Run a short local SITL smoke test and validate basic runtime health markers.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
import time
from pathlib import Path


REQUIRED_MARKERS = [
    "[Topology] All ports connected.",
]

RGE_MARKERS = [
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
]

def tail_excerpt(text: str, lines: int = 40) -> str:
    split = text.splitlines()
    if not split:
        return "<no output captured>"
    return "\n".join(split[-lines:])


def main() -> int:
    parser = argparse.ArgumentParser(description="Run DELTA-V SITL smoke test.")
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--duration", type=float, default=8.0)
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    flight_bin = build_dir / "flight_software"
    if not flight_bin.exists():
        print(f"[sitl-smoke] ERROR: missing binary: {flight_bin}", file=sys.stderr)
        return 2

    with tempfile.NamedTemporaryFile(prefix="deltav_sitl_smoke_", suffix=".log", delete=False) as log:
        log_path = Path(log.name)
        proc = subprocess.Popen(
            [str(flight_bin)],
            cwd=str(build_dir),
            stdout=log,
            stderr=subprocess.STDOUT,
            text=True,
        )

    stopped_by_smoke = False
    try:
        time.sleep(max(args.duration, 1.0))
    finally:
        if proc.poll() is None:
            stopped_by_smoke = True
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    print("[sitl-smoke] ERROR: process did not exit after kill.", file=sys.stderr)
                    return 5

    output = log_path.read_text(encoding="utf-8", errors="ignore")
    log_path.unlink(missing_ok=True)

    if not stopped_by_smoke:
        print(
            f"[sitl-smoke] ERROR: flight_software exited before {args.duration:.1f}s "
            f"(code={proc.returncode}).",
            file=sys.stderr,
        )
        print(tail_excerpt(output), file=sys.stderr)
        return 6

    if proc.returncode not in (-15, -9, 0):
        print(f"[sitl-smoke] ERROR: unexpected process exit code: {proc.returncode}", file=sys.stderr)
        print(tail_excerpt(output), file=sys.stderr)
        return 7

    missing = [m for m in REQUIRED_MARKERS if m not in output]
    if not any(marker in output for marker in RGE_MARKERS):
        missing.append("RGE startup marker")

    if missing:
        print("[sitl-smoke] ERROR: missing required startup markers:", file=sys.stderr)
        for marker in missing:
            print(f"  - {marker}", file=sys.stderr)
        print(tail_excerpt(output), file=sys.stderr)
        return 3

    failures = [m for m in FAIL_MARKERS if m in output]
    if failures:
        print("[sitl-smoke] ERROR: failure markers detected:", file=sys.stderr)
        for marker in failures:
            print(f"  - {marker}", file=sys.stderr)
        print(tail_excerpt(output), file=sys.stderr)
        return 4

    print(f"[sitl-smoke] PASS: ran {args.duration:.1f}s, markers present, no fatal signatures.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
