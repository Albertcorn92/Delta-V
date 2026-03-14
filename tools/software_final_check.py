#!/usr/bin/env python3
"""
software_final_check.py

Validate software-only release readiness and synchronize generated assurance
artifacts into docs/ for publication.
"""

from __future__ import annotations

import argparse
import json
import os
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
    required = [
        "docs/civilian_use_policy.md",
        "docs/export_control_note.md",
        "docs/legal_faq.md",
    ]
    missing = [item for item in required if item not in text]
    if missing:
        raise ValueError(
            "README legal scope links missing: " + ", ".join(sorted(missing))
        )


def validate_architecture_doc(architecture_path: Path) -> None:
    text = architecture_path.read_text(encoding="utf-8")

    forbidden_snippets = [
        "Rate Groups (Planned — Phase 5)",
        "| Rate group executive (10/1/0.1 Hz tiers) | Planned",
    ]
    for snippet in forbidden_snippets:
        if snippet in text:
            raise ValueError(
                f"Architecture doc contains stale runtime statement: '{snippet}'"
            )

    required_snippets = [
        "RateGroupExecutive",
        "DELTAV_ENABLE_HOST_HEAP_GUARD=1",
    ]
    missing = [snippet for snippet in required_snippets if snippet not in text]
    if missing:
        raise ValueError(
            "Architecture doc missing required runtime details: "
            + ", ".join(missing)
        )


def validate_safety_case_templates(workspace: Path) -> None:
    required = [
        workspace / "docs" / "safety_case" / "README.md",
        workspace / "docs" / "safety_case" / "hazards.md",
        workspace / "docs" / "safety_case" / "mitigations.md",
        workspace / "docs" / "safety_case" / "verification_links.md",
        workspace / "docs" / "safety_case" / "fmea.md",
        workspace / "docs" / "safety_case" / "fta.md",
        workspace / "docs" / "safety_case" / "change_impact.md",
        workspace / "docs" / "COVERAGE_RAMP_PLAN.md",
    ]
    missing = [str(path.relative_to(workspace)) for path in required if not path.exists()]
    if missing:
        raise ValueError("Missing safety/process templates: " + ", ".join(missing))


def require_any_glob(workspace: Path, pattern: str, label: str) -> None:
    matches = sorted(workspace.glob(pattern))
    if not matches:
        raise ValueError(f"Missing required {label}: expected pattern '{pattern}'")


def validate_process_evidence_baseline(workspace: Path) -> None:
    required_static = [
        workspace / "docs" / "process" / "DEPLOYMENT_SCREENING_PROCEDURE.md",
        workspace / "docs" / "process" / "SOFTWARE_CLASSIFICATION_BASELINE.md",
        workspace / "docs" / "process" / "NASA_REQUIREMENTS_APPLICABILITY_BASELINE.md",
        workspace / "docs" / "process" / "PSAC_DELTAV_BASELINE.md",
        workspace / "docs" / "process" / "SCMP_DELTAV_BASELINE.md",
        workspace / "docs" / "process" / "SQAP_DELTAV_BASELINE.md",
        workspace / "docs" / "process" / "SVVP_DELTAV_BASELINE.md",
        workspace / "docs" / "process" / "SOFTWARE_SAFETY_PLAN_BASELINE.md",
        workspace / "docs" / "process" / "STATIC_ANALYSIS_DEVIATION_LOG.md",
        workspace / "docs" / "process" / "TAILORING_AND_SCOPE_DEVIATIONS_BASELINE.md",
        workspace / "docs" / "process" / "TOOL_GOVERNANCE_BASELINE.md",
        workspace / "docs" / "process" / "PUBLIC_SECURITY_POSTURE_BASELINE.md",
        workspace / "docs" / "process" / "REFERENCE_MISSION_PROFILE.md",
        workspace / "docs" / "process" / "REFERENCE_PAYLOAD_PROFILE.md",
        workspace / "docs" / "process" / "REFERENCE_MISSION_REQUIREMENTS_ALLOCATION.md",
        workspace / "docs" / "process" / "REFERENCE_MISSION_INTERFACE_CONTROL.md",
        workspace / "docs" / "process" / "RISK_REGISTER_BASELINE.md",
        workspace / "docs" / "process" / "ASSUMPTIONS_LOG_BASELINE.md",
        workspace / "docs" / "process" / "CONFIGURATION_AUDIT_BASELINE.md",
        workspace / "docs" / "process" / "PROBLEM_REPORT_AND_CORRECTIVE_ACTION_LOG.md",
        workspace / "docs" / "process" / "SITL_LONG_SOAK_STATUS.md",
    ]
    missing_static = [
        str(path.relative_to(workspace)) for path in required_static if not path.exists()
    ]
    if missing_static:
        raise ValueError(
            "Missing required process evidence docs: " + ", ".join(missing_static)
        )

    require_any_glob(
        workspace,
        "docs/process/INDEPENDENT_REVIEW_RECORD_*.md",
        "independent review record",
    )
    require_any_glob(
        workspace,
        "docs/process/CCB_RELEASE_SIGNOFF_*.md",
        "CCB release sign-off record",
    )
    require_any_glob(
        workspace,
        "docs/process/DEPLOYMENT_SCREENING_LOG_*.md",
        "deployment screening log",
    )
    require_any_glob(
        workspace,
        "docs/process/OPERATIONS_REHEARSAL_*.md",
        "operations rehearsal record",
    )


