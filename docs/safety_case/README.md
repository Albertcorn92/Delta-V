# Safety Case Starter (Mission Team Template)

Date: 2026-03-07

This folder is a starter structure for mission-program safety cases built on
top of DELTA-V software artifacts.

## Scope

- Software evidence from DELTA-V gates is necessary but not sufficient.
- Mission teams must add mission-specific hazards, mitigations, and independent
  verification records.

## Files

- `hazards.md`: hazard list and severity/classification.
- `mitigations.md`: control strategy linked to requirements and tests.
- `verification_links.md`: explicit evidence links from hazard to verification.
- `change_impact.md`: impact assessment for safety-relevant changes.

## Usage Rules

1. Every hazard row must have at least one mitigation and one verification link.
2. Every mitigation must reference a requirement ID and evidence artifact.
3. Do not claim legal/certification clearance from these templates alone.
