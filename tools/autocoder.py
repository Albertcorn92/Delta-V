#!/usr/bin/env python3
"""
DELTA-V Autocoder  v4.0
========================
Reads topology.yaml and generates three files that must never be hand-edited:

  dictionary.json           — Ground Data System packet dictionary
  src/Types.hpp             — DO-178C C++ wire-protocol packet definitions
  src/TopologyManager.hpp   — Auto-wires all C++ components and rate groups

Run after ANY edit to topology.yaml:
  python3 tools/autocoder.py

Flags:
  --dry-run   Validate topology only, write nothing
  --quiet     Suppress info output (errors still print)

New in v4.0:
  - rate_group annotations → RateGroupExecutive tier registration
  - op_class per command   → MissionFsm gating policy comments
  - params section         → TmrStore<float> member generation
  - HAL-injected classes   → ImuComponent(name, id, i2c) constructor
  - Strict validation: duplicate IDs, opcode conflicts, unknown op_class
"""
import argparse
import json
import os
import sys
from pathlib import Path

try:
    import yaml
except ImportError:
    sys.exit("[Autocoder] ERROR: pyyaml not installed. Run: pip3 install pyyaml")

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
YAML_FILE  = "topology.yaml"
DICT_FILE  = "dictionary.json"
TYPES_FILE = "src/Types.hpp"
TOPO_FILE  = "src/TopologyManager.hpp"

CCSDS_SYNC  = 0x1ACFFC1D
HAL_CLASSES = {"ImuComponent"}          # Constructors need hal::I2cBus& injected
SYSTEM_INSTS = {                        # These get special wiring treatment
    "watchdog", "telem_hub", "cmd_hub", "event_hub", "radio", "recorder"
}

# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------
def validate(topology: dict) -> list[str]:
    errors:   list[str] = []
    warnings: list[str] = []
    seen_ids:     dict[int, str]   = {}
    seen_opcodes: dict[tuple, str] = {}

    for comp in topology.get("components", []):
        cid  = comp.get("id")
        name = comp.get("name", f"id={cid}")
        if cid is None:
            errors.append(f"Component missing 'id': {comp}"); continue
        if cid in seen_ids:
            errors.append(
                f"Duplicate component ID {cid}: '{name}' vs '{seen_ids[cid]}'")
        seen_ids[cid] = name
        for field in ("name", "class", "instance"):
            if not comp.get(field):
                errors.append(f"Component id={cid} missing required field '{field}'")
        rg  = comp.get("rate_group", "norm")
        typ = comp.get("type", "Passive")
        if typ != "Active" and rg not in ("fast", "norm", "slow"):
            warnings.append(
                f"Component '{name}' rate_group='{rg}' unknown — defaulting to 'norm'")

    valid_ids = set(seen_ids.keys())
    valid_op  = {"HOUSEKEEPING", "OPERATIONAL", "RESTRICTED"}

    for cmd in topology.get("commands", []):
        cname = cmd.get("name", "?")
        tid   = cmd.get("target_id")
        op    = cmd.get("opcode")
        oc    = cmd.get("op_class", "")
        if tid not in valid_ids:
            errors.append(f"Command '{cname}' targets unknown component ID {tid}")
        key = (tid, op)
        if key in seen_opcodes:
            errors.append(
                f"Opcode {op} on target {tid} used by both "
                f"'{cname}' and '{seen_opcodes[key]}'")
        seen_opcodes[key] = cname
        if oc and oc not in valid_op:
            errors.append(
                f"Command '{cname}' has invalid op_class '{oc}'. "
                f"Valid: {sorted(valid_op)}")

    for w in warnings:
        print(f"[Autocoder] WARN: {w}", file=sys.stderr)
    return errors

# ---------------------------------------------------------------------------
# Build GDS dictionary
# ---------------------------------------------------------------------------
def build_dict(topo: dict, quiet: bool) -> None:
    sys_info = topo["system"]
    if not quiet:
        print(f"[Autocoder] Building dictionary: "
              f"{sys_info['name']} v{sys_info['version']}")

    d: dict = {
        "_meta": {
            "system":    sys_info["name"],
            "version":   sys_info["version"],
            "sync_word": hex(CCSDS_SYNC),
            "apids":     {"telemetry": 10, "event": 20, "command": 30},
        },
        "components": {},
        "commands":   {},
        "params":     {},
    }

    for c in topo.get("components", []):
        d["components"][str(c["id"])] = {
            "name":       c["name"],
            "type":       c.get("type", "Passive"),
            "rate_group": c.get("rate_group", "norm"),
            "class":      c.get("class", c["name"] + "Component"),
        }

    for cmd in topo.get("commands", []):
        d["commands"][cmd["name"]] = {
            "target_id":   cmd["target_id"],
            "opcode":      cmd["opcode"],
            "op_class":    cmd.get("op_class", "OPERATIONAL"),
            "description": cmd.get("description", ""),
            "apid":        30,
        }

    for p in topo.get("params", []):
        d["params"][p["name"]] = {
            "default":     p.get("default", 0.0),
            "tmr":         p.get("tmr", False),
            "description": p.get("description", ""),
        }

    Path(DICT_FILE).write_text(json.dumps(d, indent=4))
    if not quiet:
        print(f"[Autocoder]   → {DICT_FILE}: "
              f"{len(d['components'])} components, "
              f"{len(d['commands'])} commands, "
              f"{len(d['params'])} params")

