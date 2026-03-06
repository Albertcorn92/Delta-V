"""
DELTA-V Ground Data System Dashboard
=====================================
Streamlit-based mission control UI.

Changes from previous version (Phase 4.1):
  - SipHash-2-4 Cryptographic Authentication for Uplink
  - Appends 8-byte MAC to all commands to pass Flight Software security checks
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
# CONFIG & CRYPTO
# ---------------------------------------------------------------------------
PORT_DOWNLINK = 9001
PORT_UPLINK   = 9002

CCSDS_SYNC_WORD = 0x1ACFFC1D

# 128-bit Pre-Shared Key (Must match TelemetryBridge.hpp)
UPLINK_KEY = b'\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F'

HEADER_FORMAT = "<IHHH"   # 10B: Sync(I4), APID(H2), Seq(H2), Len(H2)
TELEM_FORMAT  = "<IIf"    # 12B: MET(I4), CompID(I4), Val(f4)
EVENT_FORMAT  = "<II28s"  # 36B: severity(I4) + source_id(I4) + message(28s)
CMD_FORMAT    = "<IIf"    # 12B: target_id(I4) + opcode(I4) + argument(f4)
CRC_FORMAT    = ">H"      # 2B:  uint16_t big-endian

DATA_FILE  = "live_telem.csv"
EVENT_FILE = "events.log"
DICT_FILE  = "dictionary.json"

if "uplink_seq" not in st.session_state:
    st.session_state.uplink_seq = 0

# ---------------------------------------------------------------------------
# Cryptography (SipHash-2-4 & CRC)
# ---------------------------------------------------------------------------
def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = (crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1
        crc &= 0xFFFF
    return crc

def rotl(x: int, b: int) -> int:
    return ((x << b) | (x >> (64 - b))) & 0xFFFFFFFFFFFFFFFF

def sipround(v0: int, v1: int, v2: int, v3: int):
    v0 = (v0 + v1) & 0xFFFFFFFFFFFFFFFF; v1 = rotl(v1, 13); v1 ^= v0; v0 = rotl(v0, 32)
    v2 = (v2 + v3) & 0xFFFFFFFFFFFFFFFF; v3 = rotl(v3, 16); v3 ^= v2
    v0 = (v0 + v3) & 0xFFFFFFFFFFFFFFFF; v3 = rotl(v3, 21); v3 ^= v0
    v2 = (v2 + v1) & 0xFFFFFFFFFFFFFFFF; v1 = rotl(v1, 17); v1 ^= v2; v2 = rotl(v2, 32)
    return v0, v1, v2, v3

def siphash24(key: bytes, msg: bytes) -> int:
    k0 = struct.unpack('<Q', key[0:8])[0]
    k1 = struct.unpack('<Q', key[8:16])[0]
    v0 = k0 ^ 0x736f6d6570736575
    v1 = k1 ^ 0x646f72616e646f6d
    v2 = k0 ^ 0x6c7967656e657261
    v3 = k1 ^ 0x7465646279746573

    b = (len(msg) << 56) & 0xFFFFFFFFFFFFFFFF
    
    for i in range(0, len(msg) - (len(msg) % 8), 8):
        m = struct.unpack('<Q', msg[i:i+8])[0]
        v3 ^= m
        v0, v1, v2, v3 = sipround(v0, v1, v2, v3)
        v0, v1, v2, v3 = sipround(v0, v1, v2, v3)
        v0 ^= m

    rem = msg[len(msg) - (len(msg) % 8):]
    t = 0
    for i, byte in enumerate(rem):
        t |= (byte << (8 * i))
    b |= t

    v3 ^= b
    v0, v1, v2, v3 = sipround(v0, v1, v2, v3)
    v0, v1, v2, v3 = sipround(v0, v1, v2, v3)
    v0 ^= b

    v2 ^= 0xff
    v0, v1, v2, v3 = sipround(v0, v1, v2, v3)
    v0, v1, v2, v3 = sipround(v0, v1, v2, v3)
    v0, v1, v2, v3 = sipround(v0, v1, v2, v3)
    v0, v1, v2, v3 = sipround(v0, v1, v2, v3)

    return v0 ^ v1 ^ v2 ^ v3

# ---------------------------------------------------------------------------
# Dictionary & Uplink Logic
# ---------------------------------------------------------------------------
def load_dictionary() -> dict:
    if os.path.exists(DICT_FILE):
        with open(DICT_FILE) as f: return json.load(f)
    return {"components": {}, "commands": {}}

GDS_DICT = load_dictionary()

def get_component_name(comp_id: int) -> str:
    entry = GDS_DICT["components"].get(str(comp_id))
    if entry: return entry["name"] if isinstance(entry, dict) else entry
    return f"COMP_{comp_id}"

def send_command(target_id: int, opcode: int, argument: float = 0.0) -> None:
    payload = struct.pack(CMD_FORMAT, target_id, opcode, argument)
    header  = struct.pack(HEADER_FORMAT,
                          CCSDS_SYNC_WORD, 30, st.session_state.uplink_seq & 0xFFFF, len(payload))
    st.session_state.uplink_seq += 1
    
    msg = header + payload
    mac = siphash24(UPLINK_KEY, msg)
    mac_bytes = struct.pack('<Q', mac)
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(msg + mac_bytes, ("127.0.0.1", PORT_UPLINK))
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
    except Exception as e:
        print(f"[GDS] Bind error: {e}")
        return

    while True:
        try:
            data, _ = sock.recvfrom(1024)
            header_size = struct.calcsize(HEADER_FORMAT)
            crc_size    = struct.calcsize(CRC_FORMAT)

            if len(data) < header_size + crc_size: continue
            sync, apid, seq, plen = struct.unpack(HEADER_FORMAT, data[:header_size])
            if sync != CCSDS_SYNC_WORD: continue

            payload = data[header_size : header_size + plen]
            crc_bytes = data[header_size + plen : header_size + plen + crc_size]

            if len(crc_bytes) == crc_size:
                expected_crc = crc16_ccitt(payload)
                received_crc, = struct.unpack(CRC_FORMAT, crc_bytes)
                if expected_crc != received_crc: continue

            timestamp = datetime.now().strftime('%H:%M:%S')

            if apid == 10 and len(payload) >= 12:
                met, cid, val = struct.unpack(TELEM_FORMAT, payload[:12])
                name = get_component_name(cid)
                with open(DATA_FILE, "a") as f:
                    f.write(f"{timestamp},{name},{val:.4f}\n")

            elif apid == 20 and len(payload) >= 36:
                sev, src_id, msg_b = struct.unpack(EVENT_FORMAT, payload[:36])
                msg = msg_b.decode("ascii", errors="ignore").strip("\x00")
                sev_str = {1: "INFO", 2: "WARN", 3: "CRIT"}.get(sev, f"SEV{sev}")
                src_name = get_component_name(src_id)
                with open(EVENT_FILE, "a") as f:
                    f.write(f"[{timestamp}][{sev_str}][{src_name}] {msg}\n")

        except socket.timeout: continue
        except Exception: pass

# ---------------------------------------------------------------------------
# UI Setup
# ---------------------------------------------------------------------------
if not os.path.exists(DATA_FILE) or os.stat(DATA_FILE).st_size == 0:
    with open(DATA_FILE, "w") as f: f.write("Time,Component,Value\n")

if not any(t.name == "GDS_LISTENER" for t in threading.enumerate()):
    threading.Thread(target=udp_listener, daemon=True, name="GDS_LISTENER").start()

st.set_page_config(page_title="DELTA-V GDS", layout="wide")
st.title("🛰️  DELTA-V MISSION CONTROL")

def load_data() -> pd.DataFrame | None:
    try:
        df = pd.read_csv(DATA_FILE)
        return df if not df.empty else None
    except Exception: return None

df = load_data()

c1, c2, c3, c4, c5 = st.columns(5)
status    = "🟢 AOS" if df is not None else "🔴 LOS"
last_time = df.iloc[-1]["Time"] if df is not None else "N/A"
num_pkts  = len(df) if df is not None else 0
act_comps = df["Component"].nunique() if df is not None else 0

with c1: st.metric("📡 LINK",         status)
with c2: st.metric("⏱️ LAST PKT",     last_time)
with c3: st.metric("🧩 ACTIVE COMPS", act_comps)
with c4: st.metric("📦 PACKETS",      num_pkts)
with c5: st.metric("🔌 DOWNLINK",     PORT_DOWNLINK)

if df is not None:
    plot_df = df.pivot_table(index="Time", columns="Component", values="Value").tail(60)
    st.line_chart(plot_df)
else:
    st.warning("⏳ Waiting for telemetry — ensure flight_software is running.")

col_left, col_right = st.columns([2, 1])
with col_left:
    st.subheader("📋 Event Log")
    if os.path.exists(EVENT_FILE):
        with open(EVENT_FILE) as f: lines = f.readlines()
        recent = "".join(reversed(lines[-30:]))
        st.text_area("Events (newest first)", value=recent, height=250, label_visibility="collapsed")
    else: st.caption("No events yet.")

with col_right:
    st.subheader("📊 Stats")
    if df is not None:
        st.dataframe(
            df.groupby("Component")["Value"].agg(["last", "min", "max"])
              .rename(columns={"last": "Latest", "min": "Min", "max": "Max"})
              .round(3),
            use_container_width=True
        )

st.sidebar.header("🎮 UPLINK CONTROL")
st.sidebar.caption(f"Flight SW @ UDP :{PORT_UPLINK} | seq={st.session_state.uplink_seq}")
st.sidebar.caption(f"Auth: SipHash-2-4 MAC 🔒")
st.sidebar.divider()

if "commands" in GDS_DICT and GDS_DICT["commands"]:
    cmd_list = list(GDS_DICT["commands"].keys())
    selected_cmd = st.sidebar.selectbox("Select Command", cmd_list)
    cmd_data = GDS_DICT["commands"][selected_cmd]
    
    st.sidebar.caption(f"Target ID: {cmd_data['target_id']} | Opcode: {cmd_data['opcode']}")
    desc = cmd_data.get('description', 'No description provided.')
    st.sidebar.write(f"*{desc}*")
    arg_val = st.sidebar.number_input("Command Argument (Float)", value=0.0, step=0.1)
    
    if st.sidebar.button(f"🚀 SEND COMMAND", use_container_width=True):
        send_command(cmd_data['target_id'], cmd_data['opcode'], arg_val)
        st.sidebar.success(f"Sent {selected_cmd}({arg_val}) to ID {cmd_data['target_id']}")
else:
    st.sidebar.warning("No commands found. Run autocoder.py to generate dictionary.")

st.sidebar.divider()
st.sidebar.caption("Frame: [Header 10B][Payload 12B][SipHash 8B]")

time.sleep(1)
st.rerun()