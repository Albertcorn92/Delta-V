# DELTA-V ESP32 Golden-Image Bootchain (Civilian CRC Profile)

This profile hardens OTA recovery for civilian/open-source missions without
encryption or defense-specific features.

## What It Adds

- A fixed partition layout with:
  - immutable `factory` image
  - dual OTA slots (`ota_0`, `ota_1`)
  - ESP-IDF OTA state (`otadata`)
- CRC32-only candidate verification metadata (`ota_candidate.meta`)
- A repeatable bootchain validation script:
  - `python3 tools/golden_bootchain_check.py`

## Partition Layout

`ports/esp32/partitions_golden.csv` is the reference table used by
`ports/esp32/sdkconfig.defaults`.

Required entries:

- `factory`
- `ota_0`
- `ota_1`
- `otadata`

## OTA Handoff

When `OtaComponent` stages an image, host/SITL writes:

- `ota_candidate.bin`
- `ota_candidate.meta` with:
  - `bytes=<count>`
  - `crc32=0xXXXXXXXX`

This remains checksum-based only (no payload encryption).

## Validation Commands

```bash
python3 tools/golden_bootchain_check.py \
  --partitions ports/esp32/partitions_golden.csv \
  --candidate-image ota_candidate.bin \
  --manifest ota_candidate.meta
```

Optional factory image check:

```bash
python3 tools/golden_bootchain_check.py \
  --factory-image ports/esp32/build_esp32/delta_v_esp32_s3.bin
```

## Operational Note

For flight hardware, keep the bootloader partition immutable and use ESP-IDF
rollback state so a bad OTA slot returns to the known-good image. This repo
provides the open civilian scaffolding and validation workflow; mission teams
still need final board-level qualification.
