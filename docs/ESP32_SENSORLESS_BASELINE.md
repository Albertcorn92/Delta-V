# ESP32 Sensorless Baseline

Date: 2026-03-06
Board: ESP32-S3 Dev Module
Host OS: macOS

## Purpose

Record the minimum on-target validation that can be run with no external
sensors and no network dependency.

## Baseline Configuration

- `DELTAV_LOCAL_ONLY=ON`
- `DELTAV_ESP_SAFE_MODE=OFF`
- `DELTAV_ALLOW_IMU_SIMULATION=ON`

## Verified Behaviors

1. Device flashes successfully over USB serial (`/dev/cu.usbmodem101`).
2. Device boots into app mode (`boot:0x8 SPI_FAST_FLASH_BOOT`).
3. Framework topology initializes on-target:
- `Supervisor`, `TelemHub`, `CmdHub`, `EventHub`, `BlackBox`,
  `BatterySystem`, `StarTracker`, `IMU_01`, `RadioLink`.
4. Local-only networking policy is active:
- `[RadioLink] Local-only mode: UDP bridge disabled.`
5. Embedded scheduler starts and runs:
- `[RGE] Embedded cooperative scheduler running.`
6. Sensorless IMU fallback is active without hardware:
- `[IMU_01] WARN: Hardware IMU missing. Using simulation fallback.`
7. 3+ minute monitor window completes without:
- `stack overflow`
- `assert failed`
- panic/Guru backtrace
- reboot loop

## Command Set Used

```bash
source $HOME/esp/esp-idf/export.sh
cd ports/esp32
idf.py -B build_esp32 -p /dev/cu.usbmodem101 flash
idf.py -B build_esp32 -p /dev/cu.usbmodem101 monitor
```

## Known Gaps

- No real I2C sensor validation yet.
- No extended soak (1h+) evidence yet.
- No mission-specific HIL/fault campaign logs yet.
