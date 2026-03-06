#pragma once
// =============================================================================
// Requirements.hpp — DELTA-V Requirements Traceability Matrix (RTM)
// =============================================================================
// Every testable requirement is assigned a DV-XXX-NN identifier.
// Unit tests reference these IDs in their comments so a reviewer can trace
// from any test back to its governing requirement and vice versa.
//
// Categories:
//   DV-ARCH  : Architecture / Execution Model
//   DV-MEM   : Memory Safety
//   DV-FDIR  : Fault Detection, Isolation & Recovery
//   DV-COMM  : Communication / Protocol
//   DV-TIME  : Timing / Determinism
//   DV-SEC   : Security / Uplink Validation
//   DV-DATA  : Data Integrity / Parameter Database
//   DV-LOG   : Logging / Flight Data Recorder
//   DV-TMR   : Triple Modular Redundancy
//
// Compliance Level:
//   [A] = DO-178C DAL-A (catastrophic failure)
//   [B] = DO-178C DAL-B (hazardous failure)
//   [C] = DO-178C DAL-C (major failure)
// =============================================================================

namespace deltav {
namespace req {

// ---------------------------------------------------------------------------
// Architecture
// ---------------------------------------------------------------------------
// DV-ARCH-01 [B]: The system shall execute passive components in a single
//   deterministic cyclic loop at a configurable rate (default: 1 Hz).
// DV-ARCH-02 [B]: Active components shall execute on independent POSIX/RTOS
//   threads at their declared Hz rate using absolute-deadline sleep_until.
// DV-ARCH-03 [C]: No global mutable state outside of Component member variables
//   or explicitly named singleton services (TimeService, ParamDb).

// ---------------------------------------------------------------------------
// Memory Safety
// ---------------------------------------------------------------------------
// DV-MEM-01 [A]: No call to malloc, new, or any heap allocator shall succeed
//   after HeapGuard::arm() has been called.
// DV-MEM-02 [A]: All static arrays (ports, hub registries, scheduler)
//   shall be bounded by compile-time constants with static_assert guards.
// DV-MEM-03 [B]: RingBuffer overflow shall increment a counter (not silently
//   drop data) and be detectable by FDIR.

// ---------------------------------------------------------------------------
// FDIR
// ---------------------------------------------------------------------------
// DV-FDIR-01 [A]: The system shall enter SAFE_MODE if battery voltage drops
//   below 5%.
// DV-FDIR-02 [A]: The system shall enter EMERGENCY if battery voltage drops
//   below 2%.
// DV-FDIR-03 [B]: The system shall enter DEGRADED if any monitored component
//   reports HealthStatus::WARNING.
// DV-FDIR-04 [A]: The system shall attempt up to MAX_RESTARTS_PER_COMPONENT
//   automatic restarts of a CRITICAL_FAILURE ActiveComponent.
// DV-FDIR-05 [B]: After successful restart, component error count shall be
//   cleared and MissionState may recover to NOMINAL.
// DV-FDIR-06 [B]: A heartbeat event shall be emitted every
//   HEARTBEAT_INTERVAL_CYCLES scheduler ticks.
// DV-FDIR-07 [C]: Scheduler frame drops shall be surfaced to the FDIR
//   event stream via pollSchedulerHealth().
// DV-FDIR-08 [B]: CommandHub NACK for an unroutable command shall
//   increment the error counter (recordError()).
// DV-FDIR-09 [B]: Battery mission-state thresholds shall be configurable via
//   TmrStore<float> sources; invalid threshold ordering shall fall back to
//   conservative defaults and raise an FDIR error.

// ---------------------------------------------------------------------------
// Communication
// ---------------------------------------------------------------------------
// DV-COMM-01 [B]: All downlink frames shall use CCSDS framing with sync word
//   0x1ACFFC1D, APID, sequence counter, payload length, and CRC-16/CCITT.
// DV-COMM-02 [B]: COBS encoding shall be applied to all downlink serial
//   streams to guarantee frame synchronisation after bit-error events.
// DV-COMM-03 [A]: All uplink command packets shall use canonical CCSDS command
//   framing: [CcsdsHeader + CommandPacket] with no trailing bytes.
// DV-COMM-04 [A]: Replayed commands (sequence number <= last accepted)
//   shall be rejected and counted.
// DV-COMM-05 [B]: A maximum of MAX_CMDS_PER_TICK uplink packets shall be
//   processed per TelemetryBridge step() call (anti-jamming flood limit).
// DV-COMM-06 [A]: The last accepted uplink sequence shall be persisted and
//   reloaded on restart when a replay-state backing store is configured.

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
// DV-TIME-01 [B]: Os::Thread shall use sleep_until (absolute deadline),
//   not sleep_for (relative), to prevent drift accumulation.
// DV-TIME-02 [C]: TimeService shall emit a one-time warning log when MET
//   approaches uint32_t wrap (~46 days).

// ---------------------------------------------------------------------------
// Security
// ---------------------------------------------------------------------------
// DV-SEC-01 [A]: Uplink packets with invalid sync/APID/payload-length fields
//   shall be rejected.
// DV-SEC-02 [B]: MissionFsm::isAllowed() shall gate every uplinked command
//   against the current MissionState before dispatch.
// DV-SEC-03 [A]: Uplink packets with non-canonical lengths (truncated or
//   oversized) shall be rejected.

// ---------------------------------------------------------------------------
// Data Integrity
// ---------------------------------------------------------------------------
// DV-DATA-01 [A]: ParamDb shall maintain a CRC-32 checksum over all stored
//   parameters, verifiable at any time via verifyIntegrity().
// DV-DATA-02 [B]: Critical runtime parameters shall be stored in
//   TmrStore<T> with majority-vote read and self-healing write.
// DV-DATA-03 [B]: TmrRegistry::scrubAll() shall be called periodically by
//   WatchdogComponent to repair single-event upsets.

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------
// DV-LOG-01 [C]: LoggerComponent shall write telemetry to flight_log.csv and
//   events to flight_events.log as separate files.
// DV-LOG-02 [C]: Event log shall be flushed after every event packet.
//   Telemetry log shall be flushed every LOG_FLUSH_INTERVAL packets.

// ---------------------------------------------------------------------------
// TMR
// ---------------------------------------------------------------------------
// DV-TMR-01 [A]: TmrStore shall detect a single-copy divergence and repair
//   it by majority vote within one scrub cycle.
// DV-TMR-02 [B]: TmrStore shall log a warning on double-upset (all three
//   copies differ) and return copies[0] as a best-effort value.

} // namespace req
} // namespace deltav
