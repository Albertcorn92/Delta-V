#!/usr/bin/env python3
"""
sitl_soak.py

Run an extended local SITL soak and validate runtime stability markers.
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


def main() -> int:
    parser = argparse.ArgumentParser(description="Run DELTA-V extended SITL soak.")
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--duration", type=float, default=180.0)
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    flight_bin = build_dir / "flight_software"
    if not flight_bin.exists():
        print(f"[sitl-soak] ERROR: missing binary: {flight_bin}", file=sys.stderr)
        return 2

    with tempfile.NamedTemporaryFile(prefix="deltav_sitl_soak_", suffix=".log", delete=False) as log:
        log_path = Path(log.name)
        proc = subprocess.Popen(
            [str(flight_bin)],
            cwd=str(build_dir),
            stdout=log,
            stderr=subprocess.STDOUT,
            text=True,
        )

    early_exit = False
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
                return 5

    output = log_path.read_text(encoding="utf-8", errors="ignore")
    log_path.unlink(missing_ok=True)

    if early_exit:
        print(
            f"[sitl-soak] ERROR: early process exit before soak duration (code={proc.returncode}).",
            file=sys.stderr,
        )
        print(tail_excerpt(output), file=sys.stderr)
        return 6

    missing = [m for m in REQUIRED_MARKERS if m not in output]
    if not any(marker in output for marker in RUNTIME_MARKERS):
        missing.append("RGE runtime marker")
    if missing:
        print("[sitl-soak] ERROR: missing required runtime markers:", file=sys.stderr)
        for marker in missing:
            print(f"  - {marker}", file=sys.stderr)
        print(tail_excerpt(output), file=sys.stderr)
        return 3

    failures = [m for m in FAIL_MARKERS if m in output]
    if failures:
        print("[sitl-soak] ERROR: failure markers detected:", file=sys.stderr)
        for marker in failures:
            print(f"  - {marker}", file=sys.stderr)
        print(tail_excerpt(output), file=sys.stderr)
        return 4

    print(f"[sitl-soak] PASS: ran {args.duration:.1f}s with no fatal signatures.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
