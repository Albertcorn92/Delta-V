# DELTA-V Interface Control Document (ICD)  v4.0

## 1. Overview

This document defines the data interfaces, network protocols, and binary packet structures for communication between the DELTA-V Flight Software and the Ground Data System (GDS).

---

## 2. Transport Interface

DELTA-V supports two transport profiles for the same CCSDS payloads:

### 2.1 UDP (default SITL)

| Direction | Port | Description |
|---|---|---|
| Downlink (flight → ground) | 9001 | Telemetry and events |
| Uplink (ground → flight) | 9002 | Commands |

### 2.2 Serial-KISS (radio-oriented integration)

Enable with:

```bash
export DELTAV_LINK_MODE=serial_kiss
export DELTAV_SERIAL_PORT=/dev/tty.usbserial-0001
export DELTAV_SERIAL_BAUD=115200
```

Frames are wrapped in KISS delimiters/escapes over UART:

- `FEND` (`0xC0`) frame boundary
- `FESC TFEND` escape for `FEND`
- `FESC TFESC` escape for `FESC`

---

## 3. Frame Stack

```
[ CCSDS Primary Header (10B) ]
[ Payload (variable)          ]
[ CRC-16/CCITT (2B, downlink) ]
         │
         ▼  raw UDP payload or KISS-wrapped UART payload
```

### Framing Notes

- The CRC protects the payload only (not the CCSDS header).
- Uplink command payload remains canonical: header + command packet.

---

## 4. CCSDS Primary Header (10 Bytes)

| Field | Bytes | Type | Description |
|---|---|---|---|
| **Sync Word** | 4 | `uint32_t` | `0x1ACFFC1D` (CCSDS standard marker) |
| **APID** | 2 | `uint16_t` | `10`=Telemetry, `20`=Event, `30`=Command |
| **Seq. Count** | 2 | `uint16_t` | Monotonically incrementing per APID |
| **Length** | 2 | `uint16_t` | Payload size in bytes |

> **Note:** Earlier versions of this document incorrectly listed the sync word as `0xDEADBEEF`. The authoritative value is `0x1ACFFC1D` as defined in `Types.hpp` and `CCSDS_SYNC_WORD`.

---

## 5. Payload Definitions

All payloads use `#pragma pack(1)` — no compiler padding.

### 5.1 Telemetry Payload (APID 10) — 12 Bytes

| Field | Bytes | Type | Description |
|---|---|---|---|
| Timestamp | 4 | `uint32_t` | Mission Elapsed Time in ms |
| Component ID | 4 | `uint32_t` | Subsystem ID from topology |
| Data Payload | 4 | `float` | Sensor reading or computed value |

Total framed size: 10 (header) + 12 (payload) + 2 (CRC) = **24 bytes**

### 5.2 Event Payload (APID 20) — 36 Bytes

| Field | Bytes | Type | Description |
|---|---|---|---|
| Severity | 4 | `uint32_t` | `1`=INFO, `2`=WARNING, `3`=CRITICAL |
| Source ID | 4 | `uint32_t` | Component ID that generated the event |
| Message | 28 | `char[28]` | Null-terminated ASCII string |

> **Note:** Earlier ICD versions listed the message field as 32 bytes. The authoritative size is **28 bytes** as enforced by the `static_assert` in `Types.hpp`.

Total framed size: 10 + 36 + 2 = **48 bytes**

### 5.3 Command Payload (APID 30) — 12 Bytes

| Field | Bytes | Type | Description |
|---|---|---|---|
| Target ID | 4 | `uint32_t` | Destination component ID |
| Opcode | 4 | `uint32_t` | Operation code |
| Argument | 4 | `float` | Command parameter |

Total framed size: 10 (header) + 12 (payload) = **22 bytes**

---

## 6. Uplink Validation

All command frames must match canonical command framing:

- `[CCSDS Header (10B)] + [CommandPacket (12B)]` exactly
- sync/APID/payload-length fields must be valid
- oversized or truncated frames are rejected
- host/SITL UDP command ingest is disabled by default unless `DELTAV_ENABLE_UNAUTH_UPLINK=1`
- host/SITL accepted source IP defaults to `127.0.0.1` and can be overridden with `DELTAV_UPLINK_ALLOW_IP`
- Replay-state persistence path: `DELTAV_REPLAY_SEQ_FILE`.
- Replay protection: sequence number must be strictly greater than the last accepted value (with wrap detection at `0xFF00`→`0x0100`)

---

## 7. Mission FSM Command Policy

The flight software enforces the following command dispatch policy (see `MissionFsm.hpp`):

| State | HOUSEKEEPING | OPERATIONAL | RESTRICTED |
|---|---|---|---|
| BOOT | ✅ | ❌ | ❌ |
| NOMINAL | ✅ | ✅ | ✅ |
| DEGRADED | ✅ | ✅ | ❌ |
| SAFE_MODE | ✅ | ❌ | ❌ |
| EMERGENCY | ❌ | ❌ | ❌ |

Commands rejected by the FSM return a NACK event with message `FSM_BLOCKED: OPx in STATE`.

---

## 8. Dictionary Status

`dictionary.json` is generated from `topology.yaml` and currently includes:

- IMU component entry (`ID 300`, `IMU_01`)
- Star-tracker amplitude command (`SET_STAR_AMPLITUDE`, target `ID 100`, opcode `1`)
- IMU recalibration command (`IMU_RECALIBRATE`, target `ID 300`, opcode `1`)
- Civilian ops app commands (`SEQ_*`, `FILE_*`, `DWELL_*`, `TIME_*`, `PLAYBACK_*`, `OTA_*`, `ATS_*`, `LIM_*`, `CFDP_*`, `MODE_*`)

To refresh dictionary and interface artifacts after topology updates:

```bash
python3 tools/autocoder.py
```
