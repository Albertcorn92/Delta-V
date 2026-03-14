# Reference Payload Profile

Date: 2026-03-14
Scope: DELTA-V public reference mission baseline

This document defines the public reference payload used by DELTA-V for process,
interface, and safety-case work. It is a simple low-rate civilian technology-
demonstration payload profile, not a mission-qualified instrument.

## Payload Component

- Component: `PayloadMonitorComponent`
- Topology ID: `400`
- Instance: `payload_monitor`
- Rate group: `norm` (1 Hz)
- Purpose: model a bounded low-rate payload that can be enabled during nominal
  operations windows and produce one scalar sample for downlink/logging

## Commands

| Command | Opcode | Class | Meaning |
|---|---|---|---|
| `PAYLOAD_SET_ENABLE` | `1` | `OPERATIONAL` | `arg >= 0.5` enables the payload profile, otherwise disables it |
| `PAYLOAD_CAPTURE_SAMPLE` | `2` | `RESTRICTED` | Captures one bounded sample when the payload is enabled |
| `PAYLOAD_SET_GAIN` | `3` | `HOUSEKEEPING` | Sets bounded payload gain (`0.1-10.0`) |

## Telemetry

- Telemetry component ID: `400`
- `data_payload` carries the last captured sample when enabled
- `data_payload` returns `0.0` when the payload profile is disabled

## Safety And Ops Notes

- `PAYLOAD_CAPTURE_SAMPLE` is intentionally `RESTRICTED` so it is blocked
  outside nominal operations by the mission FSM.
- Capture requests are rejected locally if the payload profile is disabled.
- The public profile is not a deployment, release, or weaponization mechanism.
- Mission payload teams must still replace this profile with real payload
  hazards, constraints, and interface closure for operational use.
