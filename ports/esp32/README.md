# DELTA-V ESP32-S3 Port (Sensorless Baseline)

This port runs DELTA-V on ESP32-S3 in a local-only profile without requiring
external sensors.

## Default Profile

- `DELTAV_LOCAL_ONLY=ON`
- `DELTAV_ESP_SAFE_MODE=OFF`
- `DELTAV_ALLOW_IMU_SIMULATION=ON`

## Build and Flash

```bash
source $HOME/esp/esp-idf/export.sh
cd ports/esp32
idf.py set-target esp32s3
idf.py -B build_esp32 build flash -p /dev/cu.usbmodem101
```

## Monitor

```bash
idf.py -B build_esp32 -p /dev/cu.usbmodem101 monitor
```

Expected lines:

- `[RadioLink] Local-only mode: UDP bridge disabled.`
- `[RGE] Embedded cooperative scheduler running.`
- `[IMU_01] WARN: Hardware IMU missing. Using simulation fallback.`

## 3-5 Minute Smoke Test

Run monitor for 3-5 minutes and confirm absence of:

- `stack overflow`
- `assert failed`
- `Guru Meditation`
- reboot loops

## Automated Soak

```bash
source $HOME/esp/esp-idf/export.sh
python3 tools/esp32_soak.py \
  --project-dir ports/esp32 \
  --build-dir build_esp32 \
  --port /dev/cu.usbmodem101 \
  --duration 1800
```

This writes timestamped log + JSON report artifacts under `artifacts/` and
validates required runtime markers plus failure signatures.

## Runtime WCET/Stack Guard

```bash
source $HOME/esp/esp-idf/export.sh
python3 tools/esp32_runtime_guard.py \
  --project-dir ports/esp32 \
  --build-dir build_esp32 \
  --port /dev/cu.usbmodem101 \
  --duration 300
```

This checks scheduler runtime metrics emitted by `[RGE_METRIC]` lines against
`docs/ESP32_RUNTIME_THRESHOLDS.json`.
The script reads USB serial directly, so it can run in non-interactive shells.

## Reboot Stability Campaign

```bash
source $HOME/esp/esp-idf/export.sh
python3 tools/esp32_reboot_campaign.py \
  --project-dir ports/esp32 \
  --build-dir build_esp32 \
  --port /dev/cu.usbmodem101 \
  --cycles 10 \
  --cycle-seconds 12
```

This captures startup stability statistics and fails if boot markers are missing
or panic/assert/overflow signatures appear in any cycle.

Reference docs:

- `docs/ESP32_BRINGUP.md`
- `docs/ESP32_SENSORLESS_BASELINE.md`
