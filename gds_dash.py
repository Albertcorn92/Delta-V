import streamlit as st
import socket
import struct
import pandas as pd
import threading
import time
from datetime import datetime
import os
import json

# CONFIG
PORT_DOWNLINK = 9001
PORT_UPLINK = 9002
TELEM_FORMAT = "<IIf" 
EVENT_FORMAT = "<I32s"
DATA_FILE = "live_telem.csv"
EVENT_FILE = "events.log"
DICT_FILE = "dictionary.json"

# --- 1. LOAD DATA DICTIONARY ---
def load_dictionary():
    try:
        with open(DICT_FILE, "r") as f:
            return json.load(f)
    except FileNotFoundError:
        st.error(f"CRITICAL: Ground Dictionary '{DICT_FILE}' not found! GDS cannot decode telemetry.")
        return {"components": {}, "commands": {}}

GDS_DICT = load_dictionary()

def get_component_name(comp_id):
    # Dynamically resolve ID to Name, fallback to "UNKNOWN_ID" if not in dict
    return GDS_DICT["components"].get(str(comp_id), f"UNKNOWN_{comp_id}")

# --- 2. BACKGROUND UDP LISTENER ---
def udp_listener():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    except AttributeError:
        pass
        
    sock.bind(("0.0.0.0", PORT_DOWNLINK)) 
    sock.settimeout(0.5)
    
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            
            # TELEMETRY PACKET (12 bytes)
            if len(data) == 12:
                met_clock, comp_id, val = struct.unpack(TELEM_FORMAT, data)
                
                # Dictionary Translation
                comp_name = get_component_name(comp_id)
                timestamp = datetime.now().strftime('%H:%M:%S')
                
                with open(DATA_FILE, "a") as f:
                    f.write(f"{timestamp},{comp_name},{val}\n")
                    f.flush() 

            # EVENT PACKET (36 bytes)
            elif len(data) == 36:
                severity, msg_bytes = struct.unpack(EVENT_FORMAT, data)
                msg = msg_bytes.decode('ascii', errors='ignore').strip('\x00')
                timestamp = datetime.now().strftime('%H:%M:%S')
                
                with open(EVENT_FILE, "a") as f:
                    sev_str = "INFO" if severity == 1 else "WARN" if severity == 2 else "CRIT"
                    f.write(f"[{timestamp}] [{sev_str}] {msg}\n")
                    f.flush()
        except Exception:
            continue

# Ensure files exist with headers
if not os.path.exists(DATA_FILE):
    with open(DATA_FILE, "w") as f: f.write("Time,Component,Value\n")
if not os.path.exists(EVENT_FILE):
    with open(EVENT_FILE, "w") as f: f.write("--- MISSION START ---\n")

# Global Thread Check
thread_exists = any(t.name == "DELTA_V_LISTENER" for t in threading.enumerate())
if not thread_exists:
    t = threading.Thread(target=udp_listener, daemon=True, name="DELTA_V_LISTENER")
    t.start()

# --- 3. UI CONFIGURATION ---
st.set_page_config(page_title="DELTA-V GDS", page_icon="🛰️", layout="wide")

st.markdown("""
    <style>
    .block-container { padding-top: 1rem; }
    div[data-testid="metric-container"] {
        background-color: #1e1e2e;
        border-left: 5px solid #00ff00;
        padding: 10px;
    }
    </style>
""", unsafe_allow_html=True)

st.title("🛰️ DELTA-V MISSION CONTROL")

col_kpi1, col_kpi2, col_kpi3, col_kpi4 = st.columns(4)
col_plot, col_cmd = st.columns([3, 1])
col_logs, col_raw = st.columns([3, 1])

def load_data():
    try:
        df = pd.read_csv(DATA_FILE)
        return df
    except:
        return pd.DataFrame(columns=['Time', 'Component', 'Value'])

def load_events():
    try:
        if os.path.exists(EVENT_FILE):
            with open(EVENT_FILE, "r") as f:
                return "".join(f.readlines()[-15:])
        return "Waiting..."
    except:
        return "Waiting..."

df = load_data()
events = load_events()

# Dynamic KPI Logic using Dictionary Names
batt_level = "WAIT"
sys_time = "WAIT"
if not df.empty:
    latest = df.iloc[-1]
    # No longer hardcoding '200' - looking for the dictionary name
    batt_df = df[df['Component'] == 'BatterySystem']
    if not batt_df.empty:
        batt_level = f"{batt_df.iloc[-1]['Value']:.1f}%"
    sys_time = latest['Time']

with col_kpi1: st.metric("📡 UPLINK", "NOMINAL")
with col_kpi2: st.metric("🛰️ DOWNLINK", "AOS" if not df.empty else "LOS")
with col_kpi3: st.metric("🔋 BATTERY", batt_level)
with col_kpi4: st.metric("⏱️ CLOCK", sys_time)

with col_plot:
    st.subheader("📈 Telemetry Analytics")
    if len(df) > 1:
        # Pivot so each Component gets its own labeled line
        df_piv = df.pivot_table(index='Time', columns='Component', values='Value').tail(50)
        st.line_chart(df_piv)
    else:
        st.info("Awaiting telemetry stream...")

with col_cmd:
    st.subheader("🎮 Controls")
    
    # Dynamic Command Dispatch using Dictionary
    cmd_reset = GDS_DICT["commands"].get("RESET_BATTERY", {"target_id": 200, "opcode": 1})
    if st.button("⚡ RESET BATTERY", use_container_width=True):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(struct.pack(TELEM_FORMAT, cmd_reset["target_id"], cmd_reset["opcode"], 0.0), ("127.0.0.1", PORT_UPLINK))
    
    cmd_drain = GDS_DICT["commands"].get("SET_DRAIN_RATE", {"target_id": 200, "opcode": 2})
    drain_rate = st.slider("Drain Rate", 0.1, 5.0, 0.5)
    if st.button("SET PARAM"):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(struct.pack(TELEM_FORMAT, cmd_drain["target_id"], cmd_drain["opcode"], float(drain_rate)), ("127.0.0.1", PORT_UPLINK))

with col_logs:
    st.subheader("📟 System Events")
    st.code(events, language="bash")

with col_raw:
    st.subheader("🗄️ Raw Buffer")
    st.dataframe(df.tail(5), hide_index=True)

time.sleep(1)
st.rerun()