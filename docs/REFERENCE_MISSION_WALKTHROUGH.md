# Reference Mission Walkthrough

This walkthrough shows the shortest complete path through DELTA-V using the
reference civilian 3U CubeSat mission baseline.

## 1. Source Of Truth

Start with `topology.yaml`.

That file defines:

- components,
- IDs,
- commands,
- telemetry routes,
- event routes,
- and mission-state command classes.

The repository does not treat topology as a comment or design note. It is a
build input.

## 2. Generation

Run:

```bash
python3 tools/autocoder.py
```

That regenerates:

- `src/Types.hpp`
- `src/TopologyManager.hpp`
- `dictionary.json`

The normal build path also checks that these generated artifacts are current.

## 3. Runtime Build

Run:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target flight_software
```

The host runtime includes:

- `TelemetryBridge`
- `CommandHub`
- `TelemHub`
- `EventHub`
- `WatchdogComponent`
- `MissionFsm`
- `RateGroupExecutive`

## 4. Runtime Behavior

Start the host runtime:

```bash
./build/flight_software
```

For the reference mission, the expected early markers are:

- topology wired,
- scheduler tiers ready,
- bridge online,
- no fatal runtime signatures.

## 5. Command Path

The command path is:

`Ground -> TelemetryBridge -> CommandHub -> target component`

Key controls on that path:

- canonical frame validation,
- mission-state gating,
- anti-replay sequence enforcement,
- per-tick command-rate limiting,
- and explicit NACK/error behavior on routing failure.

## 6. Live Fault Exercise

Run:

```bash
cmake --build build --target sitl_fault_campaign
```

That launches the live runtime, injects:

- valid traffic,
- replayed traffic,
- malformed sync/APID/length frames,
- and random invalid blobs,

then verifies:

- the process stays alive,
- the invalid traffic is rejected,
- and valid commands still dispatch.

## 7. Software Closeout

Run:

```bash
cmake --build build --target software_final
```

That closes the enforced software path:

- generation checks,
- legal checks,
- unit tests,
- system tests,
- stress,
- static analysis,
- traceability,
- benchmarks,
- SITL soak,
- and the live fault campaign.

## 8. Reference Mission Process Layer

The repo now carries one concrete mission-shaped process baseline in
`docs/process/`.

The most important records are:

- `REFERENCE_MISSION_PROFILE.md`
- `REFERENCE_MISSION_REQUIREMENTS_ALLOCATION.md`
- `REFERENCE_MISSION_INTERFACE_CONTROL.md`
- `RISK_REGISTER_BASELINE.md`
- `ASSUMPTIONS_LOG_BASELINE.md`
- `CONFIGURATION_AUDIT_BASELINE.md`
- `OPERATIONS_REHEARSAL_20260314.md`

These documents answer the question “what does this framework mean in one real
mission context?”

## 9. What Still Requires A Real Mission Program

This repo still does not close:

- sensor-attached HIL,
- hardware timing margin,
- thermal/vibe/EMI/radiation evidence,
- independent approval authority,
- and mission-owned security controls.

Those are outside the public reference baseline by design.

## 10. Reviewer Output

Run:

```bash
cmake --build build --target review_bundle
```

That produces a staged package for technical review in:

- `build/review_bundle/`
- `build/review_bundle.zip`
