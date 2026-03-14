#!/usr/bin/env python3
"""
benchmark_report.py

Run DELTA-V software-side benchmarks and generate reproducible baseline artifacts.
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path
from statistics import median
from typing import Any


def run_cmd(cmd: list[str], cwd: Path) -> str:
    try:
        return subprocess.check_output(
            cmd,
            cwd=str(cwd),
            stderr=subprocess.STDOUT,
            text=True,
        ).strip()
    except Exception:
        return "unavailable"


def require_file(path: Path) -> None:
    if not path.exists():
        raise FileNotFoundError(f"missing required file: {path}")


def run_benchmark_binary(binary: Path, metrics_json: Path) -> dict[str, Any]:
    cmd = [str(binary), "--json", str(metrics_json)]
    result = subprocess.run(cmd, check=False, capture_output=True, text=True)
    if result.returncode != 0:
        if result.stdout:
            print(result.stdout, file=sys.stdout)
        if result.stderr:
            print(result.stderr, file=sys.stderr)
        result.check_returncode()
    return json.loads(metrics_json.read_text(encoding="utf-8"))


def sample_process_stats_procfs(
    pid: int,
    prev_jiffies: int | None,
    prev_time_s: float | None,
    clock_ticks_per_sec: float,
    page_size_bytes: float,
) -> tuple[float, float, int, float] | None:
    stat_path = Path("/proc") / str(pid) / "stat"
    if not stat_path.exists():
        return None

    raw = stat_path.read_text(encoding="utf-8", errors="replace").strip()
    if not raw:
        return None

    right = raw.rsplit(") ", maxsplit=1)
    if len(right) != 2:
        return None
    fields = right[1].split()
    if len(fields) < 22:
        return None

    try:
        utime_jiffies = int(fields[11])
        stime_jiffies = int(fields[12])
        rss_pages = int(fields[21])
    except ValueError:
        return None

    current_jiffies = utime_jiffies + stime_jiffies
    now_s = time.monotonic()
    rss_mb = (float(rss_pages) * page_size_bytes) / (1024.0 * 1024.0)

    if prev_jiffies is None or prev_time_s is None:
        cpu_pct = 0.0
    else:
        delta_jiffies = current_jiffies - prev_jiffies
        delta_time_s = max(1e-6, now_s - prev_time_s)
        cpu_pct = ((float(delta_jiffies) / clock_ticks_per_sec) / delta_time_s) * 100.0
        cpu_pct = max(0.0, cpu_pct)

    return cpu_pct, rss_mb, current_jiffies, now_s


def sample_process_stats_ps(pid: int) -> tuple[float, float] | None:
    cmd = ["ps", "-p", str(pid), "-o", "%cpu=", "-o", "rss="]
    try:
        result = subprocess.run(cmd, check=False, capture_output=True, text=True)
    except (OSError, PermissionError, subprocess.SubprocessError):
        return None
    if result.returncode != 0:
        return None

    lines = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    if not lines:
        return None

    fields = lines[0].split()
    if len(fields) < 2:
        return None

    try:
        cpu_pct = max(0.0, float(fields[0]))
        rss_kb = float(fields[1])
    except ValueError:
        return None

    rss_mb = rss_kb / 1024.0
    return cpu_pct, rss_mb


def can_sample_process_stats_ps() -> bool:
    ps_path = shutil.which("ps")
    if ps_path is None:
        return False
    return sample_process_stats_ps(os.getpid()) is not None


def scan_log_for_markers(
    log_path: Path,
    cursor: int,
    ready_markers: list[str],
) -> tuple[bool, int]:
    if not log_path.exists():
        return False, cursor
    with log_path.open("r", encoding="utf-8", errors="replace") as f:
        f.seek(cursor)
        chunk = f.read()
        new_cursor = f.tell()
    for marker in ready_markers:
        if marker in chunk:
            return True, new_cursor
    return False, new_cursor


def summarize(values: list[float]) -> tuple[float, float, float]:
    if not values:
        return 0.0, 0.0, 0.0
    return (
        percentile(values, 0.50),
        percentile(values, 0.95),
        max(values),
    )


def run_runtime_profile(
    flight_binary: Path,
    duration_s: float,
    sample_interval_s: float,
) -> dict[str, Any]:
    ready_markers = [
        "[Topology] All ports connected.",
        "[Scheduler] All systems nominal.",
    ]

    with tempfile.NamedTemporaryFile(prefix="deltav_runtime_profile_", suffix=".log", delete=False) as tmp:
        log_path = Path(tmp.name)

    start = time.monotonic()
    cursor = 0
    ready_seen = False
    ready_time = 0.0
    cpu_samples: list[float] = []
    rss_samples: list[float] = []
    procfs_supported = Path("/proc").exists()
    ps_supported = can_sample_process_stats_ps()
    sampling_supported = procfs_supported or ps_supported
    clock_ticks_per_sec = float(os.sysconf("SC_CLK_TCK")) if procfs_supported else 0.0
    page_size_bytes = float(os.sysconf("SC_PAGE_SIZE")) if procfs_supported else 0.0
    prev_jiffies: int | None = None
    prev_sample_time_s: float | None = None

    log_file = log_path.open("w", encoding="utf-8")
    proc = subprocess.Popen(
        [str(flight_binary)],
        cwd=str(flight_binary.parent),
        stdout=log_file,
        stderr=subprocess.STDOUT,
        text=True,
    )

    try:
        while True:
            elapsed = time.monotonic() - start
            found_marker, cursor = scan_log_for_markers(log_path, cursor, ready_markers)
            if found_marker and not ready_seen:
                ready_seen = True
                ready_time = elapsed

            if procfs_supported:
                sample = sample_process_stats_procfs(
                    proc.pid,
                    prev_jiffies,
                    prev_sample_time_s,
                    clock_ticks_per_sec,
                    page_size_bytes,
                )
                if sample is not None:
                    cpu_pct, rss_mb, prev_jiffies, prev_sample_time_s = sample
                    cpu_samples.append(cpu_pct)
                    rss_samples.append(rss_mb)
            elif ps_supported:
                sample = sample_process_stats_ps(proc.pid)
                if sample is not None:
                    cpu_pct, rss_mb = sample
                    cpu_samples.append(cpu_pct)
                    rss_samples.append(rss_mb)

            if elapsed >= duration_s:
                break
            if proc.poll() is not None:
                break

            time.sleep(max(0.05, sample_interval_s))
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=2.0)
        log_file.flush()
        found_marker, cursor = scan_log_for_markers(log_path, cursor, ready_markers)
        if found_marker and not ready_seen:
            ready_seen = True
            ready_time = min(duration_s, time.monotonic() - start)
        log_file.close()

    cpu_p50, cpu_p95, cpu_max = summarize(cpu_samples)
    rss_p50, rss_p95, rss_max = summarize(rss_samples)

    startup_time = ready_time if ready_seen else (duration_s + sample_interval_s)

    try:
        log_path.unlink(missing_ok=True)
    except Exception:
        pass

    return {
        "ready_seen": ready_seen,
        "sampling_supported": sampling_supported,
        "ready_markers": ready_markers,
        "window_duration_s": duration_s,
        "sample_interval_s": sample_interval_s,
        "sample_count": len(cpu_samples),
        "startup_time_to_ready_s": startup_time,
        "cpu_p50_pct": cpu_p50,
        "cpu_p95_pct": cpu_p95,
        "cpu_max_pct": cpu_max,
        "rss_p50_mb": rss_p50,
        "rss_p95_mb": rss_p95,
        "rss_max_mb": rss_max,
    }


def percentile(values: list[float], pct: float) -> float:
    if not values:
        return 0.0
    sorted_values = sorted(values)
    clamped = max(0.0, min(1.0, pct))
    idx = int(clamped * float(len(sorted_values) - 1))
    return sorted_values[idx]


def aggregate_metrics(samples: list[dict[str, Any]]) -> dict[str, Any]:
    def aggregate_node(values: list[Any]) -> Any:
        if not values:
            return None

        first = values[0]
        if isinstance(first, dict):
            out: dict[str, Any] = {}
            for key in first:
                children = [
                    value[key]
                    for value in values
                    if isinstance(value, dict) and key in value
                ]
                out[key] = aggregate_node(children)
            return out

        if isinstance(first, (int, float)) and not isinstance(first, bool):
            numerics = [
                float(value)
                for value in values
                if isinstance(value, (int, float)) and not isinstance(value, bool)
            ]
            if not numerics:
                return first
            med = float(median(numerics))
            if isinstance(first, int):
                return int(med + 0.5)
            return med

        return first

    return aggregate_node(samples)


RUN_SUMMARY_METRICS: list[tuple[str, tuple[str, ...]]] = [
    ("Uplink throughput (cmd/s)", ("uplink", "throughput_cmd_per_s")),
    ("Uplink latency p95 (us)", ("uplink", "latency_p95_us")),
    ("CRC-16 throughput (MB/s)", ("crc16", "throughput_mb_per_s")),
    ("COBS roundtrip throughput (MB/s)", ("cobs_roundtrip", "throughput_mb_per_s")),
    ("Command router throughput (cmd/s)", ("command_router", "throughput_cmd_per_s")),
    ("Command router latency p95 (us)", ("command_router", "latency_p95_us")),
    ("Telem fanout throughput (pkt/s)", ("telem_fanout", "throughput_pkt_per_s")),
    ("Telem fanout latency p95 (us)", ("telem_fanout", "latency_p95_us")),
    ("Startup time to ready (s)", ("startup_runtime", "startup_time_to_ready_s")),
    ("Runtime CPU p95 (%)", ("startup_runtime", "cpu_p95_pct")),
    ("Runtime RSS p95 (MB)", ("startup_runtime", "rss_p95_mb")),
]


def metric_value(metrics: dict[str, Any], path: tuple[str, ...]) -> float | None:
    node: Any = metrics
    for part in path:
        if not isinstance(node, dict) or part not in node:
            return None
        node = node[part]
    if isinstance(node, (int, float)) and not isinstance(node, bool):
        return float(node)
    return None


def build_run_summary(samples: list[dict[str, Any]]) -> list[dict[str, Any]]:
    summary: list[dict[str, Any]] = []
    for label, path in RUN_SUMMARY_METRICS:
        values = [
            value
            for value in (metric_value(sample, path) for sample in samples)
            if value is not None
        ]
        if not values:
            continue
        summary.append(
            {
                "label": label,
                "key": ".".join(path),
                "median": float(median(values)),
                "p95": percentile(values, 0.95),
                "min": min(values),
                "max": max(values),
            }
        )
    return summary


def run_benchmark_samples(
    binary: Path,
    flight_binary: Path,
    out_dir: Path,
    runs: int,
    runtime_duration_s: float,
    runtime_sample_interval_s: float,
) -> tuple[dict[str, Any], Path, Path, list[dict[str, Any]]]:
    samples: list[dict[str, Any]] = []
    for idx in range(runs):
        sample_json = out_dir / f"benchmark_metrics_run_{idx + 1}.json"
        print(f"[benchmark] run {idx + 1}/{runs}")
        sample = run_benchmark_binary(binary, sample_json)
        sample["startup_runtime"] = run_runtime_profile(
            flight_binary,
            runtime_duration_s,
            runtime_sample_interval_s,
        )
        samples.append(sample)

    aggregated = aggregate_metrics(samples)
    metrics_json = out_dir / "benchmark_metrics.json"
    metrics_json.write_text(json.dumps(aggregated, indent=2) + "\n", encoding="utf-8")

    runs_json = out_dir / "benchmark_runs.json"
    runs_payload = {
        "run_count": runs,
        "aggregation": "median",
        "metrics_aggregated": aggregated,
        "runs": samples,
    }
    runs_json.write_text(json.dumps(runs_payload, indent=2) + "\n", encoding="utf-8")

    return aggregated, metrics_json, runs_json, samples


def render_markdown(report: dict[str, Any]) -> str:
    uplink = report["metrics"]["uplink"]
    crc16 = report["metrics"]["crc16"]
    cobs = report["metrics"]["cobs_roundtrip"]
    cmd_router = report["metrics"]["command_router"]
    telem_fanout = report["metrics"]["telem_fanout"]
    startup_runtime = report["metrics"]["startup_runtime"]
    run_summary = report.get("run_summary", [])
    lines = [
        "# DELTA-V Benchmark Baseline",
        "",
        f"- Generated (UTC): `{report['generated_utc']}`",
        f"- Git commit: `{report['git']['commit']}`",
        f"- Git branch: `{report['git']['branch']}`",
        f"- Dirty worktree: `{report['git']['dirty_worktree']}`",
        f"- Host: `{report['host']['platform']}`",
        f"- Benchmark runs: `{report['benchmark_runs']}`",
        "- Aggregation: `per-metric median across benchmark runs`",
        f"- Raw run artifact: `{report['run_metrics_artifact']}`",
        "",
        "## DELTA-V Software Metrics",
        "",
        "| Metric | Value |",
        "|---|---|",
        f"| Uplink throughput (cmd/s) | `{uplink['throughput_cmd_per_s']:.3f}` |",
        f"| Uplink latency p50 (us) | `{uplink['latency_p50_us']:.3f}` |",
        f"| Uplink latency p95 (us) | `{uplink['latency_p95_us']:.3f}` |",
        f"| CRC-16 throughput (MB/s) | `{crc16['throughput_mb_per_s']:.3f}` |",
        f"| COBS roundtrip throughput (MB/s) | `{cobs['throughput_mb_per_s']:.3f}` |",
        f"| Command router throughput (cmd/s) | `{cmd_router['throughput_cmd_per_s']:.3f}` |",
        f"| Command router latency p95 (us) | `{cmd_router['latency_p95_us']:.3f}` |",
        f"| Telem fanout throughput (pkt/s) | `{telem_fanout['throughput_pkt_per_s']:.3f}` |",
        f"| Telem fanout latency p95 (us) | `{telem_fanout['latency_p95_us']:.3f}` |",
        f"| Startup time to ready (s) | `{startup_runtime['startup_time_to_ready_s']:.3f}` |",
        f"| Runtime sampling supported | `{startup_runtime['sampling_supported']}` |",
        f"| Runtime sample count | `{startup_runtime['sample_count']}` |",
        f"| Runtime CPU p95 (%) | `{startup_runtime['cpu_p95_pct']:.3f}` |",
        f"| Runtime RSS p95 (MB) | `{startup_runtime['rss_p95_mb']:.3f}` |",
        "",
    ]

    if run_summary:
        lines.extend(
            [
                "## Run Stability Snapshot",
                "",
                "| Metric | Median | p95 | Min | Max |",
                "|---|---|---|---|---|",
            ]
        )
        for item in run_summary:
            lines.append(
                f"| {item['label']} | `{item['median']:.3f}` | `{item['p95']:.3f}` | `{item['min']:.3f}` | `{item['max']:.3f}` |"
            )
        lines.append("")

    lines.extend(
        [
            "## Comparison Template (Fill with External Baselines)",
            "",
            "| Metric | DELTA-V | F Prime (external data) |",
            "|---|---|---|",
            f"| Uplink throughput (cmd/s) | `{uplink['throughput_cmd_per_s']:.3f}` | `N/A` |",
            f"| Uplink latency p95 (us) | `{uplink['latency_p95_us']:.3f}` | `N/A` |",
            f"| CRC-16 throughput (MB/s) | `{crc16['throughput_mb_per_s']:.3f}` | `N/A` |",
            f"| COBS roundtrip throughput (MB/s) | `{cobs['throughput_mb_per_s']:.3f}` | `N/A` |",
            f"| Command router throughput (cmd/s) | `{cmd_router['throughput_cmd_per_s']:.3f}` | `N/A` |",
            f"| Command router latency p95 (us) | `{cmd_router['latency_p95_us']:.3f}` | `N/A` |",
            f"| Telem fanout throughput (pkt/s) | `{telem_fanout['throughput_pkt_per_s']:.3f}` | `N/A` |",
            f"| Telem fanout latency p95 (us) | `{telem_fanout['latency_p95_us']:.3f}` | `N/A` |",
            f"| Startup time to ready (s) | `{startup_runtime['startup_time_to_ready_s']:.3f}` | `N/A` |",
            f"| Runtime CPU p95 (%) | `{startup_runtime['cpu_p95_pct']:.3f}` | `N/A` |",
            f"| Runtime RSS p95 (MB) | `{startup_runtime['rss_p95_mb']:.3f}` | `N/A` |",
            "",
            "## Reproducibility",
            "",
            "- Build command: `cmake --build build --target run_benchmarks`",
            "- Run command: `cmake --build build --target benchmark_baseline`",
            "- Method reference: `docs/BENCHMARK_PROTOCOL.md`",
            "",
            "Benchmark artifacts are software-only evidence and do not replace on-target HIL qualification.",
            "",
        ]
    )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate DELTA-V benchmark baseline artifacts."
    )
    parser.add_argument("--workspace", required=True, type=Path)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument(
        "--runs",
        required=False,
        type=int,
        default=3,
        help="Number of benchmark runs to aggregate (default: 3).",
    )
    parser.add_argument(
        "--runtime-duration-s",
        required=False,
        type=float,
        default=5.0,
        help="Duration in seconds for flight runtime CPU/RSS/startup sampling per run.",
    )
    parser.add_argument(
        "--runtime-sample-interval-s",
        required=False,
        type=float,
        default=0.20,
        help="Sampling interval in seconds for runtime CPU/RSS metrics.",
    )
    parser.add_argument("--no-sync-docs", action="store_true")
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be >= 1")
    if args.runtime_duration_s <= 0.0:
        parser.error("--runtime-duration-s must be > 0")
    if args.runtime_sample_interval_s <= 0.0:
        parser.error("--runtime-sample-interval-s must be > 0")

    workspace = args.workspace.resolve()
    build_dir = args.build_dir.resolve()
    sync_docs = not args.no_sync_docs

    benchmark_bin = build_dir / "run_benchmarks"
    flight_bin = build_dir / "flight_software"
    require_file(benchmark_bin)
    require_file(flight_bin)

    out_dir = build_dir / "benchmark"
    out_dir.mkdir(parents=True, exist_ok=True)
    metrics, _metrics_json, runs_json, samples = run_benchmark_samples(
        benchmark_bin,
        flight_bin,
        out_dir,
        args.runs,
        args.runtime_duration_s,
        args.runtime_sample_interval_s,
    )
    run_summary = build_run_summary(samples)

    report = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "workspace": workspace.name,
        "host": {
            "platform": platform.platform(),
            "python": platform.python_version(),
            "cmake": run_cmd(["cmake", "--version"], workspace).splitlines()[0],
        },
        "git": {
            "branch": run_cmd(["git", "rev-parse", "--abbrev-ref", "HEAD"], workspace),
            "commit": run_cmd(["git", "rev-parse", "HEAD"], workspace),
            "dirty_worktree": bool(run_cmd(["git", "status", "--porcelain"], workspace)),
        },
        "benchmark_runs": args.runs,
        "run_metrics_artifact": str(runs_json.relative_to(workspace)),
        "metrics": metrics,
        "run_summary": run_summary,
    }

    report_json = out_dir / "benchmark_baseline.json"
    report_md = out_dir / "benchmark_baseline.md"
    report_json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    report_md.write_text(render_markdown(report), encoding="utf-8")

    if sync_docs:
        docs_dir = workspace / "docs"
        docs_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy2(report_json, docs_dir / "BENCHMARK_BASELINE.json")
        shutil.copy2(report_md, docs_dir / "BENCHMARK_BASELINE.md")

    print(f"[benchmark] OK: baseline report -> {report_md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
