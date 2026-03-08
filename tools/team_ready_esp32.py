#!/usr/bin/env python3
"""
team_ready_esp32.py

Run a practical host + ESP32 evidence sequence for DELTA-V team-readiness
closeout. This does not replace mission legal review/certification.
"""

from __future__ import annotations

import argparse
import json
import shlex
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run DELTA-V team-readiness host + ESP32 evidence flow."
    )
    parser.add_argument("--workspace", type=Path, default=Path("."))
    parser.add_argument("--python-exe", default=sys.executable)
    parser.add_argument("--port", default="")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--runtime-duration", type=float, default=300.0)
    parser.add_argument("--reboot-cycles", type=int, default=10)
    parser.add_argument("--reboot-cycle-seconds", type=float, default=12.0)
    parser.add_argument("--soak-duration", type=float, default=3600.0)
    parser.add_argument("--project-dir", type=Path, default=Path("ports/esp32"))
    parser.add_argument("--build-dir", type=Path, default=Path("build_esp32"))
    parser.add_argument("--skip-host-gates", action="store_true")
    parser.add_argument("--skip-scoped-report", action="store_true")
    parser.add_argument("--continue-on-fail", action="store_true")
    parser.add_argument("--output-json", type=Path, default=None)
    return parser.parse_args()


def discover_serial_ports() -> list[str]:
    dev = Path("/dev")
    patterns = [
        "cu.usb*",
        "tty.usb*",
        "cu.SLAB*",
        "tty.SLAB*",
        "cu.wch*",
        "tty.wch*",
    ]
    found: list[str] = []
    for pattern in patterns:
        found.extend(str(path) for path in dev.glob(pattern))
    return sorted(dict.fromkeys(found))


def resolve_port(raw_port: str) -> tuple[str, list[str]]:
    if raw_port.strip():
        return raw_port.strip(), []
    discovered = discover_serial_ports()
    if discovered:
        return discovered[0], discovered
    return "", []


def shell_join(argv: list[str]) -> str:
    return " ".join(shlex.quote(item) for item in argv)


def run_step(name: str, cmd: list[str], cwd: Path) -> dict[str, Any]:
    print(f"[team-ready] STEP: {name}")
    print(f"[team-ready] CMD : {shell_join(cmd)}")
    started = time.monotonic()
    result = subprocess.run(cmd, cwd=str(cwd))
    elapsed_s = round(time.monotonic() - started, 3)
    status = "PASS" if result.returncode == 0 else "FAIL"
    print(f"[team-ready] {name}: {status} ({elapsed_s:.3f}s)")
    return {
        "name": name,
        "status": status,
        "returncode": result.returncode,
        "elapsed_s": elapsed_s,
        "command": cmd,
    }


