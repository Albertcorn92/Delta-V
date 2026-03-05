# DELTA-V Interface Control Document (ICD)

## 1. Overview
This document defines the data interfaces, network protocols, and binary packet structures used for communication between the DELTA-V Flight Software and the Ground Data System (GDS).

## 2. Network Interface
All space-to-ground communication operates over asynchronous, non-blocking UDP sockets.
* **Downlink (Telemetry & Events):** Port `9001`
* **Uplink (Commands):** Port `9002`

## 3. CCSDS Space Packet Framing
Every packet transmitted to or from the vehicle is wrapped in a 10-byte aerospace header to ensure data integrity, packet synchronization, and drop-detection.

### 3.1 Primary Header Structure (10 Bytes)
| Field | Size | Type | Description |
| :--- | :--- | :--- | :--- |
| **Sync Word** | 4 Bytes | `uint32_t` | Static identifier (`0xDEADBEEF`) used to lock onto a valid packet. |
| **APID** | 2 Bytes | `uint16_t` | Application Process ID. Identifies payload type (`10`=Telem, `20`=Event, `30`=Cmd). |
| **Seq. Count** | 2 Bytes | `uint16_t` | Incrementing counter to detect dropped packets. |
| **Length** | 2 Bytes | `uint16_t` | Size of the attached payload in bytes. |

---

## 4. Payload Definitions
All payloads adhere to strict DO-178C memory layouts with zero compiler padding. 

### 4.1 Telemetry Payload (APID 10)
**Size:** 12 Bytes | **Total Framed Size:** 22 Bytes
| Field | Size | Type | Description |
| :--- | :--- | :--- | :--- |
| **Timestamp** | 4 Bytes | `uint32_t` | Mission Elapsed Time (MET) in milliseconds. |
| **Component ID** | 4 Bytes | `uint32_t` | ID of the subsystem generating the data. |
| **Data Payload** | 4 Bytes | `float` | Sensor reading, state of charge, or algorithmic output. |

### 4.2 Event Payload (APID 20)
**Size:** 36 Bytes | **Total Framed Size:** 46 Bytes
| Field | Size | Type | Description |
| :--- | :--- | :--- | :--- |
| **Severity** | 4 Bytes | `uint32_t` | Alert level (`1`=Info, `2`=Warning, `3`=Critical). |
| **Message** | 32 Bytes | `char[32]` | ASCII-encoded system message (Null-terminated). |

### 4.3 Command Payload (APID 30)
**Size:** 12 Bytes | **Total Framed Size:** 22 Bytes
| Field | Size | Type | Description |
| :--- | :--- | :--- | :--- |
| **Target ID** | 4 Bytes | `uint32_t` | Destination subsystem ID. |
| **Opcode** | 4 Bytes | `uint32_t` | Operation code to execute. |
| **Argument** | 4 Bytes | `float` | Command parameter (e.g., drain rate, coordinate). |