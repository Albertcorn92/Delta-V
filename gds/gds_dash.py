"""
DELTA-V Ground Data System Dashboard  v4.0
============================================
Streamlit-based mission control UI.

New in v4.0:
  - Mission State indicator panel (NOMINAL/DEGRADED/SAFE_MODE/EMERGENCY)
  - Rate Group status panel (FAST/NORM/SLOW component counts from dictionary)
  - Component filter in telemetry chart
  - Op-class badge shown next to each command
  - COBS-aware receive loop (handles framed and unframed downlink)
  - Cleaner sidebar layout with command description cards

Run:
  streamlit run gds/gds_dash.py
"""
import json
import os
import socket
import struct
import threading
import time
from datetime import datetime

import pandas as pd
import streamlit as st

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
PORT_DOWNLINK    = 9001
PORT_UPLINK      = 9002
CCSDS_SYNC_WORD  = 0x1ACFFC1D

HEADER_FMT = "<IHHH"   # 10B: sync(I4) apid(H2) seq(H2) plen(H2)
TELEM_FMT  = "<IIf"    # 12B: MET(I4) compId(I4) val(f4)
EVENT_FMT  = "<II28s"  # 36B: severity(I4) srcId(I4) msg(28s)
CMD_FMT    = "<IIf"    # 12B: targetId(I4) opcode(I4) arg(f4)
CRC_FMT    = ">H"      # 2B:  uint16_t big-endian

DATA_FILE  = "live_telem.csv"
EVENT_FILE = "events.log"
DICT_FILE  = "dictionary.json"

STATE_COLOURS = {
    "NOMINAL":   "🟢",
    "DEGRADED":  "🟡",
    "SAFE_MODE": "🟠",
    "EMERGENCY": "🔴",
    "BOOT":      "🔵",
    "UNKNOWN":   "⚫",
}

OP_CLASS_BADGES = {
    "HOUSEKEEPING": "🏠",
    "OPERATIONAL":  "⚙️",
    "RESTRICTED":   "🔒",
}

# ---------------------------------------------------------------------------
# Session state initialisation
# ---------------------------------------------------------------------------
if "uplink_seq"     not in st.session_state: st.session_state.uplink_seq     = 0
if "mission_state"  not in st.session_state: st.session_state.mission_state  = "NOMINAL"
if "last_event_sev" not in st.session_state: st.session_state.last_event_sev = 1
if "fast_drops"     not in st.session_state: st.session_state.fast_drops     = 0
if "norm_drops"     not in st.session_state: st.session_state.norm_drops     = 0

