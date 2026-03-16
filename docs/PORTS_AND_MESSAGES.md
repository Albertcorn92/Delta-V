# Ports And Messages

DELTA-V moves commands, telemetry, and events through typed ports backed by
fixed-size queues.

## Port Types

The core primitives are:

- `InputPort<T>`
- `OutputPort<T>`
- `RingBuffer<T, Capacity>`

The payload type must satisfy the `FlightData` concept:

- trivially copyable
- standard layout
- bounded to a small fixed size

That keeps the core transport explicit and predictable.

## Message Model

The public baseline uses explicit packet types:

| Type | Purpose | Size |
|---|---|---|
| `TelemetryPacket` | numeric telemetry sample | `12 B` |
| `CommandPacket` | routed command | `12 B` |
| `EventPacket` | operator-facing event | `36 B` |

`Serializer` converts between packet structs and fixed byte arrays using
`std::bit_cast`.

## Important Terminology

DELTA-V is not a true zero-copy transport today.

What it is:

- fixed-size
- bounded-copy
- zero-allocation in the core message path
- explicit about overflow and disconnection failures

Each queue push stores a value copy into a ring buffer, and each pop copies it
back out. That is a deliberate tradeoff for small packets and simple ownership.

## Send Semantics

`OutputPort::send(...)` returns `false` when:

- the destination is not connected
- the destination queue is full

In the current baseline, producer components and hubs record these failures so
they are visible to FDIR instead of disappearing silently.

## Hubs

Three runtime hubs shape most traffic:

| Hub | Role |
|---|---|
| `CommandHub` | route commands and emit ack/nack events |
| `TelemHub` | fan telemetry to the bridge, recorder, and other listeners |
| `EventHub` | fan events to the bridge, recorder, and other listeners |

These hubs centralize routing while preserving typed inputs and outputs.

## Why This Model Exists

This approach keeps the public baseline:

- deterministic enough for small embedded systems
- easy to inspect in code review
- free of heap-dependent transport behavior in the core path
- compatible with generated topology verification

## Read Next

- `docs/COMPONENT_MODEL.md`
- `docs/SCHEDULER_AND_EXECUTION.md`
- `docs/ICD.md`
