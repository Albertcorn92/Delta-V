# Reference Mission Safety Case Baseline

Date: 2026-03-14

This folder contains the DELTA-V reference-mission safety-case baseline built
around the 3U CubeSat profile in
`docs/process/REFERENCE_MISSION_PROFILE.md`. It is still a public baseline, not
an operational flight safety approval package.

## Scope

- Software evidence from DELTA-V gates is necessary but not sufficient.
- The record set now carries reference-mission context instead of being a pure
  blank starter template.
- Mission teams must still extend it with hardware, payload, environment, and
  independent review evidence.

## Files

- `hazards.md`: reference-mission hazard register.
- `mitigations.md`: mitigation map from hazards to requirements and controls.
- `verification_links.md`: evidence links from hazards to tests/gates/rehearsals.
- `fmea.md`: reference-mission software FMEA.
- `fta.md`: reference-mission software FTA.
- `change_impact.md`: impact assessment template for safety-relevant changes.

## Usage Rules

1. Every hazard row must have at least one mitigation and one verification link.
2. Every mitigation must reference a requirement ID and evidence artifact.
3. Do not claim legal/certification clearance from these templates alone.
4. Extend the baseline records with mission-specific hazards, interfaces, and
   reviews when moving beyond the reference mission.
