"""
DELTA-V Ground Data System Dashboard
=====================================
Streamlit-based mission control UI.

Changes from previous version:
  - CCSDS sync word updated to 0x1ACFFC1D (standard) from 0xDEADBEEF
  - Downlink CRC-16/CCITT validation (rejects corrupted frames)
  - EventPacket format updated: severity(I), source_id(I), message(28s) = 36B
  - Command packing uses dedicated CMD_FORMAT with correct field order
  - Uplink sequence counter increments per command (replay protection)
  - Source ID shown in event log for better FDIR traceability
"""
import streamlit as st
import socket
import struct
import pandas as pd
import threading
import time
from datetime import datetime
import os
import json

# ---------------------------------------------------------------------------
# CONFIG
# ---------------------------------------------------------------------------
PORT_DOWNLINK = 9001
PORT_UPLINK   = 9002

# CCSDS sync word — must match Types.hpp: CCSDS_SYNC_WORD = 0x1ACFFC1D
CCSDS_SYNC_WORD = 0x1ACFFC1D

# Packet formats (all little-endian to match C++ on x86/ARM Linux)
HEADER_FORMAT = "<IHHH"   # 10B: Sync(I4), APID(H2), Seq(H2), Len(H2)
TELEM_FORMAT  = "<IIf"    # 12B: MET(I4), CompID(I4), Val(f4)
# EventPacket: severity(I4) + source_id(I4) + message(28s) = 36B
EVENT_FORMAT  = "<II28s"  # updated from old "<I32s" to match new Types.hpp
# CommandPacket: target_id(I4) + opcode(I4) + argument(f4) = 12B
CMD_FORMAT    = "<IIf"
# CRC appended after payload: uint16_t big-endian
CRC_FORMAT    = ">H"      # big-endian uint16 (how it's written in TelemetryBridge)

DATA_FILE  = "live_telem.csv"
EVENT_FILE = "events.log"
DICT_FILE  = "dictionary.json"

