# Safety Case Starter (Mission Team Template)

Date: 2026-03-07

This folder is a starter structure for mission-program safety cases built on
top of DELTA-V software artifacts. It now includes a framework baseline record
set that mission teams should treat as a starting point.

## Scope

- Software evidence from DELTA-V gates is necessary but not sufficient.
- Mission teams must add mission-specific hazards, mitigations, and independent
  verification records.

## Files

- `hazards.md`: framework baseline hazard log.
- `mitigations.md`: framework baseline mitigation map to requirements.
- `verification_links.md`: baseline evidence links from hazard to verification.
- `fmea.md`: framework baseline software FMEA.
- `fta.md`: framework baseline software FTA.
- `change_impact.md`: impact assessment template for safety-relevant changes.

## Usage Rules

1. Every hazard row must have at least one mitigation and one verification link.
2. Every mitigation must reference a requirement ID and evidence artifact.
3. Do not claim legal/certification clearance from these templates alone.
4. Extend the baseline records with mission-specific hazards and reviews.
