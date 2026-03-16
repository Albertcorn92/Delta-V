# Scheduler And Execution

DELTA-V separates execution ownership from message routing. Components exchange
data through ports, while the runtime decides when each component runs.

## Primary Runtime Model

The normal host runtime uses `RateGroupExecutive`.

Rate groups in the public baseline:

- `FAST`: `10 Hz`
- `NORM`: `1 Hz`
- `SLOW`: `0.1 Hz`
- `ACTIVE`: dedicated thread ownership through `ActiveComponent`

This gives the framework a small number of predictable execution tiers without
requiring each component to build its own timing loop.

## Passive Scheduling

Passive components are stepped by the executive according to their assigned rate
group from `topology.yaml`.

This is the default model for:

- hubs
- watchdog and fault-management blocks
- recorder
- most application components

## Active Execution

Some components are better modeled as active loops. `ActiveComponent` wraps an
`Os::Thread` and runs on its own cadence.

Use cases:

- radio and link I/O
- blocking or timing-sensitive peripheral work
- integration points that do not fit the passive tick model

## Why The Split Matters

The framework gets two useful properties from this split:

- most logic remains easy to reason about because it is passive and scheduled
- link-facing code can still own its own loop without infecting the entire
  architecture

## Fault Visibility

Execution and fault handling are tied together:

- components increment internal error counts on detected faults
- the watchdog supervises those counts
- threshold crossings drive warning, degradation, restart, or safe-mode logic

This means timing, queueing, and routing problems are meant to become visible at
the system level instead of staying local to one class.

## Generated Membership

`topology.yaml` also controls which components are registered into which rate
groups. `TopologyManager` applies that generated membership during startup.

## Read Next

- `docs/ARCHITECTURE.md`
- `docs/COMPONENT_MODEL.md`
- `docs/SAFETY_ASSURANCE.md`
