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

Automated soak command (writes `artifacts/esp32_soak_*.log`):

```bash
source $HOME/esp/esp-idf/export.sh
python3 tools/esp32_soak.py \
  --project-dir ports/esp32 \
  --build-dir build_esp32 \
  --port /dev/cu.usbmodem101 \
  --duration 1800
```

Runtime WCET/stack guard command:

```bash
source $HOME/esp/esp-idf/export.sh
python3 tools/esp32_runtime_guard.py \
  --project-dir ports/esp32 \
  --build-dir build_esp32 \
  --port /dev/cu.usbmodem101 \
  --duration 300
```

Reboot campaign command:

```bash
source $HOME/esp/esp-idf/export.sh
python3 tools/esp32_reboot_campaign.py \
  --project-dir ports/esp32 \
  --build-dir build_esp32 \
  --port /dev/cu.usbmodem101 \
  --cycles 10 \
  --cycle-seconds 12
```

## Latest Run Evidence (2026-03-07 UTC)

- Build/flash: PASS (`idf.py -B build_esp32 build flash -p /dev/cu.usbmodem101`).
- Sensorless soak: PASS (`300s`) via `tools/esp32_soak.py`.
- Evidence log: `artifacts/esp32_soak_20260307T005636Z.log`.
- Extended sensorless soak: PASS (`1800s`) via `tools/esp32_soak.py`.
- Evidence log: `artifacts/esp32_soak_20260307T010302Z.log`.
- No detected panic/assert/stack-overflow/reboot-loop signatures in the soak window.

## Known Gaps

- No real I2C sensor validation yet.
- No extended soak (1h+) evidence committed yet.
- No runtime guard (`tools/esp32_runtime_guard.py`) artifact committed yet.
- No reboot campaign (`tools/esp32_reboot_campaign.py`) artifact committed yet.
- No mission-specific HIL/fault campaign logs yet.
