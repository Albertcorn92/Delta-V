#!/usr/bin/env python3
"""
software_final_check.py

Validate software-only release readiness and synchronize generated assurance
artifacts into docs/ for publication.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


def require_file(path: Path) -> None:
    if not path.exists():
        raise FileNotFoundError(f"missing required artifact: {path}")


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def run_legal_check(workspace: Path, python_exe: str) -> None:
    cmd = [python_exe, str(workspace / "tools" / "legal_compliance_check.py")]
    subprocess.run(cmd, cwd=str(workspace), check=True)


def validate_traceability(trace_payload: dict[str, Any]) -> tuple[int, int]:
    summary = trace_payload.get("summary", {})
    total = int(summary.get("total_requirements", 0))
    with_tests = int(summary.get("pass_with_tests", 0))
    errors = int(summary.get("errors", 0))
    if total <= 0:
        raise ValueError("traceability summary has zero requirements")
    if errors != 0:
        raise ValueError(f"traceability summary reports {errors} error(s)")
    if with_tests != total:
        raise ValueError(
            "traceability coverage incomplete: "
            f"with_tests={with_tests}, total_requirements={total}"
        )
    return total, with_tests


def validate_qualification(qualification_payload: dict[str, Any]) -> int:
    gates = qualification_payload.get("gates", [])
    if not isinstance(gates, list) or not gates:
        raise ValueError("qualification report contains no gate results")
    failing = [
        gate.get("name", "?")
        for gate in gates
        if str(gate.get("status", "")).upper() != "PASS"
    ]
    if failing:
        raise ValueError(f"qualification gate failures present: {', '.join(failing)}")
    return len(gates)


def validate_readme_legal_links(readme_path: Path) -> None:
    text = readme_path.read_text(encoding="utf-8").lower()
    required = ["docs/civilian_use_policy.md", "docs/export_control_note.md"]
    missing = [item for item in required if item not in text]
    if missing:
        raise ValueError(
            "README legal scope links missing: " + ", ".join(sorted(missing))
        )


def sync_artifact(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def write_status_report(
    path: Path,
    *,
    generated_utc: str,
    total_requirements: int,
    covered_requirements: int,
    gate_count: int,
    synced: bool,
) -> None:
    lines = [
        "# DELTA-V Software Finalization Status",
        "",
        f"- Generated (UTC): `{generated_utc}`",
        f"- Status: `PASS`",
        f"- Requirements covered by direct tests: `{covered_requirements}/{total_requirements}`",
        f"- Qualification gates passing: `{gate_count}`",
        f"- Artifacts synced to docs/: `{synced}`",
        "",
        "## Remaining Work (Non-Software)",
        "",
        "- ESP32 on-target HIL campaign evidence (fault injection and soak logs).",
        "- On-target timing/WCET and stack-margin evidence.",
        "- Mission-program process records requiring independent review signatures.",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate and finalize software-only DELTA-V release evidence."
    )
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--python-exe", default=sys.executable)
    parser.add_argument("--no-sync-docs", action="store_true")
    args = parser.parse_args()

    workspace = args.workspace.resolve()
    build_dir = args.build_dir.resolve()
    docs_dir = workspace / "docs"
    sync_docs = not args.no_sync_docs

    trace_md = build_dir / "requirements_trace_matrix.md"
    trace_json = build_dir / "requirements_trace_matrix.json"
    qual_md = build_dir / "qualification" / "qualification_report.md"
    qual_json = build_dir / "qualification" / "qualification_report.json"
    for artifact in [trace_md, trace_json, qual_md, qual_json]:
        require_file(artifact)

    run_legal_check(workspace, args.python_exe)
    validate_readme_legal_links(workspace / "README.md")

    trace_payload = load_json(trace_json)
    total_reqs, covered_reqs = validate_traceability(trace_payload)
    qual_payload = load_json(qual_json)
    gate_count = validate_qualification(qual_payload)

    if sync_docs:
        sync_artifact(trace_md, docs_dir / "REQUIREMENTS_TRACE_MATRIX.md")
        sync_artifact(trace_json, docs_dir / "REQUIREMENTS_TRACE_MATRIX.json")
        sync_artifact(qual_md, docs_dir / "qualification_report.md")
        sync_artifact(qual_json, docs_dir / "qualification_report.json")

    generated_utc = datetime.now(timezone.utc).isoformat()
    write_status_report(
        docs_dir / "SOFTWARE_FINAL_STATUS.md",
        generated_utc=generated_utc,
        total_requirements=total_reqs,
        covered_requirements=covered_reqs,
        gate_count=gate_count,
        synced=sync_docs,
    )

    print(
        "[software-final] PASS: software-only release readiness validated "
        f"({covered_reqs}/{total_reqs} requirements with tests, {gate_count} gates)."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
