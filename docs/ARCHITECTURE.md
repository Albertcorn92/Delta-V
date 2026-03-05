ARCHITECTURE.md - DELTA-V Flight Software Framework
1. Design Philosophy
The DELTA-V Framework is designed as a Component-Based Software Architecture (CBSA) focused on high-reliability, real-time deterministic execution. It adheres to key principles of DO-178C (Software Considerations in Airborne Systems), ensuring that memory is statically allocated at boot and execution timing is strictly controlled.

2. The Core Execution Model
The framework utilizes a hybrid execution model to balance high-speed synchronous tasks with low-latency asynchronous I/O.

2.1 The Scheduler (The Heartbeat)
The Scheduler operates as a cyclic executive. It manages the mission clock via the TimeService and executes "Passive Components" in a single-threaded, deterministic loop. This prevents race conditions in core flight logic.

2.2 Active vs. Passive Components
Passive Components: Execute within the main Scheduler thread. Ideal for control loops and state logic where determinism is critical.

Active Components: Inherit from ActiveComponent and operate on their own independent POSIX threads. These are used for "blocking" or high-frequency I/O tasks, such as the TelemetryBridge (Radio) and the Watchdog.

3. Communication Topology: Ports & Hubs
DELTA-V avoids global variables. Instead, it uses a Publish-Subscribe and Point-to-Point wiring model via InputPort and OutputPort.

3.1 Thread-Safe Data Exchange
Data passed between an Active thread (like the Radio) and a Passive thread (like the Scheduler) is buffered through the RingBuffer.

Lock-Free Design: The RingBuffer uses atomic head/tail pointers to allow one-way communication between threads without using heavy Mutexes, reducing "jitter" in the flight timing.

3.2 System Hubs
TelemHub: Aggregates telemetry from all components and routes it to the TelemetryBridge and Logger.

CommandHub: Receives uplinked packets, validates opcodes, and routes them to specific hardware targets.

EventHub: Manages system-wide alerts and asynchronous notifications.

4. FDIR (Fault Detection, Isolation, and Recovery)
The WatchdogComponent acts as the system's supervisor.

Thread Monitoring: Active components must "check-in" with the Watchdog. If a thread hangs (e.g., a radio driver failure), the Watchdog detects the timeout.

Telemetry Heartbeat: Monitors critical internal metrics (like Battery Voltage) via dedicated internal ports.

Recovery: Upon detecting a failure, the Watchdog issues a high-priority EventPacket to notify the ground station.

5. Memory & Integrity (ParamDb)
The ParamDb (Parameter Database) is the single source of truth for mission configuration.

Persistence: Parameters are persisted to mission_params.db for recovery after a power cycle (SITL).

Integrity Protection: The database utilizes a CRC-32 Checksum algorithm. The system continuously hashes the parameter memory block; if a cosmic ray causes a bit-flip in RAM, the CRC check will fail, and an FDIR event is triggered.

6. Networking Stack (CCSDS)
The framework implements a subset of the CCSDS (Consultative Committee for Space Data Systems) standard.

Framing: Every packet is encapsulated in a 10-byte header.

Sync Word: 0xDEADBEEF is used to identify the start of a frame in a noisy bitstream.

Sequence Tracking: Each APID (Application Process ID) maintains an independent counter to allow the Ground Data System to track packet loss.