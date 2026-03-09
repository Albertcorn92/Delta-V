# ESP32 Bring-Up and Sensorless Validation

Date: 2026-03-06

## Goal

Run DELTA-V on ESP32-S3 in a stable, local-only, civilian profile that does not
require external sensors.

## Validated Profile (Current Baseline)

The current ESP32 baseline is configured as:

- `DELTAV_LOCAL_ONLY=ON` (UDP bridge disabled),
- `DELTAV_DISABLE_NETWORK_STACK=ON` (Wi-Fi/LWIP/TLS disabled),
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

## Automated Sensorless Soak (Recommended)

Run this after flash to collect longer-run evidence and automatic pass/fail checks:

```bash
source $HOME/esp/esp-idf/export.sh
python3 tools/esp32_soak.py \
  --project-dir ports/esp32 \
  --build-dir build_esp32 \
  --port /dev/cu.usbmodem101 \
  --duration 1800
```

Notes:

- `--duration 1800` = 30 minutes. For stronger evidence, use `3600` (1 hour) or more.
- A timestamped log file is written under `artifacts/`.
- The soak fails if panic/assert/stack-overflow or reboot-loop signatures are detected.

## Runtime WCET + Stack-Margin Guard

Use this to collect and validate on-target scheduler runtime metrics.

```bash
source $HOME/esp/esp-idf/export.sh
python3 tools/esp32_runtime_guard.py \
  --project-dir ports/esp32 \
  --build-dir build_esp32 \
  --port /dev/cu.usbmodem101 \
  --duration 300
```

The guard enforces thresholds from `docs/ESP32_RUNTIME_THRESHOLDS.json`:

- FAST tick WCET upper bound (`fast_tick_wcet_us`)
- loop WCET upper bound (`loop_wcet_us`)
- loop overrun count upper bound (`loop_overruns`)
- stack high-watermark minimum (`stack_min_free_words`)

This script uses direct serial capture (no interactive `idf.py monitor` session
required).

## Reboot Stability Campaign

Use this to verify repeated startup behavior under local USB power.

```bash
source $HOME/esp/esp-idf/export.sh
python3 tools/esp32_reboot_campaign.py \
  --project-dir ports/esp32 \
  --build-dir build_esp32 \
  --port /dev/cu.usbmodem101 \
  --cycles 10 \
  --cycle-seconds 12
```

The campaign fails if any cycle misses required startup markers or logs panic/
assert/overflow signatures.

## Safety Note (Local Hardware)

Long soaks are generally safe for both host and board when using normal USB power:

- ESP32-S3 can run continuously at its default clock in this profile.
- Host load is typically low to moderate (serial monitor + log writes).
- Keep the board on a non-conductive surface with normal airflow.
- Remove accidental jumper connections unless explicitly needed.

## If Flash/Boot Gets Stuck

If serial shows `boot:0x0 (DOWNLOAD...)` or `waiting for download`:

1. Release `BOOT`.
2. Remove any jumper tying `GPIO0` to `GND`.
3. Tap `RESET` once.
4. Re-open monitor.

## Current Limitations

- No real I2C sensor validation yet (simulation fallback only).
- Automated soak evidence exists (`300s` and `1800s`), but no long-duration soak
  evidence yet (multiple hours/days).
- Sensor-attached HIL campaign evidence is not yet available.
- Reboot fault-campaign evidence is committed (`10/10` pass).
- Runtime WCET/stack guard now has PASS evidence (`17` samples, `0` overruns).

## Next Hardware Tests (When Parts Are Available)

1. Real IMU data-path verification (I2C address `0x68`).
2. Command path + anti-replay checks with real uplink source.
3. FDIR transition tests under injected fault conditions.
4. Multi-hour soak test with reboot-cycle statistics.
5. Re-run `esp32_runtime_guard` after each firmware/toolchain change and archive
   the new PASS JSON.
