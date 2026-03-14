# DELTA-V Reference Mission Profile

Date: 2026-03-14
Scope: planning profile for framework assurance work

This profile gives the repository one concrete mission context for planning,
verification, and documentation. It is a reference baseline only.

## Mission Summary

- Spacecraft: civilian 3U CubeSat
- Orbit: low Earth orbit
- Mission type: technology demonstration with low-rate payload operations
- Operations model: scheduled passes, routine housekeeping, occasional
  payload-command windows, safe-mode recovery from the ground

## Flight Software Responsibilities

- startup sequencing and scheduler execution
- telemetry and event routing to the downlink
- command routing with mission-state gating
- watchdog/FDIR and recovery handling
- bounded low-rate payload enable/capture services
- parameter integrity and bounded operations services

## Ground Responsibilities

- command preparation and uplink through the GDS
- telemetry and event review
- timeline rehearsal in simulation before operational use

## Constraints

- civilian/scientific/educational use only
- no command-path encryption in the public baseline
- no weaponization, targeting, or military mission logic
- public-source publication only for the maintainer baseline

## Assumptions Used By The Assurance Baseline

- One main flight-software image with static topology
- No crewed-flight or human-safety claims
- Mission loss is the dominant consequence for major software faults
- Hardware qualification, RF-chain validation, and launch acceptance are handled
  by the adopting mission, not by this repository

## Verification Consequences

- command gating, safe-mode behavior, FDIR, and data-integrity paths are treated
  as the highest-priority software verification targets
- release evidence is generated from repeatable local and CI gates
- mission adopters must extend the baseline with hardware and authority evidence
