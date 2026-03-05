import streamlit as st
import socket
import struct
import pandas as pd
import threading
import time
from datetime import datetime
import os
import json

# --- CONFIG ---
PORT_DOWNLINK = 9001  
PORT_UPLINK = 9002    

SYNC_WORD = 0xDEADBEEF
HEADER_FORMAT = "<IHHH"  # 10 Bytes: Sync(I), APID(H), Seq(H), Len(H)
TELEM_FORMAT  = "<IIf"   # 12 Bytes: MET(I), CompID(I), Val(f)
EVENT_FORMAT  = "<I32s"  # 36 Bytes: Sev(I), Msg(32s)
# FIX: Dedicated format for CommandPacket — fields are target_id, opcode, argument
# Previously TELEM_FORMAT was reused here, which swapped target_id (0) and opcode (200),
# causing every command to arrive with target_id=0 and NACK: ID 0 Not Found.
CMD_FORMAT    = "<IIf"   # 12 Bytes: TargetID(I), Opcode(I), Arg(f)

DATA_FILE = "live_telem.csv"
EVENT_FILE = "events.log"
DICT_FILE  = "dictionary.json"

# --- 1. DATA DICTIONARY ---
def load_dictionary():
    if os.path.exists(DICT_FILE):
        with open(DICT_FILE, "r") as f:
            return json.load(f)
    return {"components": {"200": "BatterySystem", "100": "StarTracker"}, "commands": {}}

GDS_DICT = load_dictionary()

def get_component_name(comp_id):
    return GDS_DICT["components"].get(str(comp_id), f"COMP_{comp_id}")

def get_command(name):
    """Look up a command from the ground dictionary by name."""
    return GDS_DICT.get("commands", {}).get(name)

# Uplink sequence counter (persistent across button presses within a session)
_uplink_seq = 0

def send_command(target_id: int, opcode: int, argument: float = 0.0):
    """Pack and transmit a CCSDS-framed CommandPacket to the flight software."""
    global _uplink_seq
    payload = struct.pack(CMD_FORMAT, target_id, opcode, argument)
    header  = struct.pack(HEADER_FORMAT, SYNC_WORD, 30, _uplink_seq & 0xFFFF, len(payload))
    _uplink_seq += 1
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(header + payload, ("127.0.0.1", PORT_UPLINK))
    sock.close()

# --- 2. THE LISTENER THREAD ---
def udp_listener():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        sock.bind(("0.0.0.0", PORT_DOWNLINK))
        sock.settimeout(1.0)
        print(f"[*] GDS Listener Active on Port {PORT_DOWNLINK}")
    except Exception as e:
        print(f"[!] Bind Error: {e}")
        return

    while True:
        try:
            data, addr = sock.recvfrom(1024)
            
            if len(data) >= 10:
                sync, apid, seq, plen = struct.unpack(HEADER_FORMAT, data[:10])
                
                if sync == SYNC_WORD:
                    timestamp = datetime.now().strftime('%H:%M:%S')
                    
                    # TELEMETRY (APID 10)
                    if apid == 10 and len(data) >= 22:
                        met, cid, val = struct.unpack(TELEM_FORMAT, data[10:22])
                        name = get_component_name(cid)
                        with open(DATA_FILE, "a") as f:
                            f.write(f"{timestamp},{name},{val}\n")
                    
                    # EVENTS (APID 20)
                    elif apid == 20 and len(data) >= 46:
                        sev, msg_b = struct.unpack(EVENT_FORMAT, data[10:46])
                        msg = msg_b.decode('ascii', errors='ignore').strip('\x00')
                        with open(EVENT_FILE, "a") as f:
                            f.write(f"[{timestamp}] {msg}\n")
                            
        except socket.timeout:
            continue
        except Exception as e:
            print(f"Listener Error: {e}")

# Reset CSV on boot
if not os.path.exists(DATA_FILE) or os.stat(DATA_FILE).st_size == 0:
    with open(DATA_FILE, "w") as f:
        f.write("Time,Component,Value\n")

# Start listener thread if not already running
if not any(t.name == "GDS_LISTENER" for t in threading.enumerate()):
    threading.Thread(target=udp_listener, daemon=True, name="GDS_LISTENER").start()

# --- 3. STREAMLIT UI ---
st.set_page_config(page_title="DELTA-V GDS", layout="wide")
st.title("🛰️ DELTA-V MISSION CONTROL")

def load_data():
    try:
        df = pd.read_csv(DATA_FILE)
        return df if not df.empty else None
    except:
        return None

df = load_data()

# --- Metrics Bar ---
c1, c2, c3, c4 = st.columns(4)
status    = "AOS (SIGNAL)" if df is not None else "LOS (NO DATA)"
batt_val  = "N/A"
last_time = "N/A"

if df is not None:
    last_time = df.iloc[-1]['Time']
    batt_only = df[df['Component'] == 'BatterySystem']
    if not batt_only.empty:
        batt_val = f"{batt_only.iloc[-1]['Value']:.1f}%"

with c1: st.metric("📡 LINK STATUS", status)
with c2: st.metric("⏱️ LAST PACKET", last_time)
with c3: st.metric("🔋 BATTERY SOC", batt_val)
with c4: st.metric("🔌 DOWNLINK PORT", PORT_DOWNLINK)

# --- Telemetry Plot ---
if df is not None:
    plot_df = df.pivot_table(index='Time', columns='Component', values='Value').tail(30)
    st.line_chart(plot_df)
else:
    st.warning("⏳ Waiting for packets... Ensure Flight Software is running.")

# --- Event Log ---
st.subheader("📋 Event Log")
if os.path.exists(EVENT_FILE):
    with open(EVENT_FILE, "r") as f:
        lines = f.readlines()
    # Show last 20 events, most recent first
    st.text("".join(reversed(lines[-20:])))
else:
    st.caption("No events yet.")

# --- Command Center ---
st.sidebar.header("🎮 UPLINK CONTROL")
st.sidebar.caption(f"Routing to flight software on UDP :{PORT_UPLINK}")

# RESET BATTERY — target_id=200, opcode=1 (from dictionary.json)
if st.sidebar.button("⚡ RESET BATTERY"):
    cmd = get_command("RESET_BATTERY")
    if cmd:
        send_command(cmd["target_id"], cmd["opcode"])
        st.sidebar.success(f"✅ RESET_BATTERY → ID {cmd['target_id']} OP {cmd['opcode']}")
    else:
        # Hardcoded fallback if dictionary is missing
        send_command(200, 1)
        st.sidebar.success("✅ RESET_BATTERY sent (hardcoded fallback)")

# SET DRAIN RATE — target_id=200, opcode=2, argument from slider
st.sidebar.divider()
drain_rate = st.sidebar.slider("Drain Rate Argument", min_value=0.0, max_value=1.0, value=0.1, step=0.01)
if st.sidebar.button("📉 SET DRAIN RATE"):
    cmd = get_command("SET_DRAIN_RATE")
    if cmd:
        send_command(cmd["target_id"], cmd["opcode"], drain_rate)
        st.sidebar.success(f"✅ SET_DRAIN_RATE → arg={drain_rate:.2f}")
    else:
        send_command(200, 2, drain_rate)
        st.sidebar.success(f"✅ SET_DRAIN_RATE sent (hardcoded fallback), arg={drain_rate:.2f}")

# Auto-refresh
time.sleep(1)
st.rerun()