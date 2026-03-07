# ESP32 Sensor-Attached Evidence Template

Date: YYYY-MM-DD
Board: `<esp32_board>`
Firmware commit: `<git_commit>`

Use this template to archive runs with real attached sensors (not simulation fallback).

## Hardware Setup

- Sensor model: `<part>`
- Bus/interface: `<i2c/spi/other>`
- Wiring/connector notes: `<notes>`
- Power source: `<notes>`

## Validation Runs

| Run ID | Duration (s) | Boot OK | Sensor read OK | Runtime guard | Reboot campaign | Result | Evidence Path |
|---|---:|---|---|---|---|---|---|
| `<run_01>` | 0 | NO | NO | NO | NO | NOT RUN | `<path>` |
| `<run_02>` | 0 | NO | NO | NO | NO | NOT RUN | `<path>` |

## Notes

- Include monitor logs and JSON reports from `artifacts/`.
- Record any intermittent read failures and mitigation actions.
