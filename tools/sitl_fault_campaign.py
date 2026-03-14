#!/usr/bin/env python3
"""
sitl_fault_campaign.py

Run a local SITL fault campaign against the live UDP uplink path. The campaign
mixes valid, malformed, and replayed frames and verifies that the process stays
alive, rejects the bad traffic, and still dispatches valid commands.
"""

from __future__ import annotations

import argparse
import json
import os
import random
import socket
import struct
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


CCSDS_SYNC_WORD = 0x1ACFFC1D
APID_COMMAND = 30
COMMAND_PACKET_SIZE = 12
UPLINK_FRAME_SIZE = 22
DEFAULT_STARTUP_WAIT = 5.0
STARTUP_MARKERS = [
    "[Topology] All ports connected.",
]
RUNTIME_MARKERS = [
    "[RGE] All rate groups running.",
    "[RGE] Embedded cooperative scheduler running.",
    "[RGE] Tiers ready - FAST=",
]
TRAFFIC_READY_MARKERS = [
    "[Topology] All ports connected.",
    "Bridge online.",
    "[RGE] All rate groups running.",
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
LOG_EVIDENCE_MARKERS = {
    "valid_dispatch": "[RadioLink] CMD seq=100 op=2 -> ID 200",
    "replay_reject": "[RadioLink] Rejected replayed command seq=100",
    "battery_drain_update": "[BatterySystem] Drain rate set to 0.500%/tick",
    "battery_reset": "[BatterySystem] SOC reset to 100%",
    "payload_capture_stdout": "[PayloadMonitor] Sample captured value=",
}

EVENT_LOG_MARKERS = {
    "payload_enabled": "PAYLOAD: Enabled",
    "payload_gain_updated": "PAYLOAD: Gain updated",
    "payload_sample_captured": "PAYLOAD: Sample captured",
    "unknown_target_nack": "NACK: ID999 not found",
    "battery_unknown_opcode": "BATT: Unknown opcode",
}


def write_artifact(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def build_frame(
    seq: int,
    target_id: int,
    opcode: int,
    argument: float,
    *,
    sync_word: int = CCSDS_SYNC_WORD,
    apid: int = APID_COMMAND,
    payload_len: int = COMMAND_PACKET_SIZE,
) -> bytes:
    return struct.pack(
        "<IHHHIIf",
        sync_word,
        apid,
        seq & 0xFFFF,
        payload_len,
        target_id,
        opcode,
        argument,
    )


def reserve_udp_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run a local DELTA-V SITL fault campaign against the live UDP uplink."
    )
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument(
        "--uplink-port",
        type=int,
        default=0,
        help="UDP uplink port to target. Defaults to an auto-reserved free local port.",
    )
    parser.add_argument(
        "--downlink-port",
        type=int,
        default=0,
        help="UDP downlink port for the SITL bridge. Defaults to an auto-reserved free local port.",
    )
    parser.add_argument("--startup-wait", type=float, default=DEFAULT_STARTUP_WAIT)
    parser.add_argument("--settle-seconds", type=float, default=3.0)
    parser.add_argument("--random-invalid-count", type=int, default=32)
    parser.add_argument(
        "--output-json",
        type=Path,
        help="Path to campaign result JSON artifact (default: <build-dir>/sitl/sitl_fault_campaign_result.json)",
    )
    parser.add_argument(
        "--output-log",
        type=Path,
        help="Path to captured campaign log (default: <build-dir>/sitl/sitl_fault_campaign.log)",
    )
    args = parser.parse_args()

    start = time.monotonic()
    build_dir = args.build_dir.resolve()
    flight_bin = build_dir / "flight_software"
    output_json = (
        args.output_json
        or (build_dir / "sitl" / "sitl_fault_campaign_result.json")
    ).resolve()
    output_log = (
        args.output_log
        or (build_dir / "sitl" / "sitl_fault_campaign.log")
    ).resolve()
    replay_file = build_dir / "sitl" / "fault_campaign_replay_seq.db"
    event_log_path = build_dir / "flight_events.log"
    telem_log_path = build_dir / "flight_log.csv"
    replay_file.parent.mkdir(parents=True, exist_ok=True)
    output_log.parent.mkdir(parents=True, exist_ok=True)
    if replay_file.exists():
        replay_file.unlink()
    for stale in (event_log_path, telem_log_path):
        if stale.exists():
            stale.unlink()

    uplink_port = (
        int(args.uplink_port)
        if args.uplink_port > 0
        else reserve_udp_port()
    )
    downlink_port = (
        int(args.downlink_port)
        if args.downlink_port > 0
        else reserve_udp_port()
    )
    while downlink_port == uplink_port:
        downlink_port = reserve_udp_port()

    result = {
        "gate": "sitl_fault_campaign",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "FAIL",
        "build_dir": str(build_dir),
        "flight_binary": str(flight_bin),
        "uplink_port": uplink_port,
        "downlink_port": downlink_port,
        "startup_wait_s": float(args.startup_wait),
        "settle_seconds": float(args.settle_seconds),
        "random_invalid_count": int(args.random_invalid_count),
        "duration_actual_s": 0.0,
        "process_exit_code": None,
        "startup_markers_ok": False,
        "runtime_marker_found": False,
        "failure_markers": [],
        "log_checks": {},
        "event_log_checks": {},
        "sent_frames": {},
        "log_path": str(output_log),
        "event_log_path": str(event_log_path),
        "reason": "",
        "return_code": 1,
    }

    if not flight_bin.exists():
        result["reason"] = f"missing binary: {flight_bin}"
        result["return_code"] = 2
        result["duration_actual_s"] = round(time.monotonic() - start, 3)
        write_artifact(output_json, result)
        print(f"[sitl-fault] ERROR: missing binary: {flight_bin}", file=sys.stderr)
        return 2

    env = os.environ.copy()
    env["DELTAV_ENABLE_UNAUTH_UPLINK"] = "1"
    env["DELTAV_UPLINK_ALLOW_IP"] = "127.0.0.1"
    env["DELTAV_REPLAY_SEQ_FILE"] = str(replay_file)
    env["DELTAV_UPLINK_PORT"] = str(uplink_port)
    env["DELTAV_DOWNLINK_PORT"] = str(downlink_port)

    log_handle = output_log.open("w", encoding="utf-8")
    proc = subprocess.Popen(
        [str(flight_bin)],
        cwd=str(build_dir),
        stdout=log_handle,
        stderr=subprocess.STDOUT,
        env=env,
    )

    try:
        startup_deadline = time.monotonic() + max(args.startup_wait, 0.5)
        bridge_ready = False
        while time.monotonic() < startup_deadline:
            if proc.poll() is not None:
                break
            if output_log.exists():
                partial = output_log.read_text(encoding="utf-8", errors="ignore")
                if all(marker in partial for marker in TRAFFIC_READY_MARKERS):
                    bridge_ready = True
                    break
            time.sleep(0.1)

        if proc.poll() is not None:
            result["reason"] = (
                f"flight_software exited before campaign traffic began (code={proc.returncode})"
            )
            result["return_code"] = 3
            return 3

        # Output is redirected to a file, so stdout may be block-buffered on some
        # hosts. If markers are not yet visible but the process is still alive
        # after the full startup window, continue with the campaign.

        valid_drain = build_frame(100, 200, 2, 0.5)
        replay_drain = valid_drain
        malformed_sync = build_frame(101, 200, 2, 0.25, sync_word=0xDEADBEEF)
        malformed_apid = build_frame(102, 200, 2, 0.25, apid=20)
        malformed_len = build_frame(103, 200, 2, 0.25, payload_len=COMMAND_PACKET_SIZE - 1)
        valid_reset = build_frame(104, 200, 1, 0.0)
        payload_enable = build_frame(105, 400, 1, 1.0)
        payload_gain = build_frame(106, 400, 3, 2.0)
        payload_capture = build_frame(107, 400, 2, 4.0)
        unknown_target = build_frame(108, 999, 1, 0.0)
        invalid_battery_opcode = build_frame(109, 200, 99, 0.0)

        rng = random.Random(0xD37AFA17)
        random_invalid = [
            bytes(rng.getrandbits(8) for _ in range(rng.randint(0, UPLINK_FRAME_SIZE + 8)))
            for _ in range(max(args.random_invalid_count, 0))
        ]

        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            for frame in (
                valid_drain,
                replay_drain,
                valid_reset,
                payload_enable,
                payload_gain,
                payload_capture,
                unknown_target,
                invalid_battery_opcode,
                malformed_sync,
                malformed_apid,
                malformed_len,
            ):
                sock.sendto(frame, ("127.0.0.1", uplink_port))
                time.sleep(0.15)
            for blob in random_invalid:
                sock.sendto(blob, ("127.0.0.1", uplink_port))

        result["sent_frames"] = {
            "valid_drain": 1,
            "replay_duplicate": 1,
            "malformed_sync": 1,
            "malformed_apid": 1,
            "malformed_length": 1,
            "valid_reset": 1,
            "payload_enable": 1,
            "payload_gain": 1,
            "payload_capture": 1,
            "unknown_target": 1,
            "invalid_battery_opcode": 1,
            "random_invalid": len(random_invalid),
        }

        settle_deadline = time.monotonic() + max(args.settle_seconds, 1.0)
        while time.monotonic() < settle_deadline:
            if proc.poll() is not None:
                break
            time.sleep(0.1)

        if proc.poll() is not None:
            result["reason"] = (
                f"flight_software exited during fault campaign (code={proc.returncode})"
            )
            result["return_code"] = 5
            return 5

        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                result["reason"] = "process did not exit after termination"
                result["return_code"] = 6
                return 6

        log_handle.flush()
        combined = output_log.read_text(encoding="utf-8") if output_log.exists() else ""
        result["startup_markers_ok"] = all(
            marker in combined for marker in STARTUP_MARKERS
        )
        result["runtime_marker_found"] = any(
            marker in combined for marker in RUNTIME_MARKERS
        )
        if not result["startup_markers_ok"] or not result["runtime_marker_found"]:
            result["reason"] = "expected startup/runtime markers missing from captured log"
            result["return_code"] = 4
            return 4

        failures = [marker for marker in FAIL_MARKERS if marker in combined]
        result["failure_markers"] = failures
        if failures:
            result["reason"] = "fatal marker detected in runtime output"
            result["return_code"] = 7
            return 7

        log_checks = {
            label: marker in combined for label, marker in LOG_EVIDENCE_MARKERS.items()
        }
        result["log_checks"] = log_checks
        if not all(log_checks.values()):
            result["reason"] = "campaign log evidence incomplete"
            result["return_code"] = 8
            return 8

        events_text = (
            event_log_path.read_text(encoding="utf-8", errors="ignore")
            if event_log_path.exists()
            else ""
        )
        event_log_checks = {
            label: marker in events_text for label, marker in EVENT_LOG_MARKERS.items()
        }
        result["event_log_checks"] = event_log_checks
        if not all(event_log_checks.values()):
            result["reason"] = "campaign event-log evidence incomplete"
            result["return_code"] = 9
            return 9

        result["status"] = "PASS"
        result["reason"] = "fault campaign completed and valid commands survived malformed traffic"
        result["return_code"] = 0
        return 0
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    pass
        log_handle.flush()
        log_handle.close()
        result["process_exit_code"] = proc.returncode
        result["duration_actual_s"] = round(time.monotonic() - start, 3)
        write_artifact(output_json, result)
        if result["return_code"] == 0:
            print(
                "[sitl-fault] PASS: malformed/replay traffic rejected and valid commands dispatched."
            )
            print(f"[sitl-fault] Artifacts: {output_json} , {output_log}")


if __name__ == "__main__":
    raise SystemExit(main())
