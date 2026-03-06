#!/usr/bin/env python3
"""
requirements_trace.py

Validates DELTA-V requirements-to-evidence mapping and emits matrix artifacts.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

try:
    import yaml
except ImportError as exc:  # pragma: no cover
    raise SystemExit(
        "PyYAML is required for requirements_trace.py. Install with: pip install pyyaml"
    ) from exc


REQ_ID_RE = re.compile(r"\b(DV-[A-Z]+-\d{2})\b")
TEST_RE = re.compile(r"\bTEST(?:_F)?\s*\(\s*([^,]+?)\s*,\s*([^)]+?)\s*\)")


def parse_requirement_ids(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    ids = sorted(set(REQ_ID_RE.findall(text)))
    return ids


def parse_test_names(path: Path) -> set[str]:
    text = path.read_text(encoding="utf-8")
    tests: set[str] = set()
    for match in TEST_RE.finditer(text):
        suite = match.group(1).strip()
        case = match.group(2).strip()
        tests.add(f"{suite}.{case}")
    return tests


def load_mapping(path: Path) -> dict:
    data = yaml.safe_load(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict) or "requirements" not in data:
        raise ValueError("Mapping file must contain a top-level 'requirements' map.")
    if not isinstance(data["requirements"], dict):
        raise ValueError("'requirements' must be a dictionary keyed by requirement ID.")
    return data["requirements"]


def validate_mapping(
    req_ids: list[str],
    available_tests: set[str],
    mapping: dict,
    strict: bool,
) -> tuple[list[dict], list[str]]:
    errors: list[str] = []
    rows: list[dict] = []

    req_set = set(req_ids)
    mapped_set = set(mapping.keys())

    missing_ids = sorted(req_set - mapped_set)
    extra_ids = sorted(mapped_set - req_set)

    for req_id in missing_ids:
        errors.append(f"Missing mapping entry for requirement: {req_id}")
    for req_id in extra_ids:
        errors.append(f"Mapping includes unknown requirement ID: {req_id}")

    for req_id in req_ids:
        spec = mapping.get(req_id, {})
        dal = spec.get("dal", "?")
        statement = spec.get("statement", "")
        evidence = spec.get("evidence", [])
        if not isinstance(evidence, list):
            errors.append(f"{req_id}: 'evidence' must be a list.")
            evidence = []

        test_refs: list[str] = []
        other_refs: list[str] = []
        for item in evidence:
            if not isinstance(item, dict):
                errors.append(f"{req_id}: evidence item must be an object.")
                continue
            ev_type = str(item.get("type", "")).strip()
            ev_ref = str(item.get("ref", "")).strip()
            if not ev_type or not ev_ref:
                errors.append(f"{req_id}: each evidence entry requires 'type' and 'ref'.")
                continue

            if ev_type == "test":
                test_refs.append(ev_ref)
                if ev_ref not in available_tests:
                    errors.append(f"{req_id}: mapped test does not exist: {ev_ref}")
            else:
                other_refs.append(f"{ev_type}:{ev_ref}")

        if strict and not evidence:
            errors.append(f"{req_id}: no evidence entries.")

        status = "PASS" if test_refs else ("PARTIAL" if other_refs else "MISSING")
        rows.append(
            {
                "id": req_id,
                "dal": dal,
                "statement": statement,
                "tests": sorted(test_refs),
                "other_evidence": sorted(other_refs),
                "status": status,
            }
        )

    return rows, errors


def write_markdown(path: Path, rows: list[dict]) -> None:
    lines: list[str] = []
    lines.append("# Requirements Traceability Matrix")
    lines.append("")
    lines.append("| Requirement | DAL | Status | Test Evidence | Other Evidence |")
    lines.append("|---|---|---|---|---|")
    for row in rows:
        tests = "<br>".join(row["tests"]) if row["tests"] else "-"
        other = "<br>".join(row["other_evidence"]) if row["other_evidence"] else "-"
        lines.append(
            f"| {row['id']} | {row['dal']} | {row['status']} | {tests} | {other} |"
        )
    lines.append("")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_json(path: Path, rows: list[dict], errors: list[str]) -> None:
    payload = {
        "summary": {
            "total_requirements": len(rows),
            "pass_with_tests": sum(1 for row in rows if row["status"] == "PASS"),
            "partial_non_test_only": sum(
                1 for row in rows if row["status"] == "PARTIAL"
            ),
            "missing": sum(1 for row in rows if row["status"] == "MISSING"),
            "errors": len(errors),
        },
        "rows": rows,
        "errors": errors,
    }
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate and emit requirements trace matrix.")
    parser.add_argument("--requirements", required=True, type=Path)
    parser.add_argument("--tests", required=True, type=Path)
    parser.add_argument("--mapping", required=True, type=Path)
    parser.add_argument("--output-md", required=True, type=Path)
    parser.add_argument("--output-json", required=True, type=Path)
    parser.add_argument("--strict", action="store_true")
    args = parser.parse_args()

    req_ids = parse_requirement_ids(args.requirements)
    tests = parse_test_names(args.tests)
    mapping = load_mapping(args.mapping)
    rows, errors = validate_mapping(req_ids, tests, mapping, args.strict)

    args.output_md.parent.mkdir(parents=True, exist_ok=True)
    args.output_json.parent.mkdir(parents=True, exist_ok=True)
    write_markdown(args.output_md, rows)
    write_json(args.output_json, rows, errors)

    if errors:
        for err in errors:
            print(f"[traceability] ERROR: {err}", file=sys.stderr)
        return 1

    print(
        "[traceability] OK: "
        f"{len(rows)} requirements validated, "
        f"{sum(1 for row in rows if row['status'] == 'PASS')} with direct test evidence."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