# ---------------------------------------------------------------------------
# Build src/Types.hpp
# ---------------------------------------------------------------------------
def build_types(quiet: bool) -> None:
    if not quiet:
        print(f"[Autocoder] Generating {TYPES_FILE}")
    Path(TYPES_FILE).parent.mkdir(parents=True, exist_ok=True)
    Path(TYPES_FILE).write_text("""\
#pragma once
// ============================================================================
// Types.hpp — DELTA-V Wire Protocol & Packet Definitions
// ============================================================================
// AUTOGENERATED BY DELTA-V AUTOCODER v4.0 — DO NOT EDIT BY HAND.
// Regenerate: python3 tools/autocoder.py
// ============================================================================
#include <cstdint>
#include <cstring>
#include <array>

namespace deltav {

constexpr size_t CCSDS_HEADER_SIZE = 10;
constexpr size_t TELEMETRY_PACKET_SIZE = 12;
constexpr size_t COMMAND_PACKET_SIZE = 12;
constexpr size_t EVENT_MSG_SIZE = 28;
constexpr size_t EVENT_PACKET_SIZE = 36;

#pragma pack(push, 1)

/// CCSDS Primary Header
struct CcsdsHeader {
    uint32_t sync_word{0};   
    uint16_t apid{0};        
    uint16_t seq_count{0};   
    uint16_t payload_len{0}; 
};
static_assert(sizeof(CcsdsHeader) == CCSDS_HEADER_SIZE, "CcsdsHeader size mismatch");

struct CcsdsCrc {
    uint16_t crc16{0}; 
};

/// Telemetry packet
struct TelemetryPacket {
    uint32_t timestamp_ms{0};  
    uint32_t component_id{0};  
    float    data_payload{0.0f};  
};
static_assert(sizeof(TelemetryPacket) == TELEMETRY_PACKET_SIZE, "TelemetryPacket size mismatch");

/// Uplink command packet
struct CommandPacket {
    uint32_t target_id{0}; 
    uint32_t opcode{0};    
    float    argument{0.0f};  
};
static_assert(sizeof(CommandPacket) == COMMAND_PACKET_SIZE, "CommandPacket size mismatch");

/// Asynchronous event (telemetry + log entry)
struct EventPacket {
    uint32_t severity{0};    
    uint32_t source_id{0};   
    std::array<char, EVENT_MSG_SIZE> message{}; 

    /* NOLINTNEXTLINE(bugprone-easily-swappable-parameters) */
    static auto create(uint32_t sev, uint32_t src, const char* msg) -> EventPacket {
        EventPacket p{};
        p.severity  = sev;
        p.source_id = src;
        std::strncpy(p.message.data(), msg, EVENT_MSG_SIZE - 1);
        p.message[EVENT_MSG_SIZE - 1] = '\\0';
        return p;
    }
};
static_assert(sizeof(EventPacket) == EVENT_PACKET_SIZE, "EventPacket size mismatch");

#pragma pack(pop)

namespace Severity {
    constexpr uint32_t INFO     = 1;
    constexpr uint32_t WARNING  = 2;
    constexpr uint32_t CRITICAL = 3;
}

namespace Apid {
    constexpr uint16_t TELEMETRY = 10;
    constexpr uint16_t EVENT     = 20;
    constexpr uint16_t COMMAND   = 30;
}

constexpr uint32_t CCSDS_SYNC_WORD = 0x1ACFFC1Du;

} // namespace deltav
""")

