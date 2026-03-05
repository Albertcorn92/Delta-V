import streamlit as st
import socket
import struct
import pandas as pd
import threading
import time
import os

# CONFIG
PORT = 9001
TELEM_FORMAT = "<IIf" 
EVENT_FORMAT = "<I32s"
DATA_FILE = "live_telem.csv"
EVENT_FILE = "events.log"

# BACKGROUND LISTENER
def udp_listener():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", PORT)) # Listen on all interfaces
    sock.settimeout(0.5)
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            if len(data) == 12:
                time_ms, comp_id, val = struct.unpack(TELEM_FORMAT, data)
                with open(DATA_FILE, "a") as f:
                    f.write(f"{time_ms},{comp_id},{val}\n")
            elif len(data) == 36:
                severity, msg_bytes = struct.unpack(EVENT_FORMAT, data)
                msg = msg_bytes.decode('ascii', errors='ignore').strip('\x00')
                timestamp = time.strftime("%H:%M:%S")
                with open(EVENT_FILE, "a") as f:
                    f.write(f"[{timestamp}] {msg}\n")
        except: continue

if 'thread_started' not in st.session_state:
    with open(DATA_FILE, "w") as f: f.write("Time,ID,Value\n")
    with open(EVENT_FILE, "w") as f: f.write("--- MISSION START ---\n")
    threading.Thread(target=udp_listener, daemon=True).start()
    st.session_state.thread_started = True

# DASHBOARD UI
st.set_page_config(page_title="DELTA-V GDS", layout="wide")
st.title("🛰️ DELTA-V Mission Control")

col_main, col_ctrl = st.columns([2, 1])

with col_main:
    # 1. GRAPH SECTION
    st.subheader("📈 Live Telemetry")
    chart_placeholder = st.empty()
    
    # 2. LOG SECTION
    st.subheader("📟 Event Console")
    log_placeholder = st.empty()

with col_ctrl:
    st.subheader("🎮 Vehicle Control")
    if st.button("🔋 RESET BATTERY", use_container_width=True):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(struct.pack(TELEM_FORMAT, 200, 1, 0.0), ("127.0.0.1", PORT))
    
    if st.button("✨ RESET STARTRACKER", use_container_width=True):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(struct.pack(TELEM_FORMAT, 100, 1, 0.0), ("127.0.0.1", PORT))

# THE UPDATE LOOP (The "Engine")
while True:
    # Update Chart
    try:
        df = pd.read_csv(DATA_FILE)
        if len(df) > 1:
            df_piv = df.pivot_table(index='Time', columns='ID', values='Value').tail(30)
            chart_placeholder.line_chart(df_piv)
    except: pass

    # Update Logs
    try:
        with open(EVENT_FILE, "r") as f:
            lines = f.readlines()
            log_placeholder.code("".join(lines[-10:])) # Show last 10 lines
    except: pass

    time.sleep(0.5) # Fast update