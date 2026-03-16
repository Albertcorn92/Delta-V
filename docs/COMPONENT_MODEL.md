# Component Model

DELTA-V organizes flight software around explicit components with typed inputs,
typed outputs, and visible health status.

## Base Classes

Two base classes define the runtime model:

| Class | Use |
|---|---|
| `Component` | passive logic executed by the scheduler |
| `ActiveComponent` | thread-owning component for I/O-heavy or self-paced work |

Both derive health from internal error counters. Components surface recoverable
faults through `recordError()`, and the watchdog reads those counters to decide
whether a subsystem is nominal, warning, or critical.

## Common Shape

A typical passive component has:

- `init()`: one-time startup and resource preparation
- `step()`: bounded recurring work
- input ports for commands or upstream data
- output ports for telemetry, events, or downstream commands

Producer-side output paths should use checked sends so queue backpressure and
missing connections become visible faults instead of silent loss.

## Health Model

`Component` tracks an internal error count and maps it to:

- `NOMINAL`
- `WARNING`
- `CRITICAL_FAILURE`

That mapping is intentionally simple so the watchdog can supervise many
different components with the same mechanism.

## Passive Components

Passive components are owned by the scheduler. Examples:

- `TelemHub`
- `EventHub`
- `CommandHub`
- `LoggerComponent`
- `WatchdogComponent`
- application components such as `PowerComponent` and `PayloadMonitorComponent`

Passive components should keep `step()` bounded and deterministic.

## Active Components

Active components inherit from `ActiveComponent` and own an `Os::Thread`.

Use this model for:

- link-facing runtime blocks
- timing-sensitive I/O loops
- work that does not fit a simple scheduler tick

The public baseline uses this pattern for `TelemetryBridge`.

## Generated Topology Integration

Components are not wired by hand at runtime. `topology.yaml` drives the
generated `TopologyManager`, which:

- instantiates components
- connects command, telemetry, and event routes
- registers scheduler membership
- verifies expected wiring

That generator-backed wiring model is one of the main differences between
DELTA-V and a loose collection of hand-wired classes.

## Read Next

- `docs/PORTS_AND_MESSAGES.md`
- `docs/SCHEDULER_AND_EXECUTION.md`
- `docs/ARCHITECTURE.md`