# ---------------------------------------------------------------------------
# Build src/TopologyManager.hpp
# ---------------------------------------------------------------------------
def build_topo(topo: dict, quiet: bool) -> None:
    if not quiet:
        print(f"[Autocoder] Generating {TOPO_FILE}")

    comps    = topo.get("components", [])
    commands = topo.get("commands",   [])
    params   = topo.get("params",     [])
    custom   = topo.get("custom_wiring", [])
    instances_set = {c.get("instance", "").strip() for c in comps}
    tmr_param_names = {
        p.get("name", "").strip() for p in params if p.get("tmr", False)
    }

    includes:      list[str] = []
    instances:     list[str] = []
    reg_fast:      list[str] = []
    reg_norm:      list[str] = []
    reg_slow:      list[str] = []
    reg_active:    list[str] = []
    reg_fdir:      list[str] = []
    cmd_routes:    list[str] = []
    cmd_wiring:    list[str] = []
    telem_wiring:  list[str] = []
    event_staging: list[str] = []
    event_wiring:  list[str] = []
    verify_checks: list[str] = []
    tmr_members:   list[str] = []

    # System-level hardcoded wires
    cmd_wiring.append(
        "        radio.command_out.connect(&cmd_hub.cmd_input);")
    telem_wiring += [
        "        telem_hub.registerListener(&radio.telem_in);",
        "        telem_hub.registerListener(&recorder.telemetry_in);",
    ]
    event_wiring += [
        "        event_hub.registerListener(&radio.event_in);",
        "        event_hub.registerListener(&recorder.event_in);",
    ]

    for comp in comps:
        cls  = comp.get("class", comp["name"] + "Component")
        inst = comp.get("instance", comp["name"].lower())
        cid  = comp["id"]
        rate = comp.get("rate_group", "norm").lower()
        typ  = comp.get("type", "Passive")

        inc = f'#include "{cls}.hpp"'
        if inc not in includes:
            includes.append(inc)

        if cls in HAL_CLASSES:
            instances.append(
                f'    {cls:<22} {inst:<16}{{"{comp["name"]}", {cid}, *i2c_}};')
        else:
            instances.append(
                f'    {cls:<22} {inst:<16}{{"{comp["name"]}", {cid}}};')

        if typ == "Active":
            reg_active.append(f'        rge.registerComponent(&{inst});')
        elif rate == "fast":
            reg_fast.append(f'        rge.registerFast(&{inst});')
        elif rate == "slow":
            reg_slow.append(f'        rge.registerSlow(&{inst});')
        else:
            reg_norm.append(f'        rge.registerNorm(&{inst});')

        if inst not in {"telem_hub", "recorder"}:
            reg_fdir.append(
                f'        watchdog.registerSubsystem(&{inst});')

        if inst in SYSTEM_INSTS:
            if inst == "watchdog":
                event_staging.append("    InputPort<EventPacket> e_watchdog_{};")
                event_wiring += [
                    "        watchdog.event_out.connect(&e_watchdog_);",
                    "        event_hub.registerEventSource(&e_watchdog_);",
                ]
            elif inst == "cmd_hub":
                event_staging.append("    InputPort<EventPacket> e_cmd_hub_{};")
                event_wiring += [
                    "        cmd_hub.ack_out.connect(&e_cmd_hub_);",
                    "        event_hub.registerEventSource(&e_cmd_hub_);",
                ]
        else:
            telem_wiring.append(
                f'        telem_hub.connectInput({inst}.telemetry_out);')
            event_staging.append(f'    InputPort<EventPacket> e_{inst}_{{}};')
            event_wiring += [
                f'        {inst}.event_out.connect(&e_{inst}_);',
                f'        event_hub.registerEventSource(&e_{inst}_);',
            ]
            comp_cmds = [c for c in commands if c["target_id"] == cid]
            if comp_cmds:
                cmd_routes.append(
                    f'    OutputPort<CommandPacket> {inst}_route_{{}};')
                cmd_wiring += [
                    f'        {inst}_route_.connect(&{inst}.cmd_in);',
                    f'        cmd_hub.registerRoute({cid}, &{inst}_route_);',
                ]
            verify_checks.append(
                f'        check({inst}.telemetry_out.isConnected(), '
                f'"{inst}.telemetry_out → telem_hub");')

    verify_checks.insert(0,
        '        check(radio.command_out.isConnected(),'
        ' "radio.command_out → cmd_hub");')
    verify_checks.insert(1,
        '        check(cmd_hub.ack_out.isConnected(),'
        '   "cmd_hub.ack_out → event_hub");')
    if "cmd_hub" in instances_set and "watchdog" in instances_set:
        verify_checks.insert(2,
            '        check(cmd_hub.hasMissionStateSource(),'
            ' "cmd_hub mission state source");')
    if (
        "watchdog" in instances_set and
        {"emergency_battery_pct", "safe_mode_battery_pct", "degraded_battery_pct"}
        .issubset(tmr_param_names)
    ):
        verify_checks.insert(3,
            '        check(watchdog.hasBatteryThresholdSources(),'
            ' "watchdog battery threshold sources");')

    for p in params:
        if p.get("tmr", False):
            tmr_members.append(
                f'    TmrStore<float>  tmr_{p["name"]}{{ {p.get("default", 0.0)}f }};')

    def block(lines: list[str], comment: str = "") -> str:
        if not lines:
            return ""
        out = ""
        if comment:
            out += f"        // {comment}\n"
        out += "\n".join(lines) + "\n"
        return out

    reg_all = (
        block(reg_fast,   "── FAST tier (10 Hz) ─────────────────────────") +
        block(reg_norm,   "── NORM tier  (1 Hz) ─────────────────────────") +
        block(reg_slow,   "── SLOW tier (0.1Hz) ─────────────────────────") +
        block(reg_active, "── Active (own thread) ────────────────────────")
    )

    tmr_block = ("\n    // TMR-protected parameters (auto-generated)\n" +
                 "\n".join(tmr_members) + "\n") if tmr_members else ""

    content = f"""\
#pragma once
// ============================================================================
// TopologyManager.hpp — AUTOGENERATED BY DELTA-V AUTOCODER v4.0
// ============================================================================
// DO NOT EDIT BY HAND.  Regenerate: python3 tools/autocoder.py
// ============================================================================
#include "Port.hpp"
#include "Types.hpp"
#include "RateGroupExecutive.hpp"
#include "TmrStore.hpp"
#include "Hal.hpp"
#include <iostream>
{chr(10).join(includes)}

namespace deltav {{

struct TopologyManager {{
private:
    hal::I2cBus* i2c_{{nullptr}};

public:
    explicit TopologyManager(hal::I2cBus& i2c) : i2c_(&i2c) {{}}

    // ── Component instances ───────────────────────────────────────────────
{chr(10).join(instances)}
{tmr_block}
    // ── Lifecycle ─────────────────────────────────────────────────────────

    auto wire() -> void {{
        wireTelemetry();
        wireCommands();
        wireEvents();
        wireCustom();
        std::cout << "[Topology] All ports connected.\\n";
    }}

    auto registerAll(RateGroupExecutive& rge) -> void {{
{reg_all}
    }}

    auto registerFdir() -> void {{
{chr(10).join(reg_fdir)}
    }}

    [[nodiscard]] auto verify() -> bool {{
        bool ok = true;
        auto check = [&](bool cond, const char* label) -> void {{
            if (!cond) {{
                std::cerr << "[Topology] UNCONNECTED: " << label << "\\n";
                ok = false;
            }}
        }};
{chr(10).join(verify_checks)}
        check(event_hub.getSourceCount()   >= 2, "event_hub: need >=2 sources");
        check(event_hub.getListenerCount() >= 2, "event_hub: need >=2 listeners");
        if (!ok) std::cerr << "[Topology] FATAL: wiring incomplete.\\n";
        return ok;
    }}

private:
{chr(10).join(cmd_routes)}
{chr(10).join(event_staging)}

    auto wireTelemetry() -> void {{
{chr(10).join(telem_wiring)}
    }}

    auto wireCommands() -> void {{
{chr(10).join(cmd_wiring)}
    }}

    auto wireEvents() -> void {{
{chr(10).join(event_wiring)}
    }}

    auto wireCustom() -> void {{
{chr(10).join("        " + w for w in custom)}
    }}
}};

}} // namespace deltav
"""
    Path(TOPO_FILE).parent.mkdir(parents=True, exist_ok=True)
    Path(TOPO_FILE).write_text(content)

# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def main() -> None:
    p = argparse.ArgumentParser(description="DELTA-V Autocoder v4.0")
    p.add_argument("--dry-run", action="store_true",
                   help="Validate only — write no files")
    p.add_argument("--quiet",   action="store_true",
                   help="Suppress info output")
    args = p.parse_args()

    if not Path(YAML_FILE).exists():
        sys.exit(f"[Autocoder] FATAL: {YAML_FILE} not found.")

    with open(YAML_FILE) as f:
        topo = yaml.safe_load(f)

    if not args.quiet:
        print("[Autocoder] Validating topology...")
    errors = validate(topo)
    if errors:
        for e in errors:
            print(f"[Autocoder] ERROR: {e}", file=sys.stderr)
        sys.exit("[Autocoder] Validation FAILED — aborting.")

    if not args.quiet:
        print("[Autocoder] Validation OK.")

    if args.dry_run:
        print("[Autocoder] --dry-run: no files written.")
        return

    build_dict(topo, args.quiet)
    build_types(args.quiet)
    build_topo(topo, args.quiet)

    if not args.quiet:
        print("[Autocoder] Done.")
        print(f"[Autocoder]   {DICT_FILE}, {TYPES_FILE}, {TOPO_FILE}")
        print("[Autocoder]   Rebuild: cmake --build build")


if __name__ == "__main__":
    main()
