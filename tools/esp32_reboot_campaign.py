#!/usr/bin/env python3
"""
esp32_reboot_campaign.py

Run repeated short serial windows with hardware reset pulses to collect reboot
startup stability statistics for DELTA-V local-only runtime.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

try:
    import serial  # type: ignore
except Exception:  # pragma: no cover - runtime dependency check
    serial = None


REQUIRED_MARKERS = [
    "[Boot] HAL: ESP32 I2C hardware driver",
    "[RadioLink] Local-only mode: UDP bridge disabled.",
    "[RGE] Embedded cooperative scheduler running.",
]

FAIL_MARKERS = [
    "guru meditation",
    "assert failed",
    "stack overflow",
    "abort() was called",
    "panic",
]


@dataclass
class CycleResult:
    cycle_index: int
    pass_cycle: bool
    reason: str
    rom_banner_count: int



def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run DELTA-V ESP32 reboot startup stability campaign."
    )
    parser.add_argument("--project-dir", type=Path, default=Path("ports/esp32"))
    parser.add_argument("--build-dir", type=Path, default=Path("build_esp32"))
    parser.add_argument("--port", default="/dev/cu.usbmodem101")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--cycles", type=int, default=10)
    parser.add_argument("--cycle-seconds", type=float, default=12.0)
    parser.add_argument("--between-seconds", type=float, default=1.0)
    parser.add_argument("--max-rom-banners", type=int, default=3)
    parser.add_argument("--log-file", type=Path, default=None)
    parser.add_argument("--report-json", type=Path, default=None)
    return parser.parse_args()



def make_default_log_path() -> Path:
    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    return Path("artifacts") / f"esp32_reboot_campaign_{ts}.log"



def make_default_report_path() -> Path:
    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    return Path("artifacts") / f"esp32_reboot_campaign_{ts}.json"



def reset_esp32(ser: serial.Serial) -> None:
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



def run_cycle(
    *,
    cycle_index: int,
    ser: serial.Serial,
    cycle_seconds: float,
    max_rom_banners: int,
    log_handle,
) -> CycleResult:
    required_found = {marker: False for marker in REQUIRED_MARKERS}
    failures: set[str] = set()
    rom_banners = 0
    pending = bytearray()

    reset_esp32(ser)

    deadline = time.monotonic() + max(cycle_seconds, 1.0)
    while time.monotonic() < deadline:
        lines = read_available_lines(ser, pending)
        if not lines:
            time.sleep(0.01)
            continue
        for line in lines:
            lower = line.lower()
            log_handle.write(f"[cycle {cycle_index:03d}] {line}\n")
            log_handle.flush()

            if "esp-rom:" in line:
                rom_banners += 1
            for marker in REQUIRED_MARKERS:
                if marker in line:
                    required_found[marker] = True
            for marker in FAIL_MARKERS:
                if marker in lower:
                    failures.add(marker)

    if rom_banners > max_rom_banners:
        return CycleResult(
            cycle_index=cycle_index,
            pass_cycle=False,
            reason=f"reboot-loop indicator (esp-rom banners={rom_banners})",
            rom_banner_count=rom_banners,
        )

    missing = [marker for marker, found in required_found.items() if not found]
    if missing:
        return CycleResult(
            cycle_index=cycle_index,
            pass_cycle=False,
            reason="missing required marker(s): " + ", ".join(missing),
            rom_banner_count=rom_banners,
        )

    if failures:
        return CycleResult(
            cycle_index=cycle_index,
            pass_cycle=False,
            reason="failure marker(s): " + ", ".join(sorted(failures)),
            rom_banner_count=rom_banners,
        )

    return CycleResult(
        cycle_index=cycle_index,
        pass_cycle=True,
        reason="PASS",
        rom_banner_count=rom_banners,
    )



def main() -> int:
    args = parse_args()

    if serial is None:
        print(
            "[esp32-reboot] ERROR: pyserial is not available in this Python environment.",
            file=sys.stderr,
        )
        return 2

    cycles = max(args.cycles, 1)
    cycle_seconds = max(args.cycle_seconds, 1.0)
    between_seconds = max(args.between_seconds, 0.0)

    log_path = args.log_file.resolve() if args.log_file else make_default_log_path().resolve()
    report_json = (
        args.report_json.resolve() if args.report_json else make_default_report_path().resolve()
    )
    log_path.parent.mkdir(parents=True, exist_ok=True)
    report_json.parent.mkdir(parents=True, exist_ok=True)

    results: list[CycleResult] = []

    try:
        with serial.Serial(args.port, args.baud, timeout=0) as ser, log_path.open(
            "w", encoding="utf-8", errors="replace"
        ) as log_handle:
            log_handle.write(
                f"# DELTA-V ESP32 reboot campaign start: {datetime.now(timezone.utc).isoformat()}\n"
            )
            log_handle.flush()

            for cycle in range(1, cycles + 1):
                print(f"[esp32-reboot] cycle {cycle}/{cycles} ...")
                result = run_cycle(
                    cycle_index=cycle,
                    ser=ser,
                    cycle_seconds=cycle_seconds,
                    max_rom_banners=args.max_rom_banners,
                    log_handle=log_handle,
                )
                results.append(result)

                if result.pass_cycle:
                    print(
                        "[esp32-reboot] PASS "
                        f"cycle={result.cycle_index} rom_banners={result.rom_banner_count}"
                    )
                else:
                    print(
                        "[esp32-reboot] FAIL "
                        f"cycle={result.cycle_index} reason={result.reason}",
                        file=sys.stderr,
                    )

                if cycle < cycles and between_seconds > 0.0:
                    time.sleep(between_seconds)
    except Exception as exc:
        print(f"[esp32-reboot] ERROR: serial monitor failed: {exc}", file=sys.stderr)
        print(f"[esp32-reboot] log: {log_path}", file=sys.stderr)
        return 3

    pass_count = sum(1 for item in results if item.pass_cycle)
    fail_count = len(results) - pass_count

    payload: dict[str, Any] = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "port": args.port,
        "cycles": cycles,
        "cycle_seconds": cycle_seconds,
        "between_seconds": between_seconds,
        "summary": {
            "pass_cycles": pass_count,
            "fail_cycles": fail_count,
            "pass_rate_pct": (100.0 * pass_count / cycles),
        },
        "results": [
            {
                "cycle": item.cycle_index,
                "status": "PASS" if item.pass_cycle else "FAIL",
                "reason": item.reason,
                "rom_banner_count": item.rom_banner_count,
            }
            for item in results
        ],
        "log_path": str(log_path),
    }
    report_json.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    if fail_count > 0:
        print(
            f"[esp32-reboot] FAIL: {fail_count}/{cycles} cycle(s) failed.",
            file=sys.stderr,
        )
        print(f"[esp32-reboot] log: {log_path}", file=sys.stderr)
        print(f"[esp32-reboot] report: {report_json}", file=sys.stderr)
        return 4

    print(
        f"[esp32-reboot] PASS: {pass_count}/{cycles} cycles passed "
        f"({100.0 * pass_count / cycles:.1f}%)."
    )
    print(f"[esp32-reboot] log: {log_path}")
    print(f"[esp32-reboot] report: {report_json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
