# DELTA-V High-Assurance Roadmap

Date: 2026-03-06

## Software Baseline (current)

- `software_final` passes and synchronizes release artifacts.
- Requirements traceability is complete (`33/33` requirements with direct tests).
- Qualification bundle passes (`flight_readiness`, `vnv_stress`, `tidy_safety`, `traceability`).
- Performance regression protection is active (`benchmark_guard`).
- Runtime health protection is active (`sitl_smoke` + `sitl_soak`).
- Civilian legal scope checks pass (`tools/legal_compliance_check.py`).

## Remaining Gaps to Mission-Deployable Grade (Non-Software-Dominant)

1. On-target assurance evidence
- Run and archive ESP32 HIL campaign logs (fault injection, long soak, restart behavior).
- Run `tools/esp32_runtime_guard.py` and archive WCET/stack evidence.
- Run `tools/esp32_reboot_campaign.py` and archive reboot-cycle stability evidence.

2. Program/process assurance
- Maintain independent review records and approval workflow for safety-critical changes.
- Track tool qualification/deviation records required by mission governance.

3. Deployment security posture
- Current baseline intentionally excludes command-path crypto/auth to keep export/legal complexity low.
- Mission users must define deployment security controls outside this baseline.

## Optional Software Enhancements (not blocking baseline finalization)

1. Tighten coverage thresholds over time (`docs/COVERAGE_THRESHOLDS.json`).
2. Expand performance benchmark matrix (startup latency, CPU trend, memory trend).
3. Add static-analysis deviation ledger for long-term MISRA/CERT traceability.
