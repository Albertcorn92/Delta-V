"""
DELTA-V Ground Data System Dashboard  v5.0
=========================================
Launch-operations styled Streamlit dashboard for civilian mission simulation.

Run:
  streamlit run gds/gds_dash.py
"""

import json
import math
import os
import re
import socket
import struct
import threading
import time
from datetime import datetime, timezone
from html import escape

import pandas as pd
import streamlit as st

try:
    import altair as alt
except ImportError:
    alt = None

# ---------------------------------------------------------------------------
# Streamlit config
# ---------------------------------------------------------------------------
st.set_page_config(page_title="DELTA-V GDS", layout="wide", page_icon="🚀")

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
PORT_DOWNLINK = 9001
PORT_UPLINK = 9002
CCSDS_SYNC_WORD = 0x1ACFFC1D

HEADER_FMT = "<IHHH"  # sync(I4) apid(H2) seq(H2) plen(H2)
TELEM_FMT = "<IIf"  # MET(I4) compId(I4) val(f4)
EVENT_FMT = "<II28s"  # severity(I4) srcId(I4) msg(28s)
CMD_FMT = "<IIf"  # targetId(I4) opcode(I4) arg(f4)
CRC_FMT = ">H"  # uint16_t big-endian

DATA_FILE = "live_telem.csv"
EVENT_FILE = "events.log"
DICT_FILE = "dictionary.json"

STATE_STYLES = {
    "NOMINAL": "state-nominal",
    "DEGRADED": "state-degraded",
    "SAFE_MODE": "state-safe",
    "EMERGENCY": "state-emergency",
    "BOOT": "state-boot",
    "UNKNOWN": "state-unknown",
}

# Listener thread writes mission state to this shared cache.
_STATE_LOCK = threading.Lock()
_MISSION_STATE = "NOMINAL"


# ---------------------------------------------------------------------------
# Session state
# ---------------------------------------------------------------------------
if "uplink_seq" not in st.session_state:
    st.session_state.uplink_seq = 0
