#!/usr/bin/env python3
"""Validate DELTA-V ESP32 golden-image bootchain artifacts (CRC-only)."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import sys
import zlib


REQUIRED_PARTITIONS = ("otadata", "factory", "ota_0", "ota_1")


@dataclass(frozen=True)
class OtaManifest:
    expected_bytes: int | None
    expected_crc32: int | None


def parse_partition_table(path: Path) -> dict[str, dict[str, str]]:
    if not path.exists():
        raise ValueError(f"partition table not found: {path}")
    rows: dict[str, dict[str, str]] = {}
    for line_no, raw in enumerate(path.read_text().splitlines(), start=1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        cols = [c.strip() for c in line.split(",")]
        if len(cols) < 5:
            raise ValueError(f"{path}:{line_no}: malformed row")
        name, ptype, subtype, offset, size = cols[:5]
        rows[name] = {
            "type": ptype,
            "subtype": subtype,
            "offset": offset,
            "size": size,
        }
    return rows


def parse_int(text: str) -> int:
    return int(text.strip(), 0)


def parse_manifest(path: Path) -> OtaManifest:
    if not path.exists():
        return OtaManifest(expected_bytes=None, expected_crc32=None)

    expected_bytes: int | None = None
    expected_crc32: int | None = None
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if not line or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip()
        if key == "bytes":
            expected_bytes = parse_int(value)
        elif key == "crc32":
            expected_crc32 = parse_int(value)
    return OtaManifest(expected_bytes=expected_bytes, expected_crc32=expected_crc32)


def compute_crc32(path: Path) -> int:
    crc = 0
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            crc = zlib.crc32(chunk, crc)
    return crc & 0xFFFFFFFF


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check DELTA-V golden-image bootchain artifacts (checksum-only profile)."
    )
    parser.add_argument(
        "--partitions",
        default="ports/esp32/partitions_golden.csv",
        help="Path to partition table CSV",
    )
    parser.add_argument(
        "--factory-image",
        default="",
        help="Optional factory image binary path",
    )
    parser.add_argument(
        "--candidate-image",
        default="ota_candidate.bin",
        help="OTA candidate binary path",
    )
    parser.add_argument(
        "--manifest",
        default="ota_candidate.meta",
        help="OTA manifest path (bytes + crc32)",
    )
    parser.add_argument(
        "--expected-crc32",
        default="",
        help="Override expected CRC32 value (hex or decimal)",
    )
    args = parser.parse_args()

    partition_rows = parse_partition_table(Path(args.partitions))
    missing = [name for name in REQUIRED_PARTITIONS if name not in partition_rows]
    if missing:
        raise ValueError(
            f"partition table missing required entries: {', '.join(missing)}"
        )

    print("[golden-bootchain] partition table OK")

    if args.factory_image:
        factory = Path(args.factory_image)
        if not factory.exists() or factory.stat().st_size == 0:
            raise ValueError(f"factory image missing or empty: {factory}")
        print(f"[golden-bootchain] factory image OK ({factory.stat().st_size} bytes)")

    candidate = Path(args.candidate_image)
    manifest = parse_manifest(Path(args.manifest))
    expected_crc32 = (
        parse_int(args.expected_crc32)
        if args.expected_crc32
        else manifest.expected_crc32
    )

    if candidate.exists():
        size = candidate.stat().st_size
        if size == 0:
            raise ValueError(f"candidate image is empty: {candidate}")
        actual_crc32 = compute_crc32(candidate)
        print(f"[golden-bootchain] candidate image: {size} bytes, crc32=0x{actual_crc32:08X}")

        if manifest.expected_bytes is not None and manifest.expected_bytes != size:
            raise ValueError(
                f"candidate size mismatch: expected {manifest.expected_bytes}, got {size}"
            )
        if expected_crc32 is not None and expected_crc32 != actual_crc32:
            raise ValueError(
                f"candidate crc mismatch: expected 0x{expected_crc32:08X}, got 0x{actual_crc32:08X}"
            )
        if expected_crc32 is None:
            print("[golden-bootchain] note: no expected CRC provided; compare manually if needed")
    else:
        print(f"[golden-bootchain] note: candidate image not found at {candidate}")

    print("[golden-bootchain] PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValueError as exc:
        print(f"[golden-bootchain] FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
