#!/usr/bin/env python3
"""
review_bundle.py

Assemble a curated DELTA-V reviewer bundle from current documentation and
build artifacts. This is for technical review and portfolio evaluation, not for
tagged public release pedigree.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import zipfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


def require_file(path: Path) -> None:
    if not path.exists():
        raise FileNotFoundError(f"missing required artifact: {path}")


def digest256_file(path: Path) -> str:
    digest = hashlib.blake2b(digest_size=32)
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def copy_item(src: Path, dst: Path) -> dict[str, str]:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    return {
        "source": str(src),
        "bundle_path": str(dst),
        "digest256": digest256_file(dst),
    }


def write_bundle_readme(path: Path, manifest: dict[str, Any]) -> None:
    lines: list[str] = []
    lines.append("# DELTA-V Review Bundle")
    lines.append("")
    lines.append(f"- Generated (UTC): `{manifest['generated_utc']}`")
    lines.append(f"- Workspace: `{manifest['workspace']}`")
    lines.append(f"- Build directory: `{manifest['build_dir']}`")
    lines.append("")
    lines.append("## What This Bundle Is")
    lines.append("")
    lines.append(
        "This bundle is a curated reviewer package for technical evaluation of the "
        "DELTA-V repository."
    )
    lines.append("It is not a tagged release pedigree and it is not a flight-readiness claim.")
    lines.append("")
    lines.append("## Open These First")
    lines.append("")
    lines.append("1. `docs/ASSESS_DELTA_V.md`")
    lines.append("2. `docs/REFERENCE_MISSION_WALKTHROUGH.md`")
    lines.append("3. `docs/SOFTWARE_FINAL_STATUS.md`")
    lines.append("4. `docs/CUBESAT_READINESS_STATUS.md`")
    lines.append("5. `docs/qualification_report.md`")
    lines.append("6. `docs/process/RELEASE_PREFLIGHT_CURRENT.md`")
    lines.append("")
    lines.append("## Included Groups")
    lines.append("")
    for group in manifest["groups"]:
        lines.append(f"- `{group['name']}`: {len(group['items'])} file(s)")
    lines.append("")
    lines.append("## Important Limits")
    lines.append("")
    lines.append("- The public baseline is civilian/scientific/educational only.")
    lines.append("- The public baseline intentionally excludes protected uplink security features.")
    lines.append("- Sensor-attached HIL and hardware qualification are not closed in this bundle.")
    lines.append("- For a clean tagged public release unit, use `release_candidate`, not this bundle.")
    lines.append("")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def create_zip(bundle_dir: Path, zip_path: Path) -> None:
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(bundle_dir.rglob("*")):
            if path.is_dir():
                continue
            archive.write(path, path.relative_to(bundle_dir))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Assemble a curated DELTA-V reviewer bundle."
    )
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--zip-path", type=Path, required=True)
    args = parser.parse_args()

    workspace = args.workspace.resolve()
    build_dir = args.build_dir.resolve()
    output_dir = args.output_dir.resolve()
    zip_path = args.zip_path.resolve()

    groups_spec: list[tuple[str, list[tuple[Path, Path]]]] = [
        (
            "overview",
            [
                (workspace / "README.md", Path("README.md")),
                (workspace / "docs" / "ASSESS_DELTA_V.md", Path("docs/ASSESS_DELTA_V.md")),
                (
                    workspace / "docs" / "REFERENCE_MISSION_WALKTHROUGH.md",
                    Path("docs/REFERENCE_MISSION_WALKTHROUGH.md"),
                ),
                (workspace / "docs" / "README.md", Path("docs/README.md")),
                (workspace / "docs" / "ARCHITECTURE.md", Path("docs/ARCHITECTURE.md")),
                (workspace / "docs" / "ICD.md", Path("docs/ICD.md")),
            ],
        ),
        (
            "closeout",
            [
                (
                    workspace / "docs" / "SOFTWARE_FINAL_STATUS.md",
                    Path("docs/SOFTWARE_FINAL_STATUS.md"),
                ),
                (
                    workspace / "docs" / "qualification_report.md",
                    Path("docs/qualification_report.md"),
                ),
                (
                    workspace / "docs" / "qualification_report.json",
                    Path("docs/qualification_report.json"),
                ),
                (
                    workspace / "docs" / "REQUIREMENTS_TRACE_MATRIX.md",
                    Path("docs/REQUIREMENTS_TRACE_MATRIX.md"),
                ),
                (
                    workspace / "docs" / "REQUIREMENTS_TRACE_MATRIX.json",
                    Path("docs/REQUIREMENTS_TRACE_MATRIX.json"),
                ),
                (
                    workspace / "docs" / "BENCHMARK_BASELINE.md",
                    Path("docs/BENCHMARK_BASELINE.md"),
                ),
                (
                    workspace / "docs" / "BENCHMARK_BASELINE.json",
                    Path("docs/BENCHMARK_BASELINE.json"),
                ),
                (
                    workspace / "docs" / "CUBESAT_READINESS_STATUS.md",
                    Path("docs/CUBESAT_READINESS_STATUS.md"),
                ),
                (
                    workspace / "docs" / "CUBESAT_READINESS_STATUS_SCOPE.md",
                    Path("docs/CUBESAT_READINESS_STATUS_SCOPE.md"),
                ),
            ],
        ),
        (
            "process",
            [
                (
                    workspace / "docs" / "process" / "SOFTWARE_CLASSIFICATION_BASELINE.md",
                    Path("docs/process/SOFTWARE_CLASSIFICATION_BASELINE.md"),
                ),
                (
                    workspace / "docs" / "process" / "EXTERNAL_ASSURANCE_APPLICABILITY_BASELINE.md",
                    Path("docs/process/EXTERNAL_ASSURANCE_APPLICABILITY_BASELINE.md"),
                ),
                (
                    workspace / "docs" / "process" / "PSAC_DELTAV_BASELINE.md",
                    Path("docs/process/PSAC_DELTAV_BASELINE.md"),
                ),
                (
                    workspace / "docs" / "process" / "SCMP_DELTAV_BASELINE.md",
                    Path("docs/process/SCMP_DELTAV_BASELINE.md"),
                ),
                (
                    workspace / "docs" / "process" / "SQAP_DELTAV_BASELINE.md",
                    Path("docs/process/SQAP_DELTAV_BASELINE.md"),
                ),
                (
                    workspace / "docs" / "process" / "SVVP_DELTAV_BASELINE.md",
                    Path("docs/process/SVVP_DELTAV_BASELINE.md"),
                ),
                (
                    workspace / "docs" / "process" / "SOFTWARE_SAFETY_PLAN_BASELINE.md",
                    Path("docs/process/SOFTWARE_SAFETY_PLAN_BASELINE.md"),
                ),
                (
                    workspace / "docs" / "process" / "STATIC_ANALYSIS_DEVIATION_LOG.md",
                    Path("docs/process/STATIC_ANALYSIS_DEVIATION_LOG.md"),
                ),
                (
                    workspace / "docs" / "process" / "TAILORING_AND_SCOPE_DEVIATIONS_BASELINE.md",
                    Path("docs/process/TAILORING_AND_SCOPE_DEVIATIONS_BASELINE.md"),
                ),
                (
                    workspace / "docs" / "process" / "PUBLIC_SECURITY_POSTURE_BASELINE.md",
                    Path("docs/process/PUBLIC_SECURITY_POSTURE_BASELINE.md"),
                ),
                (
                    workspace / "docs" / "process" / "REFERENCE_MISSION_PROFILE.md",
                    Path("docs/process/REFERENCE_MISSION_PROFILE.md"),
                ),
                (
                    workspace / "docs" / "process" / "REFERENCE_PAYLOAD_PROFILE.md",
                    Path("docs/process/REFERENCE_PAYLOAD_PROFILE.md"),
                ),
                (
                    workspace / "docs" / "process" / "REFERENCE_MISSION_REQUIREMENTS_ALLOCATION.md",
                    Path("docs/process/REFERENCE_MISSION_REQUIREMENTS_ALLOCATION.md"),
                ),
                (
                    workspace / "docs" / "process" / "REFERENCE_MISSION_INTERFACE_CONTROL.md",
                    Path("docs/process/REFERENCE_MISSION_INTERFACE_CONTROL.md"),
                ),
                (
                    workspace / "docs" / "process" / "RISK_REGISTER_BASELINE.md",
                    Path("docs/process/RISK_REGISTER_BASELINE.md"),
                ),
                (
                    workspace / "docs" / "process" / "ASSUMPTIONS_LOG_BASELINE.md",
                    Path("docs/process/ASSUMPTIONS_LOG_BASELINE.md"),
                ),
                (
                    workspace / "docs" / "process" / "CONFIGURATION_AUDIT_BASELINE.md",
                    Path("docs/process/CONFIGURATION_AUDIT_BASELINE.md"),
                ),
                (
                    workspace / "docs" / "process" / "RELEASE_PREFLIGHT_CURRENT.md",
                    Path("docs/process/RELEASE_PREFLIGHT_CURRENT.md"),
                ),
                (
                    workspace / "docs" / "process" / "PROBLEM_REPORT_AND_CORRECTIVE_ACTION_LOG.md",
                    Path("docs/process/PROBLEM_REPORT_AND_CORRECTIVE_ACTION_LOG.md"),
                ),
                (
                    workspace / "docs" / "process" / "OPERATIONS_REHEARSAL_20260314.md",
                    Path("docs/process/OPERATIONS_REHEARSAL_20260314.md"),
                ),
                (
                    workspace / "docs" / "process" / "SITL_LONG_SOAK_STATUS.md",
                    Path("docs/process/SITL_LONG_SOAK_STATUS.md"),
                ),
            ],
        ),
        (
            "safety_case",
            [
                (
                    workspace / "docs" / "safety_case" / "README.md",
                    Path("docs/safety_case/README.md"),
                ),
                (
                    workspace / "docs" / "safety_case" / "hazards.md",
                    Path("docs/safety_case/hazards.md"),
                ),
                (
                    workspace / "docs" / "safety_case" / "mitigations.md",
                    Path("docs/safety_case/mitigations.md"),
                ),
                (
                    workspace / "docs" / "safety_case" / "verification_links.md",
                    Path("docs/safety_case/verification_links.md"),
                ),
                (
                    workspace / "docs" / "safety_case" / "fmea.md",
                    Path("docs/safety_case/fmea.md"),
                ),
                (
                    workspace / "docs" / "safety_case" / "fta.md",
                    Path("docs/safety_case/fta.md"),
                ),
            ],
        ),
        (
            "runtime_evidence",
            [
                (
                    build_dir / "sitl" / "sitl_fault_campaign_result.json",
                    Path("artifacts/sitl/sitl_fault_campaign_result.json"),
                ),
                (
                    build_dir / "sitl" / "sitl_fault_campaign.log",
                    Path("artifacts/sitl/sitl_fault_campaign.log"),
                ),
                (
                    build_dir / "sitl" / "sitl_soak_result.json",
                    Path("artifacts/sitl/sitl_soak_result.json"),
                ),
                (
                    build_dir / "sitl" / "sitl_soak.log",
                    Path("artifacts/sitl/sitl_soak.log"),
                ),
                (
                    build_dir / "qualification" / "qualification_report.md",
                    Path("artifacts/qualification/qualification_report.md"),
                ),
                (
                    build_dir / "qualification" / "qualification_report.json",
                    Path("artifacts/qualification/qualification_report.json"),
                ),
            ],
        ),
    ]

    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    manifest: dict[str, Any] = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "workspace": str(workspace),
        "build_dir": str(build_dir),
        "groups": [],
    }

    for group_name, items in groups_spec:
        group_items: list[dict[str, str]] = []
        for src, rel_dst in items:
            require_file(src)
            group_items.append(copy_item(src, output_dir / rel_dst))
        manifest["groups"].append({"name": group_name, "items": group_items})

    manifest_path = output_dir / "review_bundle_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    write_bundle_readme(output_dir / "README.md", manifest)

    zip_path.parent.mkdir(parents=True, exist_ok=True)
    if zip_path.exists():
        zip_path.unlink()
    create_zip(output_dir, zip_path)

    print(f"[review-bundle] OK: staged reviewer bundle -> {output_dir}")
    print(f"[review-bundle] ZIP: {zip_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