# ---------------------------------------------------------------------------
# CRC-16/CCITT (polynomial 0x1021, init 0xFFFF) — mirrors Serializer::crc16
# ---------------------------------------------------------------------------
def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = (crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1
        crc &= 0xFFFF
    return crc

# ---------------------------------------------------------------------------
# Data dictionary
# ---------------------------------------------------------------------------
def load_dictionary() -> dict:
    if os.path.exists(DICT_FILE):
        with open(DICT_FILE) as f:
            return json.load(f)
    return {
        "components": {
            "200": {"name": "BatterySystem"},
            "100": {"name": "StarTracker"}
        },
        "commands": {}
    }

GDS_DICT = load_dictionary()

def get_component_name(comp_id: int) -> str:
    entry = GDS_DICT["components"].get(str(comp_id))
    if entry:
        return entry["name"] if isinstance(entry, dict) else entry
    return f"COMP_{comp_id}"

def get_command(name: str) -> dict | None:
    return GDS_DICT.get("commands", {}).get(name)

# ---------------------------------------------------------------------------
# Uplink sequence counter
# ---------------------------------------------------------------------------
_uplink_seq = 0

def send_command(target_id: int, opcode: int, argument: float = 0.0) -> None:
    global _uplink_seq
    payload = struct.pack(CMD_FORMAT, target_id, opcode, argument)
    header  = struct.pack(HEADER_FORMAT,
                          CCSDS_SYNC_WORD, 30, _uplink_seq & 0xFFFF, len(payload))
    _uplink_seq += 1
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(header + payload, ("127.0.0.1", PORT_UPLINK))
    sock.close()

# ---------------------------------------------------------------------------
# UDP listener thread
# ---------------------------------------------------------------------------
def udp_listener() -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.bind(("0.0.0.0", PORT_DOWNLINK))
        sock.settimeout(1.0)
        print(f"[GDS] Listener on port {PORT_DOWNLINK} | sync=0x{CCSDS_SYNC_WORD:08X}")
    except Exception as e:
        print(f"[GDS] Bind error: {e}")
        return

    rejected = 0
    while True:
        try:
            data, _ = sock.recvfrom(1024)
            header_size = struct.calcsize(HEADER_FORMAT)
            crc_size    = struct.calcsize(CRC_FORMAT)

            if len(data) < header_size + crc_size:
                continue

            sync, apid, seq, plen = struct.unpack(HEADER_FORMAT, data[:header_size])

            # 1. Sync word check
            if sync != CCSDS_SYNC_WORD:
                rejected += 1
                continue

            payload = data[header_size : header_size + plen]
            crc_bytes = data[header_size + plen : header_size + plen + crc_size]

            # 2. CRC validation
            if len(crc_bytes) == crc_size:
                expected_crc = crc16_ccitt(payload)
                received_crc, = struct.unpack(CRC_FORMAT, crc_bytes)
                if expected_crc != received_crc:
                    rejected += 1
                    print(f"[GDS] CRC mismatch on APID {apid} seq {seq} "
                          f"(expected 0x{expected_crc:04X}, got 0x{received_crc:04X})")
                    continue

            timestamp = datetime.now().strftime('%H:%M:%S')

            # 3. Dispatch by APID
            if apid == 10 and len(payload) >= 12:  # Telemetry
                met, cid, val = struct.unpack(TELEM_FORMAT, payload[:12])
                name = get_component_name(cid)
                with open(DATA_FILE, "a") as f:
                    f.write(f"{timestamp},{name},{val:.4f}\n")

            elif apid == 20 and len(payload) >= 36:  # Event
                sev, src_id, msg_b = struct.unpack(EVENT_FORMAT, payload[:36])
                msg = msg_b.decode("ascii", errors="ignore").strip("\x00")
                sev_str = {1: "INFO", 2: "WARN", 3: "CRIT"}.get(sev, f"SEV{sev}")
                src_name = get_component_name(src_id)
                with open(EVENT_FILE, "a") as f:
                    f.write(f"[{timestamp}][{sev_str}][{src_name}] {msg}\n")

        except socket.timeout:
            continue
        except Exception as e:
            print(f"[GDS] Listener error: {e}")

# ---------------------------------------------------------------------------
# Boot setup
# ---------------------------------------------------------------------------
if not os.path.exists(DATA_FILE) or os.stat(DATA_FILE).st_size == 0:
    with open(DATA_FILE, "w") as f:
        f.write("Time,Component,Value\n")

if not any(t.name == "GDS_LISTENER" for t in threading.enumerate()):
    threading.Thread(target=udp_listener, daemon=True, name="GDS_LISTENER").start()

# ---------------------------------------------------------------------------
# Streamlit UI
# ---------------------------------------------------------------------------
st.set_page_config(page_title="DELTA-V GDS", layout="wide")
st.title("🛰️  DELTA-V MISSION CONTROL")

def load_data() -> pd.DataFrame | None:
    try:
        df = pd.read_csv(DATA_FILE)
        return df if not df.empty else None
    except Exception:
        return None

df = load_data()

# --- Metrics bar ---
c1, c2, c3, c4, c5 = st.columns(5)
status    = "🟢 AOS" if df is not None else "🔴 LOS"
batt_val  = "N/A"
last_time = "N/A"
num_pkts  = 0

if df is not None:
    last_time = df.iloc[-1]["Time"]
    num_pkts  = len(df)
    batt_only = df[df["Component"] == "BatterySystem"]
    if not batt_only.empty:
        batt_val = f"{batt_only.iloc[-1]['Value']:.1f}%"

with c1: st.metric("📡 LINK",       status)
with c2: st.metric("⏱️ LAST PKT",   last_time)
with c3: st.metric("🔋 BATTERY",    batt_val)
with c4: st.metric("📦 PACKETS",    num_pkts)
with c5: st.metric("🔌 DOWNLINK",   PORT_DOWNLINK)

# --- Telemetry plot ---
if df is not None:
    plot_df = df.pivot_table(index="Time", columns="Component", values="Value").tail(60)
    st.line_chart(plot_df)
else:
    st.warning("⏳ Waiting for telemetry — ensure flight_software is running.")

# --- Event log ---
col_left, col_right = st.columns([2, 1])
with col_left:
    st.subheader("📋 Event Log")
    if os.path.exists(EVENT_FILE):
        with open(EVENT_FILE) as f:
            lines = f.readlines()
        recent = "".join(reversed(lines[-30:]))
        st.text_area("Events (newest first)", value=recent, height=250,
                     label_visibility="collapsed")
    else:
        st.caption("No events yet.")

with col_right:
    st.subheader("📊 Stats")
    if df is not None:
        st.dataframe(
            df.groupby("Component")["Value"].agg(["last", "min", "max"])
              .rename(columns={"last": "Latest", "min": "Min", "max": "Max"})
              .round(3),
            use_container_width=True
        )

# --- Command sidebar ---
st.sidebar.header("🎮 UPLINK CONTROL")
st.sidebar.caption(f"Flight SW @ UDP :{PORT_UPLINK}  |  seq={_uplink_seq}")
st.sidebar.caption(f"Sync: 0x{CCSDS_SYNC_WORD:08X}")
st.sidebar.divider()

if st.sidebar.button("⚡ RESET BATTERY"):
    cmd = get_command("RESET_BATTERY")
    if cmd:
        send_command(cmd["target_id"], cmd["opcode"])
        st.sidebar.success(f"✅ RESET_BATTERY → ID {cmd['target_id']}")
    else:
        send_command(200, 1)
        st.sidebar.success("✅ RESET_BATTERY (fallback)")

st.sidebar.divider()
drain_rate = st.sidebar.slider("Drain Rate (%/tick)",
                                min_value=0.0, max_value=2.0,
                                value=0.1, step=0.05)
if st.sidebar.button("📉 SET DRAIN RATE"):
    cmd = get_command("SET_DRAIN_RATE")
    if cmd:
        send_command(cmd["target_id"], cmd["opcode"], drain_rate)
        st.sidebar.success(f"✅ SET_DRAIN_RATE arg={drain_rate:.2f}")
    else:
        send_command(200, 2, drain_rate)
        st.sidebar.success(f"✅ SET_DRAIN_RATE (fallback) arg={drain_rate:.2f}")

st.sidebar.divider()
st.sidebar.caption("Frame format: [CCSDS Header 10B][Payload][CRC-16 2B]")

# Auto-refresh
time.sleep(1)
st.rerun()