def build_output_path(path: Path | None, workspace: Path) -> Path:
    if path is not None:
        return path.resolve()
    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    return (workspace / "artifacts" / f"team_ready_esp32_{ts}.json").resolve()


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    args = parse_args()
    workspace = args.workspace.resolve()

    port, discovered = resolve_port(args.port)
    if not port:
        print(
            "[team-ready] ERROR: no serial port provided and no USB serial device found.",
            file=sys.stderr,
        )
        print(
            "[team-ready] Hint: plug in the board and rerun with --port /dev/cu.usbmodemXXX",
            file=sys.stderr,
        )
        return 2
    if discovered:
        print(f"[team-ready] Auto-selected serial port: {port}")
        if len(discovered) > 1:
            print("[team-ready] Other detected ports:")
            for candidate in discovered[1:]:
                print(f"  - {candidate}")

    output_json = build_output_path(args.output_json, workspace)
    output_json.parent.mkdir(parents=True, exist_ok=True)

    steps: list[dict[str, Any]] = []
    python_exe = args.python_exe
    project_dir = str(args.project_dir)
    build_dir = str(args.build_dir)

    if not args.skip_host_gates:
        steps.append(
            run_step(
                "software_final",
                ["cmake", "--build", "build", "--target", "software_final", "--", "-j1"],
                workspace,
            )
        )
        if steps[-1]["status"] != "PASS" and not args.continue_on_fail:
            payload = {
                "generated_utc": datetime.now(timezone.utc).isoformat(),
                "status": "FAIL",
                "reason": "software_final failed",
                "port": port,
                "steps": steps,
            }
            output_json.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
            print(f"[team-ready] report: {output_json}")
            return 3

    steps.append(
        run_step(
            "esp32_runtime_guard",
            [
                python_exe,
                "tools/esp32_runtime_guard.py",
                "--project-dir",
                project_dir,
                "--build-dir",
                build_dir,
                "--port",
                port,
                "--baud",
                str(args.baud),
                "--duration",
                str(max(args.runtime_duration, 1.0)),
            ],
            workspace,
        )
    )
    if steps[-1]["status"] != "PASS" and not args.continue_on_fail:
        payload = {
            "generated_utc": datetime.now(timezone.utc).isoformat(),
            "status": "FAIL",
            "reason": "esp32_runtime_guard failed",
            "port": port,
            "steps": steps,
        }
        output_json.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        print(f"[team-ready] report: {output_json}")
        return 4

    steps.append(
        run_step(
            "esp32_reboot_campaign",
            [
                python_exe,
                "tools/esp32_reboot_campaign.py",
                "--project-dir",
                project_dir,
                "--build-dir",
                build_dir,
                "--port",
                port,
                "--baud",
                str(args.baud),
                "--cycles",
                str(max(args.reboot_cycles, 1)),
                "--cycle-seconds",
                str(max(args.reboot_cycle_seconds, 1.0)),
            ],
            workspace,
        )
    )
    if steps[-1]["status"] != "PASS" and not args.continue_on_fail:
        payload = {
            "generated_utc": datetime.now(timezone.utc).isoformat(),
            "status": "FAIL",
            "reason": "esp32_reboot_campaign failed",
            "port": port,
            "steps": steps,
        }
        output_json.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        print(f"[team-ready] report: {output_json}")
        return 5

    steps.append(
        run_step(
            "esp32_soak",
            [
                python_exe,
                "tools/esp32_soak.py",
                "--project-dir",
                project_dir,
                "--build-dir",
                build_dir,
                "--port",
                port,
                "--baud",
                str(args.baud),
                "--duration",
                str(max(args.soak_duration, 1.0)),
            ],
            workspace,
        )
    )
    if steps[-1]["status"] != "PASS" and not args.continue_on_fail:
        payload = {
            "generated_utc": datetime.now(timezone.utc).isoformat(),
            "status": "FAIL",
            "reason": "esp32_soak failed",
            "port": port,
            "steps": steps,
        }
        output_json.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        print(f"[team-ready] report: {output_json}")
        return 6

    steps.append(
        run_step(
            "cubesat_readiness",
            ["cmake", "--build", "build", "--target", "cubesat_readiness", "--", "-j1"],
            workspace,
        )
    )

    if not args.skip_scoped_report:
        steps.append(
            run_step(
                "cubesat_readiness_scope",
                ["cmake", "--build", "build", "--target", "cubesat_readiness_scope", "--", "-j1"],
                workspace,
            )
        )

    readiness_json = workspace / "docs" / "CUBESAT_READINESS_STATUS.json"
    readiness_scope_json = workspace / "docs" / "CUBESAT_READINESS_STATUS_SCOPE.json"
    readiness_payload = load_json(readiness_json) if readiness_json.exists() else {}
    readiness_scope_payload = (
        load_json(readiness_scope_json) if readiness_scope_json.exists() else {}
    )

    all_pass = all(step["status"] == "PASS" for step in steps)
    payload = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "PASS" if all_pass else "FAIL",
        "port": port,
        "steps": steps,
        "readiness": {
            "framework_release_ready": readiness_payload.get("framework_release_ready"),
            "cubesat_flight_ready": readiness_payload.get("cubesat_flight_ready"),
        },
        "scope_readiness": {
            "framework_release_ready": readiness_scope_payload.get("framework_release_ready"),
            "cubesat_flight_ready": readiness_scope_payload.get("cubesat_flight_ready"),
        },
    }
    output_json.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    print(f"[team-ready] report: {output_json}")
    if payload["readiness"]["cubesat_flight_ready"] is True:
        print("[team-ready] Unwaived CubeSat readiness is TRUE.")
    else:
        print(
            "[team-ready] Unwaived CubeSat readiness is not TRUE yet. "
            "Check docs/CUBESAT_READINESS_STATUS.md remaining gaps."
        )

    return 0 if all_pass else 7


if __name__ == "__main__":
    raise SystemExit(main())
