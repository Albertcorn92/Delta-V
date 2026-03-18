#!/usr/bin/env python3
"""
Validate DELTA-V safety-case cross-links.

This is a repository-consistency check for the public baseline. It does not
claim external approval; it only checks that hazards, mitigations, and
verification links stay synchronized with each other and with the repository
requirements set.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


REQ_RE = re.compile(r"DV-[A-Z]+-\d+")


def parse_table(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    lines = path.read_text(encoding="utf-8").splitlines()
    table_lines: list[str] = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("|") and stripped.endswith("|"):
            table_lines.append(stripped)
        elif table_lines:
            break

    if len(table_lines) < 3:
        raise ValueError(f"{path}: no markdown table found")

    headers = [cell.strip() for cell in table_lines[0].strip("|").split("|")]
    rows: list[dict[str, str]] = []
    for raw in table_lines[2:]:
        cells = [cell.strip() for cell in raw.strip("|").split("|")]
        if len(cells) != len(headers):
            raise ValueError(f"{path}: malformed table row: {raw}")
        rows.append(dict(zip(headers, cells)))
    return headers, rows


def requirement_ids(text: str) -> set[str]:
    return set(REQ_RE.findall(text))


def require_unique(rows: list[dict[str, str]], key: str, label: str) -> None:
    seen: set[str] = set()
    duplicates: set[str] = set()
    for row in rows:
        value = row.get(key, "").strip()
        if not value:
            raise ValueError(f"{label}: blank '{key}' cell")
        if value in seen:
            duplicates.add(value)
        seen.add(value)
    if duplicates:
        raise ValueError(
            f"{label}: duplicate IDs in '{key}': " + ", ".join(sorted(duplicates))
        )


def first_path_token(value: str) -> str:
    token = value.split()[0].strip("`()")
    return token.rstrip(",")


def validate_reference_paths(workspace: Path, verification_rows: list[dict[str, str]]) -> None:
    missing: list[str] = []
    for row in verification_rows:
        path_token = first_path_token(row.get("Evidence Path", ""))
        if not path_token:
            missing.append(f"{row.get('Link ID', '?')}: blank evidence path")
            continue
        if path_token.startswith(("docs/", "src/", "tests/", "tools/")):
            if not (workspace / path_token).exists():
                missing.append(f"{row.get('Link ID', '?')}: missing {path_token}")
    if missing:
        raise ValueError(
            "verification links reference missing repository files: " + "; ".join(missing)
        )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate DELTA-V safety-case link integrity."
    )
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--requirements", type=Path, required=True)
    args = parser.parse_args()

    workspace = args.workspace.resolve()
    requirements_path = args.requirements.resolve()

    hazards_path = workspace / "docs" / "safety_case" / "hazards.md"
    mitigations_path = workspace / "docs" / "safety_case" / "mitigations.md"
    verification_path = workspace / "docs" / "safety_case" / "verification_links.md"

    _, hazard_rows = parse_table(hazards_path)
    _, mitigation_rows = parse_table(mitigations_path)
    _, verification_rows = parse_table(verification_path)

    require_unique(hazard_rows, "Hazard ID", "hazards.md")
    require_unique(mitigation_rows, "Mitigation ID", "mitigations.md")
    require_unique(verification_rows, "Link ID", "verification_links.md")

    known_requirements = requirement_ids(requirements_path.read_text(encoding="utf-8"))
    if not known_requirements:
        raise ValueError("requirements file contains no DV requirement IDs")

    hazards_by_id = {row["Hazard ID"]: row for row in hazard_rows}
    mitigations_by_id = {row["Mitigation ID"]: row for row in mitigation_rows}

    hazard_to_mitigations: dict[str, list[str]] = {hazard_id: [] for hazard_id in hazards_by_id}
    hazard_to_verifications: dict[str, list[str]] = {hazard_id: [] for hazard_id in hazards_by_id}

    for hazard_id, row in hazards_by_id.items():
        reqs = requirement_ids(row.get("Primary Requirement IDs", ""))
        if not reqs:
            raise ValueError(f"hazards.md: {hazard_id} has no requirement IDs")
        unknown = sorted(reqs - known_requirements)
        if unknown:
            raise ValueError(
                f"hazards.md: {hazard_id} references unknown requirements: "
                + ", ".join(unknown)
            )

    for mitigation_id, row in mitigations_by_id.items():
        hazard_id = row.get("Hazard ID", "").strip()
        if hazard_id not in hazards_by_id:
            raise ValueError(
                f"mitigations.md: {mitigation_id} references unknown hazard {hazard_id}"
            )

        mitigation_reqs = requirement_ids(row.get("Requirement ID", ""))
        if not mitigation_reqs:
            raise ValueError(f"mitigations.md: {mitigation_id} has no requirement IDs")
        unknown = sorted(mitigation_reqs - known_requirements)
        if unknown:
            raise ValueError(
                f"mitigations.md: {mitigation_id} references unknown requirements: "
                + ", ".join(unknown)
            )

        hazard_reqs = requirement_ids(hazards_by_id[hazard_id]["Primary Requirement IDs"])
        if not mitigation_reqs.issubset(hazard_reqs):
            raise ValueError(
                f"mitigations.md: {mitigation_id} uses requirements not listed on "
                f"{hazard_id}: " + ", ".join(sorted(mitigation_reqs - hazard_reqs))
            )
        hazard_to_mitigations[hazard_id].append(mitigation_id)

    for row in verification_rows:
        link_id = row["Link ID"]
        hazard_id = row.get("Hazard ID", "").strip()
        mitigation_id = row.get("Mitigation ID", "").strip()
        if hazard_id not in hazards_by_id:
            raise ValueError(
                f"verification_links.md: {link_id} references unknown hazard {hazard_id}"
            )
        if mitigation_id not in mitigations_by_id:
            raise ValueError(
                f"verification_links.md: {link_id} references unknown mitigation {mitigation_id}"
            )
        mitigation_hazard_id = mitigations_by_id[mitigation_id]["Hazard ID"].strip()
        if mitigation_hazard_id != hazard_id:
            raise ValueError(
                f"verification_links.md: {link_id} links hazard {hazard_id} to mitigation "
                f"{mitigation_id}, but {mitigation_id} belongs to {mitigation_hazard_id}"
            )
        hazard_to_verifications[hazard_id].append(link_id)

    validate_reference_paths(workspace, verification_rows)

    missing_mitigations = [hazard_id for hazard_id, links in hazard_to_mitigations.items() if not links]
    if missing_mitigations:
        raise ValueError(
            "hazards without mitigations: " + ", ".join(sorted(missing_mitigations))
        )

    missing_verifications = [hazard_id for hazard_id, links in hazard_to_verifications.items() if not links]
    if missing_verifications:
        raise ValueError(
            "hazards without verification links: " + ", ".join(sorted(missing_verifications))
        )

    print(
        "[safety-case] PASS: "
        f"{len(hazard_rows)} hazards, {len(mitigation_rows)} mitigations, "
        f"{len(verification_rows)} verification links are internally consistent."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
