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
    cmd_router = metrics.get("command_router", {})
    telem_fanout = metrics.get("telem_fanout", {})
    startup_runtime = metrics.get("startup_runtime", {})

    failures: list[str] = []

    frames = int(uplink.get("frames", 0))
    accepted = int(uplink.get("accepted", 0))
    consumed = int(uplink.get("consumed", 0))
    if accepted != frames:
        failures.append(f"uplink accepted mismatch: accepted={accepted}, frames={frames}")
    if consumed != accepted:
        failures.append(f"uplink consumed mismatch: consumed={consumed}, accepted={accepted}")

    router_commands = int(cmd_router.get("commands", 0))
    router_injected = int(cmd_router.get("injected", 0))
    router_routed = int(cmd_router.get("routed", 0))
    router_ack_events = int(cmd_router.get("ack_events", 0))
    if router_injected != router_commands:
        failures.append(
            f"command_router injected mismatch: injected={router_injected}, commands={router_commands}"
        )
    if router_routed != router_injected:
        failures.append(
            f"command_router routed mismatch: routed={router_routed}, injected={router_injected}"
        )
    if router_ack_events != router_routed:
        failures.append(
            f"command_router ack mismatch: ack_events={router_ack_events}, routed={router_routed}"
        )

    fanout_packets = int(telem_fanout.get("packets", 0))
    fanout_injected = int(telem_fanout.get("injected", 0))
    fanout_delivered_a = int(telem_fanout.get("delivered_listener_a", 0))
    fanout_delivered_b = int(telem_fanout.get("delivered_listener_b", 0))
    if fanout_injected != fanout_packets:
        failures.append(
            f"telem_fanout injected mismatch: injected={fanout_injected}, packets={fanout_packets}"
        )
    if fanout_delivered_a != fanout_injected:
        failures.append(
            "telem_fanout listener_a mismatch: "
            f"delivered_listener_a={fanout_delivered_a}, injected={fanout_injected}"
        )
    if fanout_delivered_b != fanout_injected:
        failures.append(
            "telem_fanout listener_b mismatch: "
            f"delivered_listener_b={fanout_delivered_b}, injected={fanout_injected}"
        )

    runtime_ready_seen = bool(startup_runtime.get("ready_seen", False))
    runtime_sampling_supported = bool(startup_runtime.get("sampling_supported", False))
    runtime_sample_count = int(startup_runtime.get("sample_count", 0))
    if not runtime_ready_seen:
        failures.append("startup_runtime ready marker was not observed")
    if runtime_sampling_supported:
        if runtime_sample_count < int(
            thresholds.get("startup_runtime_sample_count_min", 1)
        ):
            failures.append(
                "startup_runtime sample count below threshold: "
                f"value={runtime_sample_count}, "
                f"min={int(thresholds.get('startup_runtime_sample_count_min', 1))}"
            )

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
    check_min(
        "command router throughput (cmd/s)",
        float(cmd_router.get("throughput_cmd_per_s", 0.0)),
        float(thresholds.get("command_router_throughput_cmd_per_s_min", 0.0)),
    )
    check_max(
        "command router latency p95 (us)",
        float(cmd_router.get("latency_p95_us", 0.0)),
        float(thresholds.get("command_router_latency_p95_us_max", float("inf"))),
    )
    check_min(
        "telem fanout throughput (pkt/s)",
        float(telem_fanout.get("throughput_pkt_per_s", 0.0)),
        float(thresholds.get("telem_fanout_throughput_pkt_per_s_min", 0.0)),
    )
    check_max(
        "telem fanout latency p95 (us)",
        float(telem_fanout.get("latency_p95_us", 0.0)),
        float(thresholds.get("telem_fanout_latency_p95_us_max", float("inf"))),
    )
    check_max(
        "startup time to ready (s)",
        float(startup_runtime.get("startup_time_to_ready_s", float("inf"))),
        float(thresholds.get("startup_runtime_startup_time_to_ready_s_max", float("inf"))),
    )
    if runtime_sampling_supported:
        check_max(
            "runtime CPU p95 (%)",
            float(startup_runtime.get("cpu_p95_pct", float("inf"))),
            float(thresholds.get("startup_runtime_cpu_p95_pct_max", float("inf"))),
        )
        check_max(
            "runtime RSS p95 (MB)",
            float(startup_runtime.get("rss_p95_mb", float("inf"))),
            float(thresholds.get("startup_runtime_rss_p95_mb_max", float("inf"))),
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