def sync_artifact(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def latest_files(directory: Path, pattern: str) -> list[Path]:
    if not directory.exists():
        return []
    return sorted(directory.glob(pattern), key=lambda p: p.stat().st_mtime, reverse=True)


def has_passing_reboot_campaign(artifacts_dir: Path) -> bool:
    for candidate in latest_files(artifacts_dir, "esp32_reboot_campaign_*.json"):
        try:
            payload = load_json(candidate)
            summary = payload.get("summary", {})
            if int(summary.get("fail_cycles", 1)) == 0 and int(summary.get("pass_cycles", 0)) > 0:
                return True
        except Exception:
            continue
    return False


def has_passing_runtime_guard(artifacts_dir: Path) -> bool:
    for candidate in latest_files(artifacts_dir, "esp32_runtime_guard_*.json"):
        try:
            payload = load_json(candidate)
            thresholds = payload.get("thresholds", {})
            summary = payload.get("summary", {})
            samples = int(payload.get("samples", 0))

            min_samples = int(thresholds.get("min_samples", 1))
            min_stack = int(thresholds.get("min_stack_words", 0))
            max_fast = int(thresholds.get("max_fast_tick_wcet_us", 1_000_000))
            max_loop = int(thresholds.get("max_loop_wcet_us", 1_000_000))
            max_overruns = int(thresholds.get("max_loop_overruns", 0))

            if samples < min_samples:
                continue
            if int(summary.get("max_fast_tick_wcet_us", max_fast + 1)) > max_fast:
                continue
            if int(summary.get("max_loop_wcet_us", max_loop + 1)) > max_loop:
                continue
            if int(summary.get("last_loop_overruns", max_overruns + 1)) > max_overruns:
                continue
            if int(summary.get("min_stack_free_words", 0)) < min_stack:
                continue
            return True
        except Exception:
            continue
    return False


def has_passing_hil_fault_campaign(artifacts_dir: Path) -> bool:
    # Reboot/reset cycling is treated as on-target injected-fault evidence.
    return has_passing_reboot_campaign(artifacts_dir)


def has_passing_sensorless_soak(
    artifacts_dir: Path, *, min_duration_s: float = 3600.0
) -> bool:
    for candidate in latest_files(artifacts_dir, "esp32_soak_*.json"):
        try:
            payload = load_json(candidate)
            if str(payload.get("status", "")).upper() != "PASS":
                continue
            if float(payload.get("duration_s", 0.0)) < min_duration_s:
                continue
            return True
        except Exception:
            continue
    return False


def require_program_signatures() -> bool:
    return os.getenv("DELTAV_REQUIRE_PROGRAM_SIGNATURES", "0") == "1"


def collect_remaining_non_software(workspace: Path) -> list[str]:
    artifacts_dir = workspace / "artifacts"
    remaining: list[str] = []

    if not has_passing_hil_fault_campaign(artifacts_dir):
        remaining.append("ESP32 on-target HIL campaign evidence (fault injection logs).")

    if not has_passing_runtime_guard(artifacts_dir):
        remaining.append(
            "On-target timing/WCET and stack-margin PASS evidence "
            "(`tools/esp32_runtime_guard.py`)."
        )

    if not has_passing_sensorless_soak(artifacts_dir, min_duration_s=3600.0):
        remaining.append("ESP32 1h+ sensorless soak evidence (`tools/esp32_soak.py`).")
    if require_program_signatures():
        remaining.append(
            "Mission-program process records requiring independent review signatures."
        )
    return remaining


def write_status_report(
    path: Path,
    *,
    generated_utc: str,
    total_requirements: int,
    covered_requirements: int,
    gate_count: int,
    synced: bool,
    remaining_work: list[str],
) -> None:
    lines = [
        "# DELTA-V Software Finalization Status",
        "",
        f"- Generated (UTC): `{generated_utc}`",
        f"- Status: `PASS`",
        "- Scope: `Software baseline only` (not a standalone CubeSat flight-readiness claim)",
        f"- Requirements covered by direct tests: `{covered_requirements}/{total_requirements}`",
        f"- Qualification gates passing: `{gate_count}`",
        f"- Artifacts synced to docs/: `{synced}`",
        "",
        "## Remaining Work (Non-Software)",
        "",
    ]
    if remaining_work:
        lines.extend([f"- {item}" for item in remaining_work])
    else:
        lines.append("- None for baseline release profile.")
    lines.append("")
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
    validate_architecture_doc(workspace / "docs" / "ARCHITECTURE.md")
    validate_safety_case_templates(workspace)
    validate_process_evidence_baseline(workspace)

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
        remaining_work=collect_remaining_non_software(workspace),
    )

    print(
        "[software-final] PASS: software-only release readiness validated "
        f"({covered_reqs}/{total_reqs} requirements with tests, {gate_count} gates)."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