if "mission_state" not in st.session_state:
    st.session_state.mission_state = "NOMINAL"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = (crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1
        crc &= 0xFFFF
    return crc


def read_dict_file() -> dict:
    if os.path.exists(DICT_FILE):
        with open(DICT_FILE) as f:
            return json.load(f)
    return {"_meta": {}, "components": {}, "commands": {}, "params": {}}


@st.cache_data(ttl=5)
def load_dict() -> dict:
    return read_dict_file()


def comp_name(comp_id: int, gds: dict) -> str:
    entry = gds.get("components", {}).get(str(comp_id))
    if isinstance(entry, dict):
        return entry.get("name", f"COMP_{comp_id}")
    if isinstance(entry, str):
        return entry
    return f"COMP_{comp_id}"


def set_mission_state(state: str) -> None:
    global _MISSION_STATE
    with _STATE_LOCK:
        _MISSION_STATE = state


def get_mission_state() -> str:
    with _STATE_LOCK:
        return _MISSION_STATE


def parse_state_from_event(msg: str) -> str | None:
    if "NOMINAL" in msg:
        return "NOMINAL"
    if "DEGRADED" in msg:
        return "DEGRADED"
    if "SAFE_MODE" in msg:
        return "SAFE_MODE"
    if "EMERGENCY" in msg:
        return "EMERGENCY"
    return None


def load_telem() -> pd.DataFrame | None:
    if not os.path.exists(DATA_FILE):
        return None
    try:
        df = pd.read_csv(DATA_FILE)
    except Exception:
        return None
    if df.empty:
        return None
    df["Value"] = pd.to_numeric(df["Value"], errors="coerce")
    df = df.dropna(subset=["Value"])  # defensive against partial writes
    if df.empty:
        return None
    return df.tail(4000)


def load_recent_events(max_lines: int = 80) -> list[dict]:
    if not os.path.exists(EVENT_FILE):
        return []

    pattern = re.compile(r"^\[(?P<time>[^\]]+)\]\[(?P<sev>[^\]]+)\]\[(?P<src>[^\]]+)\]\s*(?P<msg>.*)$")
    events: list[dict] = []
    with open(EVENT_FILE) as f:
        lines = f.readlines()[-max_lines:]

    for line in lines:
        clean = line.strip()
        match = pattern.match(clean)
        if not match:
            events.append({"time": "--", "sev": "INFO", "src": "UNKNOWN", "msg": clean})
            continue
        events.append(match.groupdict())
    return list(reversed(events))


def event_class(sev: str) -> str:
    if sev == "CRIT":
        return "evt-crit"
    if sev == "WARN":
        return "evt-warn"
    return "evt-info"


def send_command(target_id: int, opcode: int, argument: float = 0.0) -> None:
    payload = struct.pack(CMD_FMT, target_id, opcode, argument)
    header = struct.pack(
        HEADER_FMT,
        CCSDS_SYNC_WORD,
        30,
        st.session_state.uplink_seq & 0xFFFF,
        len(payload),
    )
    st.session_state.uplink_seq += 1
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(header + payload, ("127.0.0.1", PORT_UPLINK))


def file_age_seconds(path: str) -> int | None:
    if not os.path.exists(path):
        return None
    try:
        age = int(time.time() - os.path.getmtime(path))
    except OSError:
        return None
    return max(age, 0)


def format_age(seconds: int | None) -> str:
    if seconds is None:
        return "not created"
    if seconds < 60:
        return f"{seconds}s ago"
    if seconds < 3600:
        return f"{seconds // 60}m ago"
    return f"{seconds // 3600}h ago"


def build_preview_frame(samples: int = 90) -> pd.DataFrame:
    idx = list(range(samples))
    star = [6.0 + math.sin(i / 8.0) * 1.1 for i in idx]
    battery = [92.0 - (i * 0.08) for i in idx]
    imu = [0.2 + math.sin(i / 5.0) * 0.3 for i in idx]
    return pd.DataFrame(
        {
            "StarTracker(sim)": star,
            "BatterySystem(sim)": battery,
            "IMU_01(sim)": imu,
        },
        index=idx,
    )


def render_telemetry_chart(plot_df: pd.DataFrame, height: int = 340) -> None:
    if alt is None:
        st.line_chart(plot_df, height=height, use_container_width=True)
        return

    chart_data = (
        plot_df.reset_index()
        .melt(id_vars=["Time"], var_name="Component", value_name="Value")
        .dropna(subset=["Value"])
    )
    if chart_data.empty:
        st.info("No plottable telemetry rows in current filter window.")
        return

    chart = (
        alt.Chart(chart_data)
        .mark_line(strokeWidth=2.2)
        .encode(
            x=alt.X(
                "Time:N",
                title="Time (HH:MM:SS)",
                axis=alt.Axis(labelAngle=-40, labelColor="#c5d9ef", titleColor="#d7e7f8", tickColor="#90b4d8"),
            ),
            y=alt.Y(
                "Value:Q",
                title="Value",
                axis=alt.Axis(labelColor="#c5d9ef", titleColor="#d7e7f8", tickColor="#90b4d8"),
            ),
            color=alt.Color(
                "Component:N",
                legend=alt.Legend(
                    title="Component",
                    titleColor="#d7e7f8",
                    labelColor="#c5d9ef",
                    orient="bottom",
                ),
            ),
            tooltip=["Time:N", "Component:N", alt.Tooltip("Value:Q", format=".4f")],
        )
        .properties(height=height)
        .configure_view(strokeOpacity=0)
        .configure(background="#0a2643")
        .configure_axis(gridColor="rgba(157,195,232,0.18)", domainColor="rgba(157,195,232,0.35)")
        .interactive()
    )
    st.altair_chart(chart, use_container_width=True)


# ---------------------------------------------------------------------------
# UDP listener
# ---------------------------------------------------------------------------
def udp_listener() -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.bind(("0.0.0.0", PORT_DOWNLINK))
        sock.settimeout(1.0)
    except Exception as err:
        print(f"[GDS] bind error: {err}")
        return

    hdr_sz = struct.calcsize(HEADER_FMT)
    crc_sz = struct.calcsize(CRC_FMT)
    gds_ref = {}

    while True:
        try:
            data, _addr = sock.recvfrom(2048)
        except socket.timeout:
            continue
        except Exception:
            continue

        if len(data) < hdr_sz + crc_sz:
            continue

        sync, apid, _seq, plen = struct.unpack(HEADER_FMT, data[:hdr_sz])
        if sync != CCSDS_SYNC_WORD:
            continue

        payload = data[hdr_sz : hdr_sz + plen]
        crc_bytes = data[hdr_sz + plen : hdr_sz + plen + crc_sz]
        if len(crc_bytes) == crc_sz:
            recv_crc, = struct.unpack(CRC_FMT, crc_bytes)
            if crc16(payload) != recv_crc:
                continue

        ts = datetime.now().strftime("%H:%M:%S")

        if not gds_ref:
            gds_ref = read_dict_file()

        if apid == 10 and len(payload) >= 12:
            _met, comp_id, value = struct.unpack(TELEM_FMT, payload[:12])
            name = comp_name(comp_id, gds_ref)
            with open(DATA_FILE, "a") as f:
                f.write(f"{ts},{name},{value:.4f}\n")

        elif apid == 20 and len(payload) >= 36:
            severity, src_id, msg_b = struct.unpack(EVENT_FMT, payload[:36])
            msg = msg_b.decode("ascii", errors="ignore").rstrip("\x00")
            sev = {1: "INFO", 2: "WARN", 3: "CRIT"}.get(severity, f"SEV{severity}")
            src = comp_name(src_id, gds_ref)

            state = parse_state_from_event(msg)
            if state:
                set_mission_state(state)

            with open(EVENT_FILE, "a") as f:
                f.write(f"[{ts}][{sev}][{src}] {msg}\n")


# ---------------------------------------------------------------------------
# Initialise files and listener
# ---------------------------------------------------------------------------
if not os.path.exists(DATA_FILE) or os.stat(DATA_FILE).st_size == 0:
    with open(DATA_FILE, "w") as f:
        f.write("Time,Component,Value\n")

if not any(thread.name == "GDS_LISTENER" for thread in threading.enumerate()):
    threading.Thread(target=udp_listener, daemon=True, name="GDS_LISTENER").start()

# ---------------------------------------------------------------------------
# Styling
# ---------------------------------------------------------------------------
st.markdown(
    """
<style>
@import url('https://fonts.googleapis.com/css2?family=Orbitron:wght@500;700&family=Space+Mono:wght@400;700&family=Barlow:wght@400;600;700&display=swap');

:root {
  --text: #eef4ff;
  --muted: #a7c0dd;
  --accent: #ff9a3d;
  --accent-soft: rgba(255, 154, 61, 0.20);
  --panel: rgba(14, 43, 73, 0.86);
  --panel-strong: rgba(11, 34, 58, 0.96);
  --line: rgba(157, 195, 232, 0.30);
  --ok: #6ee7b7;
  --warn: #fcd34d;
  --safe: #fb923c;
  --crit: #f87171;
}

.stApp {
  background:
    radial-gradient(1200px 420px at 88% -12%, rgba(255, 154, 61, 0.30), transparent 60%),
    radial-gradient(920px 360px at -10% 12%, rgba(56, 189, 248, 0.18), transparent 56%),
    linear-gradient(165deg, #051427 0%, #0a223f 48%, #102f4c 100%);
  color: var(--text);
  font-family: "Barlow", "Trebuchet MS", sans-serif;
}

.block-container {
  padding-top: 1.1rem;
  padding-bottom: 1.6rem;
}

.hero-shell {
  background: linear-gradient(150deg, rgba(12, 38, 64, 0.92), rgba(19, 58, 92, 0.86));
  border: 1px solid var(--line);
  border-radius: 18px;
  padding: 20px 22px;
  margin-bottom: 0.8rem;
  box-shadow: 0 22px 38px rgba(0, 0, 0, 0.28);
  animation: panel-rise 0.55s ease-out both;
}

.hero-kicker {
  letter-spacing: 0.18rem;
  text-transform: uppercase;
  font-size: 0.72rem;
  color: var(--muted);
  margin-bottom: 0.35rem;
}

.hero-title {
  font-family: "Orbitron", "Space Mono", monospace;
  font-size: clamp(1.5rem, 2.4vw, 2.1rem);
  font-weight: 700;
  letter-spacing: 0.06rem;
  margin-bottom: 0.3rem;
}

.hero-sub {
  color: #d8e6f7;
  font-size: 0.94rem;
  margin-bottom: 0.75rem;
}

.hero-meta {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(170px, 1fr));
  gap: 0.55rem;
}

.hero-pill {
  background: rgba(8, 29, 50, 0.66);
  border: 1px solid rgba(157, 195, 232, 0.26);
  border-radius: 12px;
  padding: 7px 10px;
  font-size: 0.78rem;
  color: var(--muted);
}

.hero-pill strong {
  color: var(--text);
  margin-left: 4px;
  font-family: "Space Mono", monospace;
}

[data-testid="stMetric"] {
  background: linear-gradient(160deg, rgba(9, 31, 52, 0.95), rgba(16, 48, 80, 0.88));
  border: 1px solid rgba(157, 195, 232, 0.28);
  border-radius: 14px;
  padding: 10px 12px;
  box-shadow: 0 8px 18px rgba(0, 0, 0, 0.22);
}

[data-testid="stMetricLabel"] {
  color: var(--muted);
}

[data-testid="stMetricValue"] {
  color: var(--text);
  font-family: "Space Mono", monospace;
  font-size: 2.05rem;
  line-height: 1.05;
}

.panel {
  background: var(--panel);
  border: 1px solid var(--line);
  border-radius: 16px;
  padding: 14px 14px 12px;
  box-shadow: 0 14px 26px rgba(0, 0, 0, 0.20);
}

.panel-title {
  font-family: "Orbitron", "Space Mono", monospace;
  font-size: 0.98rem;
  letter-spacing: 0.04rem;
  margin-bottom: 0.62rem;
}

.state-chip {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  border-radius: 999px;
  padding: 6px 12px;
  font-size: 0.78rem;
  font-weight: 700;
  letter-spacing: 0.04rem;
  text-transform: uppercase;
}

.state-nominal { background: rgba(110, 231, 183, 0.18); color: var(--ok); border: 1px solid rgba(110, 231, 183, 0.38); }
.state-degraded { background: rgba(252, 211, 77, 0.16); color: var(--warn); border: 1px solid rgba(252, 211, 77, 0.40); }
.state-safe { background: rgba(251, 146, 60, 0.18); color: var(--safe); border: 1px solid rgba(251, 146, 60, 0.40); }
.state-emergency { background: rgba(248, 113, 113, 0.18); color: var(--crit); border: 1px solid rgba(248, 113, 113, 0.40); }
.state-boot, .state-unknown { background: rgba(147, 197, 253, 0.16); color: #93c5fd; border: 1px solid rgba(147, 197, 253, 0.40); }

.event-stream {
  border: 1px solid var(--line);
  border-radius: 12px;
  background: var(--panel-strong);
  max-height: 365px;
  overflow-y: auto;
  padding: 8px;
}

.event-row {
  display: grid;
  grid-template-columns: 60px 54px minmax(84px, 108px) 1fr;
  gap: 8px;
  align-items: center;
  border-radius: 9px;
  padding: 6px 8px;
  margin-bottom: 6px;
  border: 1px solid rgba(157, 195, 232, 0.20);
  background: rgba(8, 27, 47, 0.82);
  font-size: 0.75rem;
}

.event-row .evt-time,
.event-row .evt-sev,
.event-row .evt-src {
  font-family: "Space Mono", monospace;
}

.evt-info { border-left: 3px solid #93c5fd; }
.evt-warn { border-left: 3px solid var(--warn); }
.evt-crit { border-left: 3px solid var(--crit); }

.rate-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
  gap: 10px;
}

.rate-card {
  border-radius: 12px;
  padding: 10px;
  border: 1px solid rgba(157, 195, 232, 0.28);
  background: rgba(9, 32, 54, 0.82);
}

.rate-card h4 {
  margin: 0 0 6px;
  font-size: 0.84rem;
  letter-spacing: 0.04rem;
}

.rate-card ul {
  margin: 0;
  padding-left: 1rem;
  font-size: 0.78rem;
  color: #d2e3f7;
}

.startup-grid {
  display: grid;
  grid-template-columns: 1.35fr 1fr;
  gap: 10px;
}

.startup-card {
  border-radius: 12px;
  border: 1px solid rgba(157, 195, 232, 0.30);
  background: rgba(7, 30, 53, 0.82);
  padding: 12px;
}

.startup-card h4 {
  margin: 0 0 6px;
  font-size: 0.9rem;
  letter-spacing: 0.03rem;
}

.startup-card p {
  margin: 0;
  color: #d3e5f8;
  font-size: 0.82rem;
}

.event-empty {
  border: 1px dashed rgba(157, 195, 232, 0.35);
  border-radius: 10px;
  background: rgba(8, 27, 47, 0.62);
  padding: 12px;
  color: #d2e6f8;
  font-size: 0.84rem;
}

[data-testid="stSidebar"] {
  background:
    radial-gradient(440px 220px at 100% 0%, rgba(255, 154, 61, 0.20), transparent 72%),
    linear-gradient(180deg, #071a2e 0%, #0e2a46 100%);
  border-left: 1px solid rgba(157, 195, 232, 0.20);
  min-width: 300px !important;
  max-width: 300px !important;
}

[data-testid="stSidebar"] h1,
[data-testid="stSidebar"] h2,
[data-testid="stSidebar"] h3,
[data-testid="stSidebar"] label,
[data-testid="stSidebar"] p,
[data-testid="stSidebar"] span,
[data-testid="stSidebar"] .stCaption {
  color: #d4e7fa !important;
}

[data-testid="stSidebar"] [data-baseweb="select"] > div,
[data-testid="stSidebar"] input {
  background: rgba(9, 33, 56, 0.86) !important;
  border: 1px solid rgba(157, 195, 232, 0.30) !important;
  color: #edf5ff !important;
}

[data-testid="stSidebar"] [data-baseweb="select"] * {
  color: #edf5ff !important;
}

[data-testid="stSidebar"] [data-testid="stCodeBlock"] {
  border: 1px solid rgba(157, 195, 232, 0.25);
}

[data-testid="stCodeBlock"] {
  background: rgba(9, 33, 56, 0.86) !important;
  border: 1px solid rgba(157, 195, 232, 0.28);
}

[data-testid="stCodeBlock"] pre, [data-testid="stCodeBlock"] code {
  color: #eaf4ff !important;
}

[data-testid="stSidebar"] .stButton>button {
  background: linear-gradient(160deg, #e9782d, #f8a64e);
  color: #1a1208;
  border: none;
  font-weight: 700;
  letter-spacing: 0.02rem;
}

[data-testid="stSidebar"] .stButton>button:hover {
  filter: brightness(1.05);
}

@media (max-width: 900px) {
  .hero-meta {
    grid-template-columns: 1fr 1fr;
  }

  .event-row {
    grid-template-columns: 60px 50px 1fr;
  }

  .event-row .evt-src {
    display: none;
  }

  .startup-grid {
    grid-template-columns: 1fr;
  }

  [data-testid="stSidebar"] {
    min-width: auto !important;
    max-width: none !important;
  }
}

@keyframes panel-rise {
  from { transform: translateY(10px); opacity: 0; }
  to { transform: translateY(0); opacity: 1; }
}
</style>
""",
    unsafe_allow_html=True,
)

# ---------------------------------------------------------------------------
# Data load
# ---------------------------------------------------------------------------
gds = load_dict()
meta = gds.get("_meta", {})
df = load_telem()
recent_events = load_recent_events()

st.session_state.mission_state = get_mission_state() or st.session_state.mission_state
mission_state = st.session_state.mission_state
mission_class = STATE_STYLES.get(mission_state, "state-unknown")

system_name = meta.get("system", "DELTA-V")
version = meta.get("version", "unknown")
utc_stamp = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC")

link = "AOS" if df is not None else "LOS"
last_ts = df.iloc[-1]["Time"] if df is not None else "N/A"
packet_count = len(df) if df is not None else 0
component_count = df["Component"].nunique() if df is not None else 0
has_data = df is not None and len(df) > 0
telem_file_age = file_age_seconds(DATA_FILE)
event_file_age = file_age_seconds(EVENT_FILE)

state_display = {
    "NOMINAL": "NOM",
    "DEGRADED": "DEGR",
    "SAFE_MODE": "SAFE",
    "EMERGENCY": "EMERG",
    "BOOT": "BOOT",
}.get(mission_state, mission_state[:5])
last_packet_display = last_ts[-8:] if last_ts != "N/A" else "N/A"

# ---------------------------------------------------------------------------
# Header
# ---------------------------------------------------------------------------
st.markdown(
    f"""
<section class="hero-shell">
  <div class="hero-kicker">Ground Data System</div>
  <div class="hero-title">Launch Operations Console</div>
  <div class="hero-sub">Civilian mission control stack for {escape(system_name)}. Real-time telemetry ingest, event watch, and uplink command dispatch.</div>
  <div class="hero-meta">
    <div class="hero-pill">SYSTEM<strong>{escape(system_name)}</strong></div>
    <div class="hero-pill">VERSION<strong>{escape(str(version))}</strong></div>
    <div class="hero-pill">DOWNLINK<strong>UDP {PORT_DOWNLINK}</strong></div>
    <div class="hero-pill">UPLINK<strong>UDP {PORT_UPLINK}</strong></div>
    <div class="hero-pill">UTC<strong>{escape(utc_stamp)}</strong></div>
    <div class="hero-pill"><span class="state-chip {mission_class}">{escape(mission_state)}</span></div>
  </div>
</section>
""",
    unsafe_allow_html=True,
)

# ---------------------------------------------------------------------------
# Status row
# ---------------------------------------------------------------------------
m1, m2, m3, m4, m5, m6 = st.columns(6)
with m1:
    st.metric("Link", link)
with m2:
    st.metric("Last Packet", last_packet_display)
with m3:
    st.metric("Packets", packet_count)
with m4:
    st.metric("Components", component_count)
with m5:
    st.metric("State", state_display)
with m6:
    st.metric("Uplink Seq", st.session_state.uplink_seq)

st.markdown("<div style='height:6px'></div>", unsafe_allow_html=True)

# ---------------------------------------------------------------------------
# Main panels
# ---------------------------------------------------------------------------
main_col, event_col = st.columns([2.3, 1.4])

with main_col:
    st.markdown('<div class="panel">', unsafe_allow_html=True)
    st.markdown('<div class="panel-title">Telemetry Plot</div>', unsafe_allow_html=True)

    if has_data:
        all_components = sorted(df["Component"].unique().tolist())
        default_components = all_components[: min(8, len(all_components))]
        selected_components = st.multiselect(
            "Components",
            all_components,
            default=default_components,
            label_visibility="collapsed",
        )

        filtered = df[df["Component"].isin(selected_components)] if selected_components else df
        plot_df = filtered.pivot_table(
            index="Time",
            columns="Component",
            values="Value",
            aggfunc="last",
        ).tail(160)
        render_telemetry_chart(plot_df, height=340)

        stats_tab, boot_tab = st.tabs(["Component Stats", "Boot Checklist"])
        with stats_tab:
            stats = (
                df.groupby("Component")["Value"]
                .agg(["last", "min", "max", "std"])
                .rename(columns={"last": "Latest", "min": "Min", "max": "Max", "std": "StdDev"})
                .round(4)
            )
            st.dataframe(stats, use_container_width=True, height=220)

        with boot_tab:
            checks = [
                ("boot_radio", "Radio bridge linked"),
                ("boot_telem", "Telemetry stream stable"),
                ("boot_events", "Event logger receiving"),
                ("boot_cmd", "Uplink command path verified"),
            ]
            done = 0
            for key, label in checks:
                if key not in st.session_state:
                    st.session_state[key] = False
                checked = st.checkbox(label, value=st.session_state[key], key=key)
                if checked:
                    done += 1
            pct = done / len(checks)
            st.progress(pct, text=f"Boot completion: {done}/{len(checks)}")
            if pct == 1.0:
                st.success("All startup checks complete. Console is launch-ready.")
    else:
        st.markdown(
            f"""
<div class="startup-grid">
  <div class="startup-card">
    <h4>No Downlink Telemetry Yet</h4>
    <p>The console is armed and listening on UDP {PORT_DOWNLINK}. Start the flight software process to begin live packet flow.</p>
  </div>
  <div class="startup-card">
    <h4>Link Diagnostics</h4>
    <p>Telemetry log updated: {escape(format_age(telem_file_age))}</p>
    <p>Event log updated: {escape(format_age(event_file_age))}</p>
    <p>Current packet count: {packet_count}</p>
  </div>
</div>
""",
            unsafe_allow_html=True,
        )

        with st.expander("Startup checks and preview", expanded=True):
            startup_checks = [
                ("startup_fsw", "Flight software process running"),
                ("startup_radio", "Radio/UART bridge active"),
                ("startup_ports", "UDP ports 9001/9002 reachable"),
                ("startup_path", "dictionary.json regenerated"),
            ]
            completed = 0
            for key, label in startup_checks:
                if key not in st.session_state:
                    st.session_state[key] = False
                if st.checkbox(label, value=st.session_state[key], key=key):
                    completed += 1
            st.progress(completed / len(startup_checks), text=f"Startup readiness: {completed}/{len(startup_checks)}")

            cmd_col_a, cmd_col_b = st.columns([1.1, 1.0])
            with cmd_col_a:
                st.caption("Terminal quick start")
                st.code(
                    "./build/flight_software\n# then in another terminal\nstreamlit run gds/gds_dash.py",
                    language="bash",
                )
            with cmd_col_b:
                preview_enabled = st.toggle("Show simulated preview", value=True, key="show_preview")
                st.caption("Preview mode uses synthetic traces so you can inspect the layout before live packets.")

            if preview_enabled:
                render_telemetry_chart(build_preview_frame(), height=250)

    st.markdown('</div>', unsafe_allow_html=True)

with event_col:
    st.markdown('<div class="panel">', unsafe_allow_html=True)
    st.markdown('<div class="panel-title">Event Stream</div>', unsafe_allow_html=True)

    if recent_events:
        rows = []
        for event in recent_events[:60]:
            row = (
                f"<div class='event-row {event_class(event['sev'])}'>"
                f"<span class='evt-time'>{escape(event['time'])}</span>"
                f"<span class='evt-sev'>{escape(event['sev'])}</span>"
                f"<span class='evt-src'>{escape(event['src'])}</span>"
                f"<span class='evt-msg'>{escape(event['msg'])}</span>"
                "</div>"
            )
            rows.append(row)
        st.markdown(f"<div class='event-stream'>{''.join(rows)}</div>", unsafe_allow_html=True)
    else:
        st.markdown(
            """
<div class="event-empty">
  Event stream is quiet. Once flight software is online, this panel will show INFO/WARN/CRIT packets in real time.
</div>
""",
            unsafe_allow_html=True,
        )
        st.caption("Expected startup events: watchdog heartbeat, bridge-ready marker, component init messages.")

    sev_counts = {"INFO": 0, "WARN": 0, "CRIT": 0}
    for event in recent_events:
        sev = event.get("sev", "INFO")
        if sev in sev_counts:
            sev_counts[sev] += 1

    e1, e2, e3 = st.columns(3)
    e1.metric("INFO", sev_counts["INFO"])
    e2.metric("WARN", sev_counts["WARN"])
    e3.metric("CRIT", sev_counts["CRIT"])

    st.markdown('</div>', unsafe_allow_html=True)

# ---------------------------------------------------------------------------
# Topology panel
# ---------------------------------------------------------------------------
st.markdown('<div class="panel" style="margin-top: 0.85rem;">', unsafe_allow_html=True)
st.markdown('<div class="panel-title">Rate Groups and Component Topology</div>', unsafe_allow_html=True)

components = gds.get("components", {})
fast_components = [
    value["name"]
    for value in components.values()
    if isinstance(value, dict) and value.get("rate_group") == "fast"
]
norm_components = [
    value["name"]
    for value in components.values()
    if isinstance(value, dict) and value.get("rate_group") == "norm"
]
slow_components = [
    value["name"]
    for value in components.values()
    if isinstance(value, dict) and value.get("rate_group") == "slow"
]
active_components = [
    value["name"]
    for value in components.values()
    if isinstance(value, dict) and value.get("type") == "Active"
]


def component_list_html(items: list[str]) -> str:
    if not items:
        return "<ul><li>None</li></ul>"
    rows = "".join(f"<li>{escape(item)}</li>" for item in items)
    return f"<ul>{rows}</ul>"


st.markdown(
    f"""
<div class="rate-grid">
  <div class="rate-card">
    <h4>FAST 10 Hz ({len(fast_components)})</h4>
    {component_list_html(fast_components)}
  </div>
  <div class="rate-card">
    <h4>NORM 1 Hz ({len(norm_components)})</h4>
    {component_list_html(norm_components)}
  </div>
  <div class="rate-card">
    <h4>SLOW 0.1 Hz ({len(slow_components)})</h4>
    {component_list_html(slow_components)}
  </div>
  <div class="rate-card">
    <h4>ACTIVE THREADS ({len(active_components)})</h4>
    {component_list_html(active_components)}
  </div>
</div>
""",
    unsafe_allow_html=True,
)
st.markdown('</div>', unsafe_allow_html=True)

# ---------------------------------------------------------------------------
# Sidebar command console
# ---------------------------------------------------------------------------
sb = st.sidebar
sb.title("Command Console")
sb.caption(f"Uplink UDP :{PORT_UPLINK}")
sb.caption(f"Frame: CCSDS header + payload, seq={st.session_state.uplink_seq}")
sb.markdown(f"<span class='state-chip {mission_class}'>{escape(mission_state)}</span>", unsafe_allow_html=True)

commands = gds.get("commands", {})
if commands:
    classes = ["ALL", "HOUSEKEEPING", "OPERATIONAL", "RESTRICTED"]
    selected_class = sb.selectbox("Filter class", classes, index=0)

    names = [
        cmd_name
        for cmd_name, cmd_data in commands.items()
        if selected_class == "ALL" or cmd_data.get("op_class") == selected_class
    ]
    if names:
        selected_name = sb.selectbox("Command", names)
        command = commands[selected_name]

        sb.code(
            f"target_id={command['target_id']}\nopcode={command['opcode']}\nop_class={command.get('op_class', 'OPERATIONAL')}",
            language="text",
        )
        if command.get("description"):
            sb.info(command["description"])

        argument = sb.number_input("Argument (float)", value=0.0, step=0.1, format="%.3f")

        dry_run = sb.toggle("Dry-run only", value=False)
        if sb.button("Transmit Command", use_container_width=True):
            if dry_run:
                sb.warning(f"Dry-run: {selected_name} not sent")
            else:
                send_command(command["target_id"], command["opcode"], argument)
                sb.success(f"Sent {selected_name}({argument:.3f})")
    else:
        sb.warning("No commands match current class filter.")
else:
    sb.warning("No commands found. Regenerate dictionary: python3 tools/autocoder.py")

sb.divider()
sb.caption("Scaffold fast: python3 tools/dv-util.py quickstart-component ThermalControl --build")

# Auto-refresh every second
# Keeping this simple polling loop avoids extra dependencies.
time.sleep(1)
st.rerun()
