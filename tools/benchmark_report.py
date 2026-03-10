#!/usr/bin/env python3
"""
benchmark_report.py

Run DELTA-V software-side benchmarks and generate reproducible baseline artifacts.
"""

from __future__ import annotations

import argparse
import json
import platform
import shutil
import subprocess
import sys
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
    out_dir: Path,
    runs: int,
) -> tuple[dict[str, Any], Path, Path, list[dict[str, Any]]]:
    samples: list[dict[str, Any]] = []
    for idx in range(runs):
        sample_json = out_dir / f"benchmark_metrics_run_{idx + 1}.json"
        print(f"[benchmark] run {idx + 1}/{runs}")
        samples.append(run_benchmark_binary(binary, sample_json))

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
    parser.add_argument("--no-sync-docs", action="store_true")
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be >= 1")

    workspace = args.workspace.resolve()
    build_dir = args.build_dir.resolve()
    sync_docs = not args.no_sync_docs

    benchmark_bin = build_dir / "run_benchmarks"
    require_file(benchmark_bin)

    out_dir = build_dir / "benchmark"
    out_dir.mkdir(parents=True, exist_ok=True)
    metrics, _metrics_json, runs_json, samples = run_benchmark_samples(
        benchmark_bin,
        out_dir,
        args.runs,
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
