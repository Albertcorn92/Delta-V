# Verification Links (Template)

Date: 2026-03-07

| Link ID | Hazard ID | Mitigation ID | Evidence Type | Evidence Path | Result | Reviewer |
|---|---|---|---|---|---|---|
| VL-001 | HZ-001 | MT-001 | Test | `tests/system_tests.cpp` | PASS | Name |
| VL-002 | HZ-001 | MT-001 | Gate report | `docs/qualification_report.md` | PASS | Name |

## Guidance

- Prefer immutable evidence paths tied to release artifacts.
- For hardware evidence, include `artifacts/*.log` and `artifacts/*.json`.
