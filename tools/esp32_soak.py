#!/usr/bin/env python3
"""
esp32_soak.py

Run an ESP32 sensorless soak over serial and validate runtime health markers.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from collections import deque
from datetime import datetime, timezone
from pathlib import Path

try:
    import serial  # type: ignore
except Exception:  # pragma: no cover - runtime dependency check
    serial = None


REQUIRED_MARKERS = [
    "[Boot] HAL: ESP32 I2C hardware driver",
    "[RadioLink] Local-only mode: UDP bridge disabled.",
    "[IMU_01] WARN: Hardware IMU missing. Using simulation fallback.",
]

RUNTIME_MARKERS = [
    "[RGE] Embedded cooperative scheduler running.",
    "[RGE] Tiers ready - FAST=",
]

FAIL_MARKERS = [
    "guru meditation",
    "assert failed",
    "stack overflow",
    "abort() was called",
    "panic",
]



def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run DELTA-V ESP32 sensorless soak and validate health markers."
    )
    parser.add_argument("--project-dir", type=Path, default=Path("ports/esp32"))
    parser.add_argument("--build-dir", type=Path, default=Path("build_esp32"))
    parser.add_argument("--port", default="/dev/cu.usbmodem101")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--duration", type=float, default=600.0)
    parser.add_argument("--max-rom-banners", type=int, default=3)
    parser.add_argument("--log-file", type=Path, default=None)
    parser.add_argument("--report-json", type=Path, default=None)
    parser.add_argument("--no-reset", action="store_true")
    return parser.parse_args()



def make_default_log_path() -> Path:
    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    return Path("artifacts") / f"esp32_soak_{ts}.log"


def write_report(path: Path, payload: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")



def reset_esp32(ser: serial.Serial) -> None:
    # Typical reset pulse for ESP32 boards: keep IO0 high (DTR low), pulse EN via RTS.
    ser.setDTR(False)
    ser.setRTS(True)
    time.sleep(0.08)
    ser.setRTS(False)
    time.sleep(0.12)
    ser.reset_input_buffer()


def read_available_lines(ser: serial.Serial, pending: bytearray) -> list[str]:
    chunk = ser.read(max(ser.in_waiting, 1))
    if not chunk:
        return []

    pending.extend(chunk)
    lines: list[str] = []
    while True:
        newline_index = pending.find(b"\n")
        if newline_index < 0:
            break
        raw = bytes(pending[: newline_index + 1])
        del pending[: newline_index + 1]
        lines.append(raw.decode("utf-8", errors="replace").rstrip("\r\n"))
    return lines



def main() -> int:
    args = parse_args()
    duration = max(args.duration, 1.0)

    if serial is None:
        print(
            "[esp32-soak] ERROR: pyserial is not available in this Python environment.",
            file=sys.stderr,
        )
        return 2

    log_path = args.log_file.resolve() if args.log_file else make_default_log_path().resolve()
    log_path.parent.mkdir(parents=True, exist_ok=True)
    report_path = args.report_json.resolve() if args.report_json else log_path.with_suffix(".json")

    required_found = {marker: False for marker in REQUIRED_MARKERS}
    runtime_seen = False
    failures: set[str] = set()
    rom_banners = 0
    tail_lines: deque[str] = deque(maxlen=120)
    pending = bytearray()

    try:
        with serial.Serial(args.port, args.baud, timeout=0) as ser, log_path.open(
            "w", encoding="utf-8", errors="replace"
        ) as log_file:
            if not args.no_reset:
                reset_esp32(ser)

            deadline = time.monotonic() + duration
            while time.monotonic() < deadline:
                lines = read_available_lines(ser, pending)
                if not lines:
                    time.sleep(0.01)
                    continue
                for line in lines:
                    log_file.write(line + "\n")
                    log_file.flush()

                    lower = line.lower()
                    tail_lines.append(line)
                    if "esp-rom:" in line:
                        rom_banners += 1
                    for marker in REQUIRED_MARKERS:
                        if marker in line:
                            required_found[marker] = True
                    if any(marker in line for marker in RUNTIME_MARKERS):
                        runtime_seen = True
                    for marker in FAIL_MARKERS:
                        if marker in lower:
                            failures.add(marker)
    except Exception as exc:
        write_report(
            report_path,
            {
                "generated_utc": datetime.now(timezone.utc).isoformat(),
                "duration_s": duration,
                "port": args.port,
                "baud": args.baud,
                "status": "ERROR",
                "reason": f"serial monitor failed: {exc}",
                "required_markers": required_found,
                "runtime_seen": runtime_seen,
                "rom_banner_count": rom_banners,
                "failures": sorted(failures),
                "max_rom_banners": args.max_rom_banners,
                "log_path": str(log_path),
            },
        )
        print(f"[esp32-soak] ERROR: serial monitor failed: {exc}", file=sys.stderr)
        print(f"[esp32-soak] log: {log_path}", file=sys.stderr)
        print(f"[esp32-soak] report: {report_path}", file=sys.stderr)
        return 3

    tail_excerpt = "\n".join(tail_lines) if tail_lines else "<no output captured>"

    if rom_banners > args.max_rom_banners:
        failures.add(f"reboot-loop: esp-rom banner count={rom_banners}")

    missing = [marker for marker, found in required_found.items() if not found]
    if not runtime_seen:
        missing.append("RGE runtime marker")
    if missing:
        write_report(
            report_path,
            {
                "generated_utc": datetime.now(timezone.utc).isoformat(),
                "duration_s": duration,
                "port": args.port,
                "baud": args.baud,
                "status": "FAIL",
                "reason": "missing required runtime markers",
                "missing_markers": missing,
                "required_markers": required_found,
                "runtime_seen": runtime_seen,
                "rom_banner_count": rom_banners,
                "failures": sorted(failures),
                "max_rom_banners": args.max_rom_banners,
                "log_path": str(log_path),
            },
        )
        print("[esp32-soak] ERROR: missing required runtime markers:", file=sys.stderr)
        for marker in missing:
            print(f"  - {marker}", file=sys.stderr)
        print(tail_excerpt, file=sys.stderr)
        print(f"[esp32-soak] log: {log_path}", file=sys.stderr)
        print(f"[esp32-soak] report: {report_path}", file=sys.stderr)
        return 4

    if failures:
        write_report(
            report_path,
            {
                "generated_utc": datetime.now(timezone.utc).isoformat(),
                "duration_s": duration,
                "port": args.port,
                "baud": args.baud,
                "status": "FAIL",
                "reason": "failure markers detected",
                "required_markers": required_found,
                "runtime_seen": runtime_seen,
                "rom_banner_count": rom_banners,
                "failures": sorted(failures),
                "max_rom_banners": args.max_rom_banners,
                "log_path": str(log_path),
            },
        )
        print("[esp32-soak] ERROR: failure markers detected:", file=sys.stderr)
        for marker in sorted(failures):
            print(f"  - {marker}", file=sys.stderr)
        print(tail_excerpt, file=sys.stderr)
        print(f"[esp32-soak] log: {log_path}", file=sys.stderr)
        print(f"[esp32-soak] report: {report_path}", file=sys.stderr)
        return 5

    write_report(
        report_path,
        {
            "generated_utc": datetime.now(timezone.utc).isoformat(),
            "duration_s": duration,
            "port": args.port,
            "baud": args.baud,
            "status": "PASS",
            "reason": "healthy runtime markers",
            "required_markers": required_found,
            "runtime_seen": runtime_seen,
            "rom_banner_count": rom_banners,
            "failures": sorted(failures),
            "max_rom_banners": args.max_rom_banners,
            "log_path": str(log_path),
        },
    )
    print(f"[esp32-soak] PASS: ran {duration:.1f}s with healthy runtime markers.")
    print(f"[esp32-soak] log: {log_path}")
    print(f"[esp32-soak] report: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
