# Reference Mission Walkthrough

The reference mission is a civilian 3U CubeSat profile used to show how the
main DELTA-V runtime, process, and safety records fit together.

## Purpose

Use this page after reading:

- `README.md`
- `docs/QUICKSTART_10_MIN.md`
- `docs/DEVELOPER_GUIDE.md`

Use this page to move from topology to a running host build and then into the
reference mission records.

## 1. Start With `topology.yaml`

`topology.yaml` is the source of truth for the generated runtime description.
It defines:

- components
- numeric IDs
- commands
- telemetry routes
- event routes
- mission-state command classes

When the topology changes, regenerate the derived files before building.

## 2. Regenerate Derived Files

```bash
python3 tools/autocoder.py
```

This updates:

- `src/Types.hpp`
- `src/TopologyManager.hpp`
- `dictionary.json`

The normal build also checks that these files are in sync with
`topology.yaml`.

## 3. Build The Host Runtime

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target flight_software
```

The reference host runtime includes:

- `TelemetryBridge`
- `CommandHub`
- `TelemHub`
- `EventHub`
- `WatchdogComponent`
- `MissionFsm`
- `RateGroupExecutive`

## 4. Start The Runtime

```bash
./build/flight_software
```

Normal startup should show the topology loading, scheduler initialization, and
the bridge coming online without fatal runtime errors.

## 5. Command And Telemetry Flow

The reference command path is:

`Ground -> TelemetryBridge -> CommandHub -> target component`

The command path enforces:

- frame validation
- mission-state gating
- anti-replay sequence checks
- command-rate limiting
- explicit NACK/error handling for invalid or unroutable commands

Telemetry and events move through `TelemHub` and `EventHub` to both the radio
path and recorder path based on the routes declared in `topology.yaml`.

`TopologyManager::verify()` also checks the expected command, telemetry, and
event wiring before the runtime is treated as healthy.

## 6. Exercise The Runtime

```bash
cmake --build build --target sitl_fault_campaign
```

This target starts the host runtime, injects valid and invalid traffic, and
checks that:

- malformed input is rejected
- replayed input is rejected
- valid commands still dispatch
- the process stays alive

## 7. Run The Software Closeout Path

```bash
cmake --build build --target software_final
```

`software_final` synchronizes the generated software evidence in `docs/` after
running the enforced repository checks.

## 8. Read The Reference Mission Records

The reference mission baseline is described in `docs/process/`. The main files
are:

- `REFERENCE_MISSION_PROFILE.md`
- `REFERENCE_MISSION_REQUIREMENTS_ALLOCATION.md`
- `REFERENCE_MISSION_INTERFACE_CONTROL.md`
- `REFERENCE_PAYLOAD_PROFILE.md`
- `RISK_REGISTER_BASELINE.md`
- `ASSUMPTIONS_LOG_BASELINE.md`
- `CONFIGURATION_AUDIT_BASELINE.md`
- `OPERATIONS_REHEARSAL_20260314.md`

These records define the mission context used by the public DELTA-V baseline.

## 9. Review The Safety Case

The reference mission safety case is in `docs/safety_case/`:

- `hazards.md`
- `mitigations.md`
- `fmea.md`
- `fta.md`
- `verification_links.md`

These files connect hazards, mitigations, and verification evidence for the
reference mission profile.

## 10. Current Boundary

The public repository does not close:

- sensor-attached HIL
- hardware timing margin on the target flight computer
- environmental qualification
- independent mission approval authority
- mission-owned protected uplink/security controls

Those items remain outside the public baseline.

## 11. Review Bundle

```bash
cmake --build build --target review_bundle
```

This creates:

- `build/review_bundle/`
- `build/review_bundle.zip`

Use that package when handing the current software baseline to another engineer
for review.
