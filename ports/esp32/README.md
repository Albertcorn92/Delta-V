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

Reference docs:

- `docs/ESP32_BRINGUP.md`
- `docs/ESP32_SENSORLESS_BASELINE.md`
