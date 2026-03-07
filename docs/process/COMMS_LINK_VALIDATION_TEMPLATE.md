# Comms Link Validation Template

Date: YYYY-MM-DD
Mission: `<mission_name>`
Release: `<git_tag_or_commit>`

Use this template to document end-to-end command/telemetry validation for mission operations.

## Link Configuration

- Ground station ID: `<id>`
- Radio profile: `<frequency/modulation/data-rate>`
- Command path mode: `<local-only / mission link>`
- Time sync source: `<source>`

## Validation Runs

| Run ID | Scenario | Uplink Commands Sent | Ack Rate | Packet Loss | Latency p95 | Result | Evidence Path |
|---|---|---:|---:|---:|---:|---|---|
| `<run_01>` | Nominal pass | 0 | 0% | 0% | 0 ms | NOT RUN | `<path>` |
| `<run_02>` | Interference/weak link | 0 | 0% | 0% | 0 ms | NOT RUN | `<path>` |
| `<run_03>` | Recovery after link drop | 0 | 0% | 0% | 0 ms | NOT RUN | `<path>` |

## Safety Checks

- Command approval workflow verified: `YES/NO`
- Safe-mode command set verified: `YES/NO`
- Replay/sequence behavior verified: `YES/NO`

## Notes

- Attach logs and operator console captures for each run.
