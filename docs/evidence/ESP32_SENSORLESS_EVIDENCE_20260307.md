# ESP32 Sensorless Evidence Summary (Public-Shareable)

Date: 2026-03-07
Board: ESP32-S3 Dev Module
Profile: Local-only, sensorless-capable baseline

This file is a public summary of latest ESP32 baseline evidence.
Raw host logs/reports remain in local `artifacts/` (gitignored).

## Results

- Build/flash: PASS (`idf.py -B build_esp32 build flash -p /dev/cu.usbmodem101`)
- Sensorless soak (300s): PASS
- Sensorless soak (1800s): PASS
- Reboot campaign: PASS (`10/10` cycles)
- Runtime guard: PASS (`17` samples, `0` overruns)
- No panic/assert/stack-overflow/reboot-loop signatures detected during soak windows

## Local Raw Artifact Filenames

- `esp32_soak_20260307T005636Z.log`
- `esp32_soak_20260307T010302Z.log`
- `esp32_reboot_campaign_20260307T072234Z.log`
- `esp32_reboot_campaign_20260307T072234Z.json`
- `esp32_runtime_guard_20260307T071843Z.log`
- `esp32_runtime_guard_20260307T071843Z.json`
