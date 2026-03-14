# DELTA-V Software Safety Plan Baseline

Date: 2026-03-14
Scope: public DELTA-V framework baseline

This document is the software safety planning baseline for the public DELTA-V
repository. It describes how the repository treats safety-significant software
paths, what evidence is required before publication, and what remains outside
the public baseline.

It is not a mission-approved software safety plan. Mission operators must
replace or tailor it under their own authority, hazard classification, and
hardware context.

## 1. Safety Scope

Safety-significant software in the public baseline includes:

- runtime scheduling and thread release behavior
- mode/state isolation and safe-mode transitions
- watchdog and recovery behavior
- command validation, anti-replay checks, and mission-state gating
- parameter integrity and majority-vote storage paths
- generated topology wiring that affects command, telemetry, or event routing

Support tooling that can influence safety evidence includes:

- `tools/autocoder.py`
- `tools/software_final_check.py`
- `tools/qualification_report.py`
- `tools/release_pedigree.py`

## 2. Safety Objectives

- prevent unsafe command acceptance in the public baseline
- fail loudly on invalid topology or registration conditions
- keep runtime behavior bounded and observable
- keep safety documentation tied to tests and generated evidence
- stop publication when blocking software gates fail

## 3. Safety-Critical Assumptions

- the public baseline is used for civilian/scientific/educational work only
- protected uplink security controls are outside the published baseline
- the reference mission is uncrewed and mission-loss is the dominant
  consequence for major software faults
- mission hardware limits, payload constraints, and ground authorization remain
  mission-owned

## 4. Safety Evidence Required In The Repository

- requirements traceability with direct tests
- `flight_readiness` passing
- `qualification_bundle` passing
- `software_final` passing
- current safety-case files in `docs/safety_case/`
- current process baseline documents in `docs/process/`
- current SITL runtime/fault evidence and long-soak status summary

## 5. Safety Verification Activities

- unit and system tests for FDIR, command routing, replay rejection, parameter
  integrity, and mode handling
- static analysis through `tidy_safety`
- shuffled/repeated verification through `vnv_stress`
- host/SITL smoke, soak, and live fault campaigns
- review of generated topology artifacts through `autocoder_check`

## 6. Change-Control Rules

- changes to safety-significant runtime paths require updated tests or trace
  links before release
- changes that affect generated outputs require regeneration and freshness checks
- changes that affect evidence or safety-case records require updated
  documentation in the same baseline
- public release publication additionally requires `release_candidate`

## 7. Safety Limits Of This Baseline

- no independent software assurance authority is provided by this repository
- no sensor-attached HIL closure is claimed
- no mission hardware environmental qualification is claimed
- no mission operational approval is claimed

## 8. Handoff To Mission Programs

Mission programs adopting DELTA-V must provide:

- mission hazard classification and approval authority
- mission-specific software safety analysis and signoff
- hardware and payload interface safety closure
- protected uplink/security controls if operational exposure requires them
- independent reviewers as required by the adopting program
