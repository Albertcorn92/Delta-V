# Operations Runbook (Civilian Baseline)

Date: 2026-03-07
Scope: DELTA-V framework simulation and local integration operations

This runbook defines first-line operations for the open-source DELTA-V baseline.
It is for civilian simulation/lab use and does not replace mission authority
procedures for live spacecraft operations.

## 1) Startup

1. Run legal and software gate checks:
   - `python3 tools/legal_compliance_check.py`
   - `cmake --build build --target software_final`
2. Start flight software:
   - `./build/flight_software`
3. Start GDS in a second terminal:
   - `streamlit run gds/gds_dash.py`
4. Confirm startup markers:
   - `[Topology] All ports connected.`
   - `[RGE] Tiers ready - FAST=...`

## 2) Nominal Pass Procedure

1. Confirm telemetry updates in GDS and no critical startup events.
2. Send one housekeeping command (`RESET_BATTERY`) and verify response.
3. Verify event stream stays INFO/WARN only for nominal run.

## 3) Contingency Drills

1. Safe-mode command gate check:
   - Verify OPERATIONAL command is blocked in SAFE_MODE.
2. Link/replay resilience check:
   - Verify duplicate/replayed sequence frames are rejected.
3. Restart behavior check:
   - Verify watchdog restart escalation logic passes automated tests.

## 4) Go/No-Go Rules (Simulation Scope)

- `GO` when software gates pass and no unhandled runtime failures are present.
- `NO-GO` on any failed gate, failed replay protection check, or critical event.

## 5) Escalation Path

- Flight software lead: triage software gate failures and command-path anomalies.
- Ground segment lead: triage GDS ingest/visualization issues.
- Independent reviewer: verify evidence completeness before release update.

## 6) Scope Limits

- Hardware environmental qualification and mission RF chain validation are
  mission-team responsibilities and must be executed outside this repo baseline.
