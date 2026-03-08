#!/usr/bin/env python3
"""
cubesat_readiness_report.py

Generate a CubeSat readiness status report for the DELTA-V framework.
This separates framework automation evidence from mission-program evidence.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

PASS = "PASS"
FAIL = "FAIL"
MANUAL = "MANUAL"
WAIVED = "WAIVED"


def latest_files(directory: Path, pattern: str) -> list[Path]:
    if not directory.exists():
        return []
    return sorted(directory.glob(pattern), key=lambda p: p.stat().st_mtime, reverse=True)


def display_path(path: Path, workspace: Path) -> str:
    try:
        return str(path.resolve().relative_to(workspace.resolve()))
    except ValueError:
        return str(path.resolve())


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def run_legal_check(workspace: Path, python_exe: str) -> tuple[str, str]:
    cmd = [python_exe, str(workspace / "tools" / "legal_compliance_check.py")]
    result = subprocess.run(
        cmd,
        cwd=str(workspace),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    status = PASS if result.returncode == 0 else FAIL
    lines = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    snippet = lines[-1] if lines else "no output"
    return status, snippet


def check_software_final_status(workspace: Path) -> tuple[str, str]:
    path = workspace / "docs" / "SOFTWARE_FINAL_STATUS.md"
    if not path.exists():
        return FAIL, "missing docs/SOFTWARE_FINAL_STATUS.md"
    text = path.read_text(encoding="utf-8", errors="ignore")
    if "- Status: `PASS`" in text:
        return PASS, display_path(path, workspace)
    return FAIL, "status is not PASS in docs/SOFTWARE_FINAL_STATUS.md"


def check_qualification_report(workspace: Path) -> tuple[str, str]:
    path = workspace / "docs" / "qualification_report.json"
    if not path.exists():
        return FAIL, "missing docs/qualification_report.json"
    payload = load_json(path)
    gates = payload.get("gates", [])
    if not isinstance(gates, list) or not gates:
        return FAIL, "qualification report has no gate results"
    failing = [
        str(gate.get("name", "?"))
        for gate in gates
        if str(gate.get("status", "")).upper() != PASS
    ]
    if failing:
        return FAIL, "failing qualification gates: " + ", ".join(sorted(failing))
    return PASS, display_path(path, workspace)


def check_traceability(workspace: Path) -> tuple[str, str]:
    path = workspace / "docs" / "REQUIREMENTS_TRACE_MATRIX.json"
    if not path.exists():
        return FAIL, "missing docs/REQUIREMENTS_TRACE_MATRIX.json"
    payload = load_json(path)
    summary = payload.get("summary", {})
    total = int(summary.get("total_requirements", 0))
    with_tests = int(summary.get("pass_with_tests", 0))
    errors = int(summary.get("errors", 0))
    if total <= 0:
        return FAIL, "traceability has zero requirements"
    if errors != 0:
        return FAIL, f"traceability reports {errors} error(s)"
    if with_tests != total:
        return FAIL, f"requirements with tests {with_tests}/{total}"
    return PASS, f"{display_path(path, workspace)} ({with_tests}/{total})"


def has_passing_reboot_campaign(artifacts_dir: Path, workspace: Path) -> tuple[bool, str]:
    for candidate in latest_files(artifacts_dir, "esp32_reboot_campaign_*.json"):
        try:
            payload = load_json(candidate)
            summary = payload.get("summary", {})
            if int(summary.get("fail_cycles", 1)) == 0 and int(summary.get("pass_cycles", 0)) > 0:
                return True, display_path(candidate, workspace)
        except Exception:
            continue
    return False, "no passing esp32_reboot_campaign_*.json"


def has_passing_runtime_guard(artifacts_dir: Path, workspace: Path) -> tuple[bool, str]:
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
            return True, display_path(candidate, workspace)
        except Exception:
            continue
    return False, "no passing esp32_runtime_guard_*.json"


def has_passing_soak(
    artifacts_dir: Path, workspace: Path, *, min_duration_s: float
) -> tuple[bool, str]:
    for candidate in latest_files(artifacts_dir, "esp32_soak_*.json"):
        try:
            payload = load_json(candidate)
            if str(payload.get("status", "")).upper() != PASS:
                continue
            duration = float(payload.get("duration_s", 0.0))
            if duration < min_duration_s:
                continue
            return True, f"{display_path(candidate, workspace)} ({duration:.0f}s)"
        except Exception:
            continue

    # Legacy evidence fallback for baseline sensorless runs where
    # markdown evidence exists but JSON artifacts are not archived.
    if min_duration_s <= 1800.0:
        for doc in latest_files(workspace / "docs" / "evidence", "ESP32_SENSORLESS_EVIDENCE_*.md"):
            text = doc.read_text(encoding="utf-8", errors="ignore")
            status = _sensorless_soak_status_from_markdown(text, min_duration_s)
            if status == PASS:
                return True, f"{display_path(doc, workspace)} (documented soak pass)"
            if status == FAIL:
                return False, f"{display_path(doc, workspace)} (documented soak fail)"

    return False, f"no passing esp32_soak_*.json with duration >= {min_duration_s:.0f}s"


def _sensorless_soak_status_from_markdown(text: str, min_duration_s: float) -> str | None:
    # Matches lines like: "Sensorless soak (1800s): PASS"
    pattern = re.compile(
        r"sensorless\s+soak\s*\(\s*(\d+(?:\.\d+)?)\s*s\s*\)\s*:\s*([A-Z ]+)",
        re.IGNORECASE,
    )
    matched_any = False
    saw_pass = False
    for duration_s_text, status_text in pattern.findall(text):
        try:
            duration_s = float(duration_s_text)
        except ValueError:
            continue
        if duration_s < min_duration_s:
            continue
        matched_any = True
        status = _classify_status_text(status_text)
        if status == FAIL:
            return FAIL
        if status == PASS:
            saw_pass = True
    if saw_pass:
        return PASS
    if matched_any:
        return MANUAL
    return None


def _classify_status_text(value: str) -> str | None:
    normalized = re.sub(r"\s+", " ", value).strip().upper()
    if not normalized:
        return None

    if "NOT RUN" in normalized or "PENDING" in normalized:
        return MANUAL

    if any(token in normalized for token in ("FAIL", "FAILED", "REJECTED", "BLOCKED", "ERROR")):
        return FAIL

    if any(token in normalized for token in ("PASS", "COMPLETE", "APPROVED", "DONE")):
        return PASS

    if any(token in normalized for token in ("MISSION HANDOFF", "TBD", "IN PROGRESS")):
        return MANUAL

    return None


def _status_from_markdown_table_rows(text: str) -> str | None:
    saw_pass = False
    saw_manual = False
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line.startswith("|"):
            continue
        if re.fullmatch(r"\|[-:\s|]+\|", line):
            continue
        cells = [cell.strip() for cell in line.strip("|").split("|")]
        for cell in cells:
            status = _classify_status_text(cell)
            if status == FAIL:
                return FAIL
            if status == PASS:
                saw_pass = True
            if status == MANUAL:
                saw_manual = True
    if saw_pass:
        return PASS
    if saw_manual:
        return MANUAL
    return None


def _record_status(text: str) -> tuple[str, str]:
    status_line = re.search(r"^\s*Status\s*:\s*(.+)$", text, flags=re.IGNORECASE | re.MULTILINE)
    if status_line:
        raw_status = status_line.group(1).strip()
        classified = _classify_status_text(raw_status)
        if classified is not None:
            return classified, f"status: {raw_status}"

    table_status = _status_from_markdown_table_rows(text)
    if table_status == FAIL:
        return FAIL, "table includes FAIL result"
    if table_status == PASS:
        return PASS, "table includes PASS results"
    if table_status == MANUAL:
        return MANUAL, "table shows manual/pending results"

    if re.search(r"\bNOT\s+RUN\b", text, re.IGNORECASE):
        return MANUAL, "record exists, execution pending"

    return MANUAL, "record exists, no explicit pass/fail status"


def record_or_template_state(
    workspace: Path,
    *,
    record_glob: str,
    template_relpath: str,
    label: str,
) -> tuple[str, str]:
    records = [
        path
        for path in latest_files(workspace, record_glob)
        if not path.name.endswith("_TEMPLATE.md")
    ]
    if records:
        text = records[0].read_text(encoding="utf-8", errors="ignore")
        status, reason = _record_status(text)
        return status, f"{display_path(records[0], workspace)} ({reason})"

    template = workspace / template_relpath
    if template.exists():
        return MANUAL, f"{label}: template ready ({display_path(template, workspace)})"

    return FAIL, f"{label}: missing template {template_relpath}"


def write_markdown(path: Path, payload: dict[str, Any]) -> None:
    checks = payload["checks"]
    lines: list[str] = []
    lines.append("# DELTA-V CubeSat Readiness Status")
    lines.append("")
    lines.append(f"- Generated (UTC): `{payload['generated_utc']}`")
    lines.append(f"- Framework release readiness: `{payload['framework_release_ready']}`")
    lines.append(f"- CubeSat flight readiness: `{payload['cubesat_flight_ready']}`")
    lines.append("")
    lines.append("## Automated and Program Checks")
    lines.append("")
    lines.append("| ID | Category | Status | Required For | Evidence |")
    lines.append("|---|---|---|---|---|")
    for check in checks:
        required_for = ", ".join(check["required_for"])
        lines.append(
            f"| {check['id']} | {check['category']} | {check['status']} | "
            f"{required_for} | {check['evidence']} |"
        )
    lines.append("")

    lines.append("## Remaining Gaps")
    lines.append("")
    gaps = payload.get("remaining_gaps", [])
    if gaps:
        for gap in gaps:
            lines.append(f"- {gap}")
    else:
        lines.append("- None.")
    lines.append("")

    lines.append("## Notes")
    lines.append("")
    lines.append("- Framework automation can reduce software risk, but mission qualification still requires hardware and operations evidence.")
    lines.append("- This report is an engineering status snapshot, not legal advice or certification approval.")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate DELTA-V CubeSat readiness status from repository evidence."
    )
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--python-exe", default=sys.executable)
    parser.add_argument(
        "--output-md",
        type=Path,
        default=Path("docs/CUBESAT_READINESS_STATUS.md"),
    )
    parser.add_argument(
        "--output-json",
        type=Path,
        default=Path("docs/CUBESAT_READINESS_STATUS.json"),
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Return non-zero if CubeSat flight readiness is false.",
    )
    parser.add_argument(
        "--exclude-check",
        action="append",
        default=[],
        help="Check ID to waive for scope-limited readiness runs (repeatable).",
    )
    args = parser.parse_args()

    workspace = args.workspace.resolve()
    artifacts_dir = workspace / "artifacts"
    checks: list[dict[str, Any]] = []
    excluded_checks = {value.strip() for value in args.exclude_check if value.strip()}

    def add_check(
        check_id: str,
        category: str,
        status: str,
        required_for: list[str],
        evidence: str,
    ) -> None:
        if check_id in excluded_checks:
            status = WAIVED
            evidence = (
                "scope waiver requested via --exclude-check"
                if not evidence
                else f"{evidence} | scope waiver requested via --exclude-check"
            )
        checks.append(
            {
                "id": check_id,
                "category": category,
                "status": status,
                "required_for": required_for,
                "evidence": evidence,
            }
        )

    legal_status, legal_evidence = run_legal_check(workspace, args.python_exe)
    add_check(
        "legal-compliance",
        "Civilian/Legal",
        legal_status,
        ["framework", "flight"],
        legal_evidence,
    )

    status, evidence = check_software_final_status(workspace)
    add_check(
        "software-final",
        "Software",
        status,
        ["framework"],
        evidence,
    )

    status, evidence = check_qualification_report(workspace)
    add_check(
        "qualification-report",
        "Software",
        status,
        ["framework"],
        evidence,
    )

    status, evidence = check_traceability(workspace)
    add_check(
        "requirements-traceability",
        "Software",
        status,
        ["framework"],
        evidence,
    )

    runtime_ok, runtime_evidence = has_passing_runtime_guard(artifacts_dir, workspace)
    add_check(
        "esp32-runtime-guard",
        "Hardware",
        PASS if runtime_ok else FAIL,
        ["framework", "flight"],
        runtime_evidence,
    )

    reboot_ok, reboot_evidence = has_passing_reboot_campaign(artifacts_dir, workspace)
    add_check(
        "esp32-reboot-campaign",
        "Hardware",
        PASS if reboot_ok else FAIL,
        ["framework", "flight"],
        reboot_evidence,
    )

    soak_30_ok, soak_30_evidence = has_passing_soak(
        artifacts_dir, workspace, min_duration_s=1800.0
    )
    add_check(
        "esp32-soak-30m",
        "Hardware",
        PASS if soak_30_ok else FAIL,
        ["framework"],
        soak_30_evidence,
    )

    soak_1h_ok, soak_1h_evidence = has_passing_soak(
        artifacts_dir, workspace, min_duration_s=3600.0
    )
    add_check(
        "esp32-soak-1h",
        "Hardware",
        PASS if soak_1h_ok else FAIL,
        ["flight"],
        soak_1h_evidence,
    )

    status, evidence = record_or_template_state(
        workspace,
        record_glob="docs/evidence/ESP32_SENSOR_ATTACHED_EVIDENCE_*.md",
        template_relpath="docs/evidence/ESP32_SENSOR_ATTACHED_EVIDENCE_TEMPLATE.md",
        label="sensor-attached evidence",
    )
    add_check(
        "sensor-attached-evidence",
        "Hardware",
        status,
        ["flight"],
        evidence,
    )

    status, evidence = record_or_template_state(
        workspace,
        record_glob="docs/process/FLIGHT_ENV_TEST_MATRIX_*.md",
        template_relpath="docs/process/FLIGHT_ENV_TEST_MATRIX_TEMPLATE.md",
        label="flight environment test matrix",
    )
    add_check(
        "flight-env-test-matrix",
        "Program",
        status,
        ["flight"],
        evidence,
    )

    status, evidence = record_or_template_state(
        workspace,
        record_glob="docs/process/COMMS_LINK_VALIDATION_*.md",
        template_relpath="docs/process/COMMS_LINK_VALIDATION_TEMPLATE.md",
        label="comms link validation",
    )
    add_check(
        "comms-link-validation",
        "Program",
        status,
        ["flight"],
        evidence,
    )

    status, evidence = record_or_template_state(
        workspace,
        record_glob="docs/process/OPERATIONS_READINESS_*.md",
        template_relpath="docs/process/OPERATIONS_READINESS_TEMPLATE.md",
        label="operations readiness",
    )
    add_check(
        "operations-readiness",
        "Program",
        status,
        ["flight"],
        evidence,
    )

    status, evidence = record_or_template_state(
        workspace,
        record_glob="docs/process/RELEASE_MANIFEST_*.md",
        template_relpath="docs/process/RELEASE_MANIFEST_TEMPLATE.md",
        label="release manifest",
    )
    add_check(
        "release-manifest",
        "Program",
        status,
        ["flight"],
        evidence,
    )

    review_records = latest_files(workspace / "docs" / "process", "INDEPENDENT_REVIEW_RECORD_*.md")
    add_check(
        "independent-review-record",
        "Process",
        PASS if review_records else FAIL,
        ["framework", "flight"],
        display_path(review_records[0], workspace) if review_records else "missing docs/process/INDEPENDENT_REVIEW_RECORD_*.md",
    )

    ccb_records = latest_files(workspace / "docs" / "process", "CCB_RELEASE_SIGNOFF_*.md")
    add_check(
        "ccb-signoff",
        "Process",
        PASS if ccb_records else FAIL,
        ["framework", "flight"],
        display_path(ccb_records[0], workspace) if ccb_records else "missing docs/process/CCB_RELEASE_SIGNOFF_*.md",
    )

    screening_logs = latest_files(workspace / "docs" / "process", "DEPLOYMENT_SCREENING_LOG_*.md")
    screening_proc = workspace / "docs" / "process" / "DEPLOYMENT_SCREENING_PROCEDURE.md"
    screening_ok = bool(screening_logs) and screening_proc.exists()
    screening_evidence = (
        f"{display_path(screening_proc, workspace)}, {display_path(screening_logs[0], workspace)}"
        if screening_ok
        else "missing deployment screening procedure/log evidence"
    )
    add_check(
        "deployment-screening",
        "Civilian/Legal",
        PASS if screening_ok else FAIL,
        ["framework", "flight"],
        screening_evidence,
    )

    safety_case_required = [
        workspace / "docs" / "safety_case" / "README.md",
        workspace / "docs" / "safety_case" / "hazards.md",
        workspace / "docs" / "safety_case" / "mitigations.md",
        workspace / "docs" / "safety_case" / "verification_links.md",
        workspace / "docs" / "safety_case" / "fmea.md",
        workspace / "docs" / "safety_case" / "fta.md",
        workspace / "docs" / "safety_case" / "change_impact.md",
    ]
    missing_safety = [display_path(p, workspace) for p in safety_case_required if not p.exists()]
    add_check(
        "safety-case-baseline",
        "Process",
        PASS if not missing_safety else FAIL,
        ["framework", "flight"],
        "all baseline safety-case files present"
        if not missing_safety
        else "missing: " + ", ".join(missing_safety),
    )

    def scope_ready(scope: str) -> bool:
        scoped = [c for c in checks if scope in c["required_for"]]
        return all(c["status"] in {PASS, WAIVED} for c in scoped)

    framework_release_ready = scope_ready("framework")
    cubesat_flight_ready = scope_ready("flight")

    remaining_gaps = [
        f"{c['id']}: {c['evidence']}"
        for c in checks
        if c["status"] in {FAIL, MANUAL}
    ]

    payload = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "workspace": workspace.name,
        "build_dir": str(args.build_dir),
        "framework_release_ready": framework_release_ready,
        "cubesat_flight_ready": cubesat_flight_ready,
        "checks": checks,
        "remaining_gaps": remaining_gaps,
    }

    output_md = args.output_md.resolve() if args.output_md.is_absolute() else (workspace / args.output_md)
    output_json = args.output_json.resolve() if args.output_json.is_absolute() else (workspace / args.output_json)
    output_md.parent.mkdir(parents=True, exist_ok=True)
    output_json.parent.mkdir(parents=True, exist_ok=True)

    write_markdown(output_md, payload)
    output_json.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    print(f"[cubesat-readiness] report: {output_md}")
    print(
        "[cubesat-readiness] framework_release_ready="
        f"{framework_release_ready} cubesat_flight_ready={cubesat_flight_ready}"
    )

    if args.strict and not cubesat_flight_ready:
        print("[cubesat-readiness] FAIL: CubeSat flight readiness is not complete.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
