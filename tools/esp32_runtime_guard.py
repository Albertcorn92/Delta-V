#!/usr/bin/env python3
"""
esp32_runtime_guard.py

Run DELTA-V on ESP32 over serial, collect runtime scheduler metrics,
and enforce conservative WCET/stack thresholds.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import time
from collections import deque
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

METRIC_RE = re.compile(
    r"\[RGE_METRIC\]\s+"
    r"fast_tick_wcet_us=(\d+)\s+"
    r"loop_wcet_us=(\d+)\s+"
    r"loop_overruns=(\d+)\s+"
    r"stack_min_free_words=(\d+)"
)


@dataclass
class RuntimeSample:
    fast_tick_wcet_us: int
    loop_wcet_us: int
    loop_overruns: int
    stack_min_free_words: int



def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run DELTA-V ESP32 runtime guard with WCET and stack checks."
    )
    parser.add_argument("--project-dir", type=Path, default=Path("ports/esp32"))
    parser.add_argument("--build-dir", type=Path, default=Path("build_esp32"))
    parser.add_argument("--port", default="/dev/cu.usbmodem101")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--duration", type=float, default=300.0)
    parser.add_argument(
        "--thresholds",
        type=Path,
        default=Path("docs/ESP32_RUNTIME_THRESHOLDS.json"),
    )
    parser.add_argument("--max-rom-banners", type=int, default=3)
    parser.add_argument("--log-file", type=Path, default=None)
    parser.add_argument("--report-json", type=Path, default=None)
    parser.add_argument("--no-reset", action="store_true")
    return parser.parse_args()



def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))



def make_default_log_path() -> Path:
    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    return Path("artifacts") / f"esp32_runtime_guard_{ts}.log"



def make_default_report_path() -> Path:
    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    return Path("artifacts") / f"esp32_runtime_guard_{ts}.json"



def reset_esp32(ser: serial.Serial) -> None:
    ser.setDTR(False)
    ser.setRTS(True)
    time.sleep(0.08)
    ser.setRTS(False)
    time.sleep(0.12)
    ser.reset_input_buffer()



def main() -> int:
    args = parse_args()

    thresholds_path = args.thresholds.resolve()
    duration = max(args.duration, 1.0)

    if serial is None:
        print(
            "[esp32-runtime] ERROR: pyserial is not available in this Python environment.",
            file=sys.stderr,
        )
        return 2
    if not thresholds_path.exists():
        print(f"[esp32-runtime] ERROR: missing thresholds file: {thresholds_path}", file=sys.stderr)
        return 3

    thresholds = load_json(thresholds_path)
    minimums = thresholds.get("minimums", {})
    maximums = thresholds.get("maximums", {})

    min_samples = int(minimums.get("metric_samples", 1))
    min_stack_words = int(minimums.get("stack_min_free_words", 0))
    max_fast_wcet = int(maximums.get("fast_tick_wcet_us", 1_000_000))
    max_loop_wcet = int(maximums.get("loop_wcet_us", 1_000_000))
    max_overruns = int(maximums.get("loop_overruns", 0))

    log_path = args.log_file.resolve() if args.log_file else make_default_log_path().resolve()
    report_json = (
        args.report_json.resolve() if args.report_json else make_default_report_path().resolve()
    )
    log_path.parent.mkdir(parents=True, exist_ok=True)
    report_json.parent.mkdir(parents=True, exist_ok=True)

    required_found = {marker: False for marker in REQUIRED_MARKERS}
    samples: list[RuntimeSample] = []
    failures: set[str] = set()
    rom_banners = 0
    tail_lines: deque[str] = deque(maxlen=120)

    try:
        with serial.Serial(args.port, args.baud, timeout=0.25) as ser, log_path.open(
            "w", encoding="utf-8", errors="replace"
        ) as log_file:
            if not args.no_reset:
                reset_esp32(ser)

            deadline = time.monotonic() + duration
            while time.monotonic() < deadline:
                raw = ser.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                log_file.write(line + "\n")
                log_file.flush()

                lower = line.lower()
                tail_lines.append(line)
                if "esp-rom:" in line:
                    rom_banners += 1
                for marker in REQUIRED_MARKERS:
                    if marker in line:
                        required_found[marker] = True
                for fail in FAIL_MARKERS:
                    if fail in lower:
                        failures.add(fail)

                match = METRIC_RE.search(line)
                if match:
                    samples.append(
                        RuntimeSample(
                            fast_tick_wcet_us=int(match.group(1)),
                            loop_wcet_us=int(match.group(2)),
                            loop_overruns=int(match.group(3)),
                            stack_min_free_words=int(match.group(4)),
                        )
                    )
    except Exception as exc:
        print(f"[esp32-runtime] ERROR: serial monitor failed: {exc}", file=sys.stderr)
        print(f"[esp32-runtime] log: {log_path}", file=sys.stderr)
        return 4

    tail_excerpt = "\n".join(tail_lines) if tail_lines else "<no output captured>"

    if rom_banners > args.max_rom_banners:
        failures.add(f"reboot-loop: esp-rom banner count={rom_banners}")

    missing = [marker for marker, found in required_found.items() if not found]
    if missing:
        print("[esp32-runtime] ERROR: missing required runtime markers:", file=sys.stderr)
        for marker in missing:
            print(f"  - {marker}", file=sys.stderr)
        print(tail_excerpt, file=sys.stderr)
        print(f"[esp32-runtime] log: {log_path}", file=sys.stderr)
        return 5

    if len(samples) < min_samples:
        print(
            "[esp32-runtime] ERROR: insufficient RGE runtime metric samples "
            f"({len(samples)} < {min_samples}).",
            file=sys.stderr,
        )
        print(tail_excerpt, file=sys.stderr)
        print(f"[esp32-runtime] log: {log_path}", file=sys.stderr)
        return 6

    max_fast = max(sample.fast_tick_wcet_us for sample in samples)
    max_loop = max(sample.loop_wcet_us for sample in samples)
    last_overruns = samples[-1].loop_overruns
    min_stack = min(sample.stack_min_free_words for sample in samples)

    threshold_failures: list[str] = []
    if max_fast > max_fast_wcet:
        threshold_failures.append(
            f"fast_tick_wcet_us={max_fast} exceeds max={max_fast_wcet}"
        )
    if max_loop > max_loop_wcet:
        threshold_failures.append(
            f"loop_wcet_us={max_loop} exceeds max={max_loop_wcet}"
        )
    if last_overruns > max_overruns:
        threshold_failures.append(
            f"loop_overruns={last_overruns} exceeds max={max_overruns}"
        )
    if min_stack < min_stack_words:
        threshold_failures.append(
            f"stack_min_free_words={min_stack} below min={min_stack_words}"
        )

    if failures:
        print("[esp32-runtime] ERROR: failure markers detected:", file=sys.stderr)
        for marker in sorted(failures):
            print(f"  - {marker}", file=sys.stderr)
        print(tail_excerpt, file=sys.stderr)
        print(f"[esp32-runtime] log: {log_path}", file=sys.stderr)
        return 7

    payload: dict[str, Any] = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "duration_s": duration,
        "port": args.port,
        "samples": len(samples),
        "thresholds": {
            "min_samples": min_samples,
            "min_stack_words": min_stack_words,
            "max_fast_tick_wcet_us": max_fast_wcet,
            "max_loop_wcet_us": max_loop_wcet,
            "max_loop_overruns": max_overruns,
        },
        "summary": {
            "max_fast_tick_wcet_us": max_fast,
            "max_loop_wcet_us": max_loop,
            "last_loop_overruns": last_overruns,
            "min_stack_free_words": min_stack,
            "rom_banner_count": rom_banners,
        },
        "log_path": str(log_path),
    }
    report_json.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    if threshold_failures:
        print("[esp32-runtime] FAIL: runtime thresholds not met.", file=sys.stderr)
        for item in threshold_failures:
            print(f"  - {item}", file=sys.stderr)
        print(f"[esp32-runtime] log: {log_path}", file=sys.stderr)
        print(f"[esp32-runtime] report: {report_json}", file=sys.stderr)
        return 8

    print(
        "[esp32-runtime] PASS: "
        f"samples={len(samples)}, "
        f"max_fast_tick_wcet_us={max_fast}, "
        f"max_loop_wcet_us={max_loop}, "
        f"loop_overruns={last_overruns}, "
        f"stack_min_free_words={min_stack}"
    )
    print(f"[esp32-runtime] log: {log_path}")
    print(f"[esp32-runtime] report: {report_json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
