#!/usr/bin/env python3
"""
DELTA-V legal compliance guard.

Fails if banned military terms appear in code-facing files or if required
policy documents are missing.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

EXCLUDED_DIRS = {
    ".git",
    "build",
    "build_cov",
    "build_release",
    "build_tidy",
    "venv",
    "__pycache__",
}

MILITARY_PATTERNS = [
    re.compile(r"\bweapon(s)?\b", re.IGNORECASE),
    re.compile(r"\bmissile(s)?\b", re.IGNORECASE),
    re.compile(r"\bwarhead(s)?\b", re.IGNORECASE),
    re.compile(r"\bmunition(s)?\b", re.IGNORECASE),
    re.compile(r"\btorpedo(s)?\b", re.IGNORECASE),
    re.compile(r"\btargeting\b", re.IGNORECASE),
    re.compile(r"\bfire[\s_-]?control\b", re.IGNORECASE),
    re.compile(r"\blethal\b", re.IGNORECASE),
    re.compile(r"\bcombat\b", re.IGNORECASE),
]

ENDORSEMENT_PATTERN = re.compile(r"\bnasa[- ]grade\b", re.IGNORECASE)

PROHIBITED_CLAIM_PATTERNS = [
    re.compile(r"\bliability[-\s]*free\b", re.IGNORECASE),
    re.compile(r"\bzero[-\s]*liability\b", re.IGNORECASE),
    re.compile(r"\b100%\s*legal\b", re.IGNORECASE),
    re.compile(r"\bitar[-\s]*free\b", re.IGNORECASE),
    re.compile(r"\bear[-\s]*free\b", re.IGNORECASE),
    re.compile(r"\bguaranteed\s+legal\b", re.IGNORECASE),
    re.compile(r"\bguaranteed\s+compliance\b", re.IGNORECASE),
    re.compile(r"\blegal\s+clearance\s+guaranteed\b", re.IGNORECASE),
]

CODE_SCAN_ROOTS = [
    ROOT / "src",
    ROOT / "tests",
    ROOT / "gds",
    ROOT / "tools",
]

CODE_SCAN_SUFFIXES = {
    ".c",
    ".cc",
    ".cpp",
    ".h",
    ".hpp",
    ".py",
    ".sh",
    ".yaml",
    ".yml",
    ".toml",
    ".json",
}

SELF_SKIP = ROOT / "tools" / "legal_compliance_check.py"

TEXT_SCAN_ROOTS = [ROOT / "README.md", ROOT / "docs", ROOT / "src", ROOT / "bootstrap.sh"]
TEXT_SCAN_SUFFIXES = CODE_SCAN_SUFFIXES | {".md", ".txt"}

REQUIRED_FILES = [
    ROOT / "LICENSE",
    ROOT / "docs" / "CIVILIAN_USE_POLICY.md",
    ROOT / "docs" / "EXPORT_CONTROL_NOTE.md",
    ROOT / "DISCLAIMER.md",
]

REQUIRED_PHRASES = [
    (
        ROOT / "README.md",
        "not certified for operational or safety-critical deployment",
    ),
    (
        ROOT / "DISCLAIMER.md",
        "not certified for safety-critical, life-critical, or operational flight use",
    ),
]


def should_skip(path: Path) -> bool:
    return any(part in EXCLUDED_DIRS for part in path.parts)


def iter_files(base: Path, suffixes: set[str]) -> list[Path]:
    if base.is_file():
        if base.suffix.lower() in suffixes or base.name in {"README.md", "bootstrap.sh"}:
            return [base]
        return []

    files: list[Path] = []
    for path in base.rglob("*"):
        if not path.is_file():
            continue
        if should_skip(path.relative_to(ROOT)):
            continue
        if path.suffix.lower() in suffixes:
            files.append(path)
    return files


def scan_for_patterns(paths: list[Path], patterns: list[re.Pattern[str]]) -> list[str]:
    violations: list[str] = []
    for path in paths:
        if path == SELF_SKIP:
            continue
        rel = path.relative_to(ROOT)
        try:
            content = path.read_text(encoding="utf-8", errors="ignore")
        except OSError as exc:
            violations.append(f"{rel}: unable to read file: {exc}")
            continue

        for lineno, line in enumerate(content.splitlines(), start=1):
            for pattern in patterns:
                if pattern.search(line):
                    violations.append(
                        f"{rel}:{lineno}: banned pattern '{pattern.pattern}' matched: {line.strip()}"
                    )
    return violations


def main() -> int:
    missing = [str(path.relative_to(ROOT)) for path in REQUIRED_FILES if not path.exists()]
    if missing:
        print("[legal] Missing required compliance files:")
        for item in missing:
            print(f"  - {item}")
        return 1

    missing_phrases: list[str] = []
    for file_path, phrase in REQUIRED_PHRASES:
        try:
            content = file_path.read_text(encoding="utf-8", errors="ignore")
        except OSError as exc:
            missing_phrases.append(
                f"{file_path.relative_to(ROOT)}: unable to read file: {exc}"
            )
            continue
        normalized_content = " ".join(content.lower().split())
        normalized_phrase = " ".join(phrase.lower().split())
        if normalized_phrase not in normalized_content:
            missing_phrases.append(
                f"{file_path.relative_to(ROOT)}: missing required phrase '{phrase}'"
            )

    if missing_phrases:
        print("[legal] Required legal language check failed.")
        for item in missing_phrases:
            print(f"  - {item}")
        return 1

    code_paths: list[Path] = []
    for root in CODE_SCAN_ROOTS:
        if root.exists():
            code_paths.extend(iter_files(root, CODE_SCAN_SUFFIXES))
    topology = ROOT / "topology.yaml"
    if topology.exists():
        code_paths.append(topology)

    military_violations = scan_for_patterns(code_paths, MILITARY_PATTERNS)

    text_paths: list[Path] = []
    for root in TEXT_SCAN_ROOTS:
        if root.exists():
            text_paths.extend(iter_files(root, TEXT_SCAN_SUFFIXES))

    endorsement_violations = scan_for_patterns(text_paths, [ENDORSEMENT_PATTERN])

    claim_violations = scan_for_patterns(text_paths, PROHIBITED_CLAIM_PATTERNS)

    violations = military_violations + endorsement_violations + claim_violations
    if violations:
        print("[legal] Compliance check failed.")
        for violation in violations:
            print(f"  - {violation}")
        return 1

    print("[legal] Compliance check passed.")
    print("[legal] Required files and policy scans are valid.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
