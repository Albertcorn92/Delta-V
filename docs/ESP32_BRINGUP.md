# ESP32 Bring-Up and Sensorless Validation

Date: 2026-03-06

## Goal

Run DELTA-V on ESP32-S3 in a stable, local-only, civilian profile that does not
require external sensors.

## Validated Profile (Current Baseline)

The current ESP32 baseline is configured as:

- `DELTAV_LOCAL_ONLY=ON` (UDP bridge disabled),
- `DELTAV_ESP_SAFE_MODE=OFF` (full framework runtime),
- `DELTAV_ALLOW_IMU_SIMULATION=ON` (no physical IMU required).

This profile boots the full framework topology and runs a cooperative embedded
scheduler on-target.

## Quick Start (ESP32-S3)

1. Export ESP-IDF environment:
- `source $HOME/esp/esp-idf/export.sh`

2. Build and flash:
- `cd ports/esp32`
- `idf.py set-target esp32s3`
- `idf.py -B build_esp32 build flash -p /dev/cu.usbmodem101`

3. Open monitor:
- `idf.py -B build_esp32 -p /dev/cu.usbmodem101 monitor`

## 3-5 Minute Hardware Smoke Test

Run monitor for 3-5 minutes and check for both expected startup lines and
absence of failure signatures.

Expected startup lines:

- `[Boot] HAL: ESP32 I2C hardware driver`
- `[RadioLink] Local-only mode: UDP bridge disabled.`
- `[RGE] Embedded cooperative scheduler running.`
- `[IMU_01] WARN: Hardware IMU missing. Using simulation fallback.`

Failure signatures (must not appear):

- `stack overflow`
- `assert failed`
- `Guru Meditation`
- repeating reboot banners (`ESP-ROM ...` loop)

## If Flash/Boot Gets Stuck

If serial shows `boot:0x0 (DOWNLOAD...)` or `waiting for download`:

1. Release `BOOT`.
2. Remove any jumper tying `GPIO0` to `GND`.
3. Tap `RESET` once.
4. Re-open monitor.

## Current Limitations

- No real I2C sensor validation yet (simulation fallback only).
- No long-duration soak evidence yet (hours/days).
- No mission-specific HIL campaign evidence yet.

## Next Hardware Tests (When Parts Are Available)

1. Real IMU data-path verification (I2C address `0x68`).
2. Command path + anti-replay checks with real uplink source.
3. FDIR transition tests under injected fault conditions.
4. Multi-hour soak test with reboot-cycle statistics.
