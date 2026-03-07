#!/usr/bin/env python3
"""
requirements_trace.py

Validates DELTA-V requirements-to-evidence mapping and emits matrix artifacts.
"""

from __future__ import annotations

import argparse
import ast
import json
import re
import sys
from pathlib import Path

try:
    import yaml
except ImportError:  # pragma: no cover
    yaml = None


REQ_ID_RE = re.compile(r"\b(DV-[A-Z]+-\d{2})\b")
TEST_RE = re.compile(r"\bTEST(?:_F)?\s*\(\s*([^,]+?)\s*,\s*([^)]+?)\s*\)")


def parse_scalar(raw: str) -> str:
    value = raw.strip()
    if not value:
        return ""
    if value[0] in {'"', "'"} and value[-1] == value[0]:
        try:
            parsed = ast.literal_eval(value)
            return str(parsed)
        except Exception:
            return value[1:-1]
    return value


def split_key_value(line: str) -> tuple[str, str]:
    if ":" not in line:
        raise ValueError(f"Invalid mapping line (missing ':'): {line}")
    key, value = line.split(":", 1)
    key = key.strip()
    if not key:
        raise ValueError(f"Invalid mapping line (empty key): {line}")
    return key, value.strip()


def load_mapping_without_yaml(text: str) -> dict:
    lines = text.splitlines()
    i = 0
    while i < len(lines) and not lines[i].strip():
        i += 1
    if i >= len(lines) or lines[i].strip() != "requirements:":
        raise ValueError(
            "Mapping file must begin with 'requirements:' when PyYAML is unavailable."
        )
    i += 1

    requirements: dict[str, dict] = {}
    current_req: str | None = None

    while i < len(lines):
        raw = lines[i]
        stripped = raw.strip()
        if not stripped or stripped.startswith("#"):
            i += 1
            continue

        indent = len(raw) - len(raw.lstrip(" "))
        if indent == 2 and stripped.endswith(":"):
            current_req = stripped[:-1].strip()
            if not current_req:
                raise ValueError(f"Invalid requirement key on line {i + 1}")
            requirements[current_req] = {"evidence": []}
            i += 1
            continue

        if current_req is None:
            raise ValueError(f"Unexpected content before requirement block on line {i + 1}")

        if indent != 4:
            raise ValueError(f"Unsupported indentation on line {i + 1}: {raw}")

        key, value = split_key_value(stripped)
        if key != "evidence":
            requirements[current_req][key] = parse_scalar(value)
            i += 1
            continue

        if value:
            raise ValueError(f"'evidence' must not have an inline value (line {i + 1})")

        evidence_items: list[dict[str, str]] = []
        i += 1
        while i < len(lines):
            ev_raw = lines[i]
            ev_stripped = ev_raw.strip()
            if not ev_stripped or ev_stripped.startswith("#"):
                i += 1
                continue

            ev_indent = len(ev_raw) - len(ev_raw.lstrip(" "))
            if ev_indent <= 4:
                break
            if ev_indent != 6 or not ev_stripped.startswith("- "):
                raise ValueError(f"Invalid evidence entry on line {i + 1}: {ev_raw}")

            item: dict[str, str] = {}
            first = ev_stripped[2:].strip()
            if first:
                k, v = split_key_value(first)
                item[k] = parse_scalar(v)
            i += 1

            while i < len(lines):
                field_raw = lines[i]
                field_stripped = field_raw.strip()
                if not field_stripped or field_stripped.startswith("#"):
                    i += 1
                    continue

                field_indent = len(field_raw) - len(field_raw.lstrip(" "))
                if field_indent <= 6:
                    break
                if field_indent != 8:
                    raise ValueError(
                        f"Invalid evidence field indentation on line {i + 1}: {field_raw}"
                    )
                fk, fv = split_key_value(field_stripped)
                item[fk] = parse_scalar(fv)
                i += 1

            evidence_items.append(item)

        requirements[current_req]["evidence"] = evidence_items

    return {"requirements": requirements}


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
    raw_text = path.read_text(encoding="utf-8")
    if path.suffix.lower() == ".json":
        data = json.loads(raw_text)
    elif yaml is not None:
        data = yaml.safe_load(raw_text)
    else:
        data = load_mapping_without_yaml(raw_text)
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
