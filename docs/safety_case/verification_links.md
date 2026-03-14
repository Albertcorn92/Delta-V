# Verification Links (Reference Mission Baseline)

Date: 2026-03-14

| Link ID | Hazard ID | Mitigation ID | Evidence Type | Evidence Path | Result | Reviewer |
|---|---|---|---|---|---|---|
| VL-001 | HZ-001 | MT-001 | Unit test | `tests/unit_tests.cpp` (`TelemetryBridge.RejectsReplaySequence`) | PASS | Maintainer |
| VL-002 | HZ-001 | MT-001 | Unit test | `tests/unit_tests.cpp` (`TelemetryBridge.PersistsReplaySequenceAcrossRestart`) | PASS | Maintainer |
| VL-003 | HZ-002 | MT-002 | Unit test | `tests/unit_tests.cpp` (`TelemetryBridge.RejectsInvalidHeaderFields`) | PASS | Maintainer |
| VL-004 | HZ-002 | MT-002 | Unit test | `tests/unit_tests.cpp` (`TelemetryBridge.RejectsNonCanonicalFrameLength`) | PASS | Maintainer |
| VL-005 | HZ-002 | MT-002 | Unit test | `tests/unit_tests.cpp` (`TelemetryBridge.RejectsTruncatedFrame`) | PASS | Maintainer |
| VL-006 | HZ-003 | MT-003 | Unit test | `tests/unit_tests.cpp` (`CommandHubFixture.NackWhenRouteQueueIsFull`) | PASS | Maintainer |
| VL-007 | HZ-004 | MT-004 | Unit test | `tests/unit_tests.cpp` (`TimeService.OverflowWarningThreshold`) | PASS | Maintainer |
| VL-008 | HZ-005 | MT-005 | Unit test | `tests/unit_tests.cpp` (`ParamDb.IntegrityAfterWrite`) | PASS | Maintainer |
| VL-009 | HZ-001 | MT-001 | Qualification gate | `docs/qualification_report.md` | PASS | Maintainer |
| VL-010 | HZ-003 | MT-003 | Software-final gate | `docs/SOFTWARE_FINAL_STATUS.md` | PASS | Maintainer |
| VL-011 | HZ-001 | MT-001 | SITL fault campaign | `build/sitl/sitl_fault_campaign_result.json` | PASS | Maintainer |
| VL-012 | HZ-002 | MT-002 | SITL fault campaign | `build/sitl/sitl_fault_campaign_result.json` | PASS | Maintainer |
| VL-013 | HZ-001 | MT-001 | Operations rehearsal | `docs/process/OPERATIONS_REHEARSAL_20260314.md` | PASS | Maintainer |
| VL-014 | HZ-002 | MT-002 | Operations rehearsal | `docs/process/OPERATIONS_REHEARSAL_20260314.md` | PASS | Maintainer |
| VL-015 | HZ-006 | MT-006 | Unit test | `tests/unit_tests.cpp` (`PayloadMonitorComponent.CaptureRejectedWhenDisabled`) | PASS | Maintainer |
| VL-016 | HZ-006 | MT-006 | Unit test | `tests/unit_tests.cpp` (`PayloadMonitorComponent.EnableGainAndCaptureSample`) | PASS | Maintainer |
| VL-017 | HZ-006 | MT-006 | SITL fault campaign | `build/sitl/sitl_fault_campaign_result.json` | PASS | Maintainer |

## Maintenance Rules

- Prefer immutable evidence paths tied to release artifacts.
- For hardware evidence, include `artifacts/*.log` and `artifacts/*.json`.
- Update this table whenever test names or evidence paths move.