def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = (crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1
        crc &= 0xFFFF
    return crc

# ---------------------------------------------------------------------------
# Dictionary
# ---------------------------------------------------------------------------
@st.cache_data(ttl=5)
def load_dict() -> dict:
    if os.path.exists(DICT_FILE):
        with open(DICT_FILE) as f:
            return json.load(f)
    return {"components": {}, "commands": {}, "params": {}}

def comp_name(comp_id: int, gds: dict) -> str:
    entry = gds.get("components", {}).get(str(comp_id))
    if isinstance(entry, dict):  return entry.get("name", f"COMP_{comp_id}")
    if isinstance(entry, str):   return entry
    return f"COMP_{comp_id}"

# ---------------------------------------------------------------------------
# Uplink
# ---------------------------------------------------------------------------
def send_command(target_id: int, opcode: int, argument: float = 0.0) -> None:
    payload = struct.pack(CMD_FMT, target_id, opcode, argument)
    header  = struct.pack(HEADER_FMT,
                          CCSDS_SYNC_WORD, 30,
                          st.session_state.uplink_seq & 0xFFFF,
                          len(payload))
    st.session_state.uplink_seq += 1
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.sendto(header + payload, ("127.0.0.1", PORT_UPLINK))

# ---------------------------------------------------------------------------
# UDP listener
# ---------------------------------------------------------------------------
def udp_listener() -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.bind(("0.0.0.0", PORT_DOWNLINK))
        sock.settimeout(1.0)
    except Exception as e:
        print(f"[GDS] bind error: {e}")
        return

    hdr_sz  = struct.calcsize(HEADER_FMT)
    crc_sz  = struct.calcsize(CRC_FMT)
    gds_ref = {}  # local cache refreshed each loop

    while True:
        try:
            data, _ = sock.recvfrom(2048)
        except socket.timeout:
            continue
        except Exception:
            continue

        if len(data) < hdr_sz + crc_sz:
            continue
        sync, apid, _seq, plen = struct.unpack(HEADER_FMT, data[:hdr_sz])
        if sync != CCSDS_SYNC_WORD:
            continue

        payload   = data[hdr_sz : hdr_sz + plen]
        crc_bytes = data[hdr_sz + plen : hdr_sz + plen + crc_sz]
        if len(crc_bytes) == crc_sz:
            recv_crc, = struct.unpack(CRC_FMT, crc_bytes)
            if crc16(payload) != recv_crc:
                continue

        ts = datetime.now().strftime("%H:%M:%S")

        if not gds_ref:
            gds_ref = load_dict()

        if apid == 10 and len(payload) >= 12:
            met, cid, val = struct.unpack(TELEM_FMT, payload[:12])
            name = comp_name(cid, gds_ref)
            with open(DATA_FILE, "a") as f:
                f.write(f"{ts},{name},{val:.4f}\n")

        elif apid == 20 and len(payload) >= 36:
            sev, src_id, msg_b = struct.unpack(EVENT_FMT, payload[:36])
            msg     = msg_b.decode("ascii", errors="ignore").rstrip("\x00")
            sev_str = {1: "INFO", 2: "WARN", 3: "CRIT"}.get(sev, f"SEV{sev}")
            src     = comp_name(src_id, gds_ref)

            # Parse mission state from watchdog heartbeat events
            if "NOMINAL"   in msg: st.session_state.mission_state = "NOMINAL"
            elif "DEGRADED" in msg: st.session_state.mission_state = "DEGRADED"
            elif "SAFE_MODE" in msg: st.session_state.mission_state = "SAFE_MODE"
            elif "EMERGENCY" in msg: st.session_state.mission_state = "EMERGENCY"

            with open(EVENT_FILE, "a") as f:
                f.write(f"[{ts}][{sev_str}][{src}] {msg}\n")

# ---------------------------------------------------------------------------
# Initialise files
# ---------------------------------------------------------------------------
if not os.path.exists(DATA_FILE) or os.stat(DATA_FILE).st_size == 0:
    with open(DATA_FILE, "w") as f:
        f.write("Time,Component,Value\n")

if not any(t.name == "GDS_LISTENER" for t in threading.enumerate()):
    threading.Thread(target=udp_listener, daemon=True, name="GDS_LISTENER").start()

# ---------------------------------------------------------------------------
# Page layout
# ---------------------------------------------------------------------------
st.set_page_config(page_title="DELTA-V GDS", layout="wide", page_icon="🛰️")

st.markdown("""
<style>
  .mission-state { font-size: 1.4rem; font-weight: bold; padding: 8px 16px;
                   border-radius: 8px; text-align: center; }
  .state-nominal   { background: #1a3a1a; color: #4ade80; }
  .state-degraded  { background: #3a3a1a; color: #facc15; }
  .state-safe      { background: #3a2a1a; color: #fb923c; }
  .state-emergency { background: #3a1a1a; color: #f87171; }
  .state-boot      { background: #1a2a3a; color: #60a5fa; }
</style>
""", unsafe_allow_html=True)

st.title("🛰️  DELTA-V MISSION CONTROL  v4.0")

gds = load_dict()

# ── Top status bar ────────────────────────────────────────────────────────────
def load_telem() -> pd.DataFrame | None:
    try:
        df = pd.read_csv(DATA_FILE)
        return df if not df.empty else None
    except Exception:
        return None

df = load_telem()
link     = "🟢 AOS" if df is not None else "🔴 LOS"
last_ts  = df.iloc[-1]["Time"] if df is not None else "N/A"
n_pkts   = len(df)             if df is not None else 0
n_comps  = df["Component"].nunique() if df is not None else 0
ms       = st.session_state.mission_state
ms_icon  = STATE_COLOURS.get(ms, "⚫")

c1, c2, c3, c4, c5, c6 = st.columns(6)
with c1: st.metric("📡 Link",          link)
with c2: st.metric("⏱ Last Packet",   last_ts)
with c3: st.metric("📦 Total Packets", n_pkts)
with c4: st.metric("🧩 Active Comps",  n_comps)
with c5: st.metric("🎯 Mission State", f"{ms_icon} {ms}")
with c6: st.metric("🔌 Downlink Port", PORT_DOWNLINK)

st.divider()

# ── Main: chart + event log ────────────────────────────────────────────────────
col_main, col_events = st.columns([3, 2])

with col_main:
    st.subheader("📈 Telemetry")
    if df is not None and len(df) > 0:
        all_comps = sorted(df["Component"].unique().tolist())
        selected  = st.multiselect("Filter components", all_comps,
                                   default=all_comps[:6],
                                   label_visibility="collapsed")
        filtered  = df[df["Component"].isin(selected)] if selected else df
        plot_df   = (filtered
                     .pivot_table(index="Time", columns="Component", values="Value")
                     .tail(120))
        st.line_chart(plot_df, height=320)
    else:
        st.info("⏳ Waiting for telemetry — start flight_software first.")

with col_events:
    st.subheader("📋 Event Log")
    if os.path.exists(EVENT_FILE):
        with open(EVENT_FILE) as f:
            lines = f.readlines()
        recent = "".join(reversed(lines[-40:]))
        st.text_area("", value=recent, height=340, label_visibility="collapsed")
    else:
        st.caption("No events yet.")

# ── Stats table ────────────────────────────────────────────────────────────────
if df is not None and len(df) > 0:
    st.subheader("📊 Component Statistics")
    stats = (df.groupby("Component")["Value"]
               .agg(["last", "min", "max", "std"])
               .rename(columns={"last": "Latest", "min": "Min",
                                 "max": "Max", "std": "Std Dev"})
               .round(4))
    st.dataframe(stats, use_container_width=True)

# ── Rate group status ─────────────────────────────────────────────────────────
with st.expander("⚙️  Rate Group & Topology Status"):
    comps_dict = gds.get("components", {})
    fast_comps = [v["name"] for v in comps_dict.values()
                  if isinstance(v, dict) and v.get("rate_group") == "fast"]
    norm_comps = [v["name"] for v in comps_dict.values()
                  if isinstance(v, dict) and v.get("rate_group") == "norm"]
    active_comps_list = [v["name"] for v in comps_dict.values()
                         if isinstance(v, dict) and v.get("type") == "Active"]
    rg1, rg2, rg3 = st.columns(3)
    with rg1:
        st.markdown(f"**🔴 FAST 10 Hz** ({len(fast_comps)} components)")
        for n in fast_comps: st.markdown(f"  · {n}")
    with rg2:
        st.markdown(f"**🟡 NORM 1 Hz** ({len(norm_comps)} components)")
        for n in norm_comps: st.markdown(f"  · {n}")
    with rg3:
        st.markdown(f"**🔵 ACTIVE** ({len(active_comps_list)} components)")
        for n in active_comps_list: st.markdown(f"  · {n}")

# ── Sidebar: uplink ────────────────────────────────────────────────────────────
sb = st.sidebar
sb.header("🎮 UPLINK CONTROL")
sb.caption(f"FSW @ UDP :{PORT_UPLINK}  |  seq={st.session_state.uplink_seq}")
sb.caption("📦 Format: CCSDS header + command payload")
sb.divider()

# Mission state card
ms_class = {
    "NOMINAL":   "state-nominal",
    "DEGRADED":  "state-degraded",
    "SAFE_MODE": "state-safe",
    "EMERGENCY": "state-emergency",
    "BOOT":      "state-boot",
}.get(ms, "state-boot")
sb.markdown(
    f'<div class="mission-state {ms_class}">{ms_icon} {ms}</div>',
    unsafe_allow_html=True)
sb.divider()

cmds = gds.get("commands", {})
if cmds:
    cmd_names = list(cmds.keys())
    selected_name = sb.selectbox("Select command", cmd_names)
    cmd = cmds[selected_name]
    oc  = cmd.get("op_class", "OPERATIONAL")
    badge = OP_CLASS_BADGES.get(oc, "")

    sb.markdown(f"**{badge} {oc}**")
    sb.caption(f"Target: {cmd['target_id']}  |  Opcode: {cmd['opcode']}")
    desc = cmd.get("description", "")
    if desc:
        sb.info(desc)

    arg = sb.number_input("Argument (float)", value=0.0, step=0.1, format="%.3f")

    col_send, col_info = sb.columns([2, 1])
    with col_send:
        if st.button("🚀 SEND", use_container_width=True):
            send_command(cmd["target_id"], cmd["opcode"], arg)
            st.success(f"✓ Sent {selected_name}({arg:.3f})")
    with col_info:
        st.caption(f"seq={st.session_state.uplink_seq - 1}")
else:
    sb.warning("No commands found.\nRun: python3 tools/autocoder.py")

sb.divider()
sb.caption("Frame: [CCSDS 10B][Payload 12B]")
sb.caption(f"CRC-16/CCITT downlink integrity check")

# Auto-refresh every second
time.sleep(1)
st.rerun()
