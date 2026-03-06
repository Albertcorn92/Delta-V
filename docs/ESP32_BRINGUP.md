# ESP32 Bring-Up Plan

Date: 2026-03-06

## Goal

Run DELTA-V on ESP32 with the same safety properties verified in SITL:

- command validation and anti-replay,
- watchdog-driven FDIR transitions,
- no heap allocation after initialization,
- deterministic component scheduling.

## Pre-Flight Checklist

1. Toolchain
- Install ESP-IDF and verify `idf.py --version`.
- Pin SDK/toolchain versions in your project release notes.

2. Build configuration
- Build with `ESP_PLATFORM` defined.
- Verify HAL selection logs show `ESP32 I2C hardware driver`.

3. Hardware interface checks
- Confirm I2C pins, pull-ups, and MPU6050 address (`0x68`).
- Validate IMU wake write (`reg 0x6B <- 0x00`) succeeds.

4. Safety checks on target
- Confirm `HeapGuard::arm()` is reached after all init steps.
- Send valid and invalid uplink packets; verify:
  - valid packets dispatch,
  - malformed or wrong-length packets increment reject count,
  - replay packets are rejected.
- Force battery thresholds and verify state transitions:
  - `NOMINAL -> DEGRADED -> SAFE_MODE -> EMERGENCY`.

5. Runtime soak
- Run for at least 1 hour with telemetry active.
- Record:
  - watchdog heartbeat cadence,
  - command reject counts,
  - frame-drop counters,
  - reset/restart events.

## Recommended First Hardware Tests

1. IMU smoke test
- Boot and verify IMU telemetry is produced each fast cycle.

2. Command path test
- Send one valid command frame to battery component and verify ACK + behavior change.

3. FDIR threshold test
- Inject battery level command sequence to hit each threshold.

4. Anti-replay test
- Send same sequence twice and verify second command is rejected.

## Exit Criteria Before Mission Prototyping

- No runtime crashes during soak run.
- All command security behaviors observed on hardware.
- All FDIR mode transitions observed and logged.
- No post-init heap violations.
