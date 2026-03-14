# Operations Rehearsal Record

Date: 2026-03-14
Scope: DELTA-V reference mission simulation and local integration baseline

This record captures the reference-mission operations rehearsal completed on a
Mac host for the public baseline. It is not a live mission-ops certification.

## Executed Commands

```bash
cmake --build build --target sitl_fault_campaign
cmake --build build --target sitl_soak
cmake --build build --target software_final
```

## Rehearsed Scenarios

| Scenario ID | Scenario | Expected Result | Result | Evidence |
|---|---|---|---|---|
| OPS-001 | Startup and topology bring-up | Flight software reaches topology-connected and scheduler-ready markers | PASS | `build/sitl/sitl_fault_campaign.log` |
| OPS-002 | Nominal command dispatch during pass | Valid command reaches `BatterySystem` and updates state | PASS | `build/sitl/sitl_fault_campaign.log` (`CMD seq=100`, drain update, reset) |
| OPS-003 | Replay rejection | Duplicate command sequence is rejected without process failure | PASS | `build/sitl/sitl_fault_campaign_result.json` |
| OPS-004 | Malformed uplink rejection | Invalid sync/APID/length/random frames are rejected and valid traffic still survives | PASS | `build/sitl/sitl_fault_campaign_result.json` |
| OPS-005 | Sustained runtime stability | 10-minute host soak completes with no fatal signatures | PASS | `build/sitl/sitl_soak_result.json` |
| OPS-006 | Software closeout discipline | Integrated software gate path completes successfully | PASS | `docs/SOFTWARE_FINAL_STATUS.md` |
| OPS-007 | Reference payload window handling | Payload enable/gain/capture path succeeds and event log records the operation | PASS | `build/sitl/sitl_fault_campaign_result.json`, `build/flight_events.log` |
| OPS-008 | Unknown-target rejection | Command to undefined target produces a NACK event instead of silent success | PASS | `build/flight_events.log` |

## Operator-Facing Conclusions

- The reference mission baseline supports a repeatable nominal-pass rehearsal on
  the host.
- The public baseline now includes a live malformed/replay uplink drill, not
  just unit-level parser coverage.
- The rehearsal remains simulation/local-integration scope. It does not close
  RF operations, mission staffing, or flight hardware behavior.

## Follow-On Rehearsals Still Needed

- Archived 1-hour, 6-hour, 12-hour, and 24-hour host/SITL campaigns.
- Sensor-attached HIL operations rehearsal.
- Mission-team staffed anomaly-response rehearsal.
