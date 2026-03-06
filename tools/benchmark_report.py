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
    subprocess.run(cmd, check=True)
    return json.loads(metrics_json.read_text(encoding="utf-8"))


def render_markdown(report: dict[str, Any]) -> str:
    uplink = report["metrics"]["uplink"]
    crc16 = report["metrics"]["crc16"]
    cobs = report["metrics"]["cobs_roundtrip"]
    return "\n".join(
        [
            "# DELTA-V Benchmark Baseline",
            "",
            f"- Generated (UTC): `{report['generated_utc']}`",
            f"- Git commit: `{report['git']['commit']}`",
            f"- Git branch: `{report['git']['branch']}`",
            f"- Dirty worktree: `{report['git']['dirty_worktree']}`",
            f"- Host: `{report['host']['platform']}`",
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
            "",
            "## Comparison Template (Fill with External Baselines)",
            "",
            "| Metric | DELTA-V | F Prime (fill) |",
            "|---|---|---|",
            f"| Uplink throughput (cmd/s) | `{uplink['throughput_cmd_per_s']:.3f}` | `TBD` |",
            f"| Uplink latency p95 (us) | `{uplink['latency_p95_us']:.3f}` | `TBD` |",
            f"| CRC-16 throughput (MB/s) | `{crc16['throughput_mb_per_s']:.3f}` | `TBD` |",
            f"| COBS roundtrip throughput (MB/s) | `{cobs['throughput_mb_per_s']:.3f}` | `TBD` |",
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


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate DELTA-V benchmark baseline artifacts."
    )
    parser.add_argument("--workspace", required=True, type=Path)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--no-sync-docs", action="store_true")
    args = parser.parse_args()

    workspace = args.workspace.resolve()
    build_dir = args.build_dir.resolve()
    sync_docs = not args.no_sync_docs

    benchmark_bin = build_dir / "run_benchmarks"
    require_file(benchmark_bin)

    out_dir = build_dir / "benchmark"
    out_dir.mkdir(parents=True, exist_ok=True)
    metrics_json = out_dir / "benchmark_metrics.json"

    metrics = run_benchmark_binary(benchmark_bin, metrics_json)

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
        "metrics": metrics,
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
