#!/usr/bin/env python3
"""
dv-util — DELTA-V Developer CLI  v4.0
======================================
The primary scaffolding and project management tool for DELTA-V.
Run from the project root directory.

Commands:
  add-component <Name> [--active] [--rate fast|norm|slow]
  add-command   <NAME> --target-id N --opcode N [--op-class ...]
  list              Print topology summary
  check             Validate topology.yaml (autocoder dry-run)

Examples:
  python3 tools/dv-util.py add-component ThermalControl
  python3 tools/dv-util.py add-component AttitudeController --active --rate fast
  python3 tools/dv-util.py add-command SET_HEATER --target-id 400 --opcode 1
  python3 tools/dv-util.py add-command ACTUATOR_ENABLE --target-id 500 --opcode 1 --op-class RESTRICTED
  python3 tools/dv-util.py list
  python3 tools/dv-util.py check
"""
import argparse
import subprocess
import sys
from pathlib import Path

try:
    import yaml
except ImportError:
    sys.exit("[dv-util] ERROR: pyyaml not installed. Run: pip3 install pyyaml")

TOPOLOGY_FILE = "topology.yaml"
RATE_HZ = {"fast": 10, "norm": 1, "slow": 0}

# ANSI colours
C = "\033[96m"; G = "\033[92m"; Y = "\033[93m"; B = "\033[1m"; N = "\033[0m"

# ────────────────────────────────────────────────────────────────────────────
# Component templates
# ────────────────────────────────────────────────────────────────────────────
_PASSIVE = """\
#pragma once
// ============================================================================
// {cls}.hpp — DELTA-V Passive Component
// Rate Group : {rate_upper} ({hz} Hz)
// ============================================================================
// REQUIREMENTS (link to src/Requirements.hpp IDs):
//   DV-ARCH-01 (DAL-C): Executes in the scheduler cyclic loop.
//   <Add DV-XXX-NN IDs for this component's specific requirements>
// ============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <iostream>

namespace deltav {{

class {cls} : public Component {{
public:
    // ── Ports ─────────────────────────────────────────────────────────────
    InputPort<CommandPacket>          cmd_in;
    OutputPort<Serializer::ByteArray> telemetry_out;
    OutputPort<EventPacket>           event_out;

    {cls}(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {{}}

    void init() override {{
        // Load parameters from ParamDb here (still single-threaded — safe).
        // Example: amplitude = ParamDb::getInstance().getParam(
        //     ParamDb::fnv1a("{short}_amplitude"), 10.0f);
        std::cout << "[" << getName() << "] Initialized.\\n";
    }}

    void step() override {{
        // ── 1. Drain commands (DO-178C: fully drain every tick) ───────────
        CommandPacket cmd{{}};
        while (cmd_in.tryConsume(cmd)) {{
            handleCommand(cmd);
        }}

        // ── 2. Cyclic logic ───────────────────────────────────────────────
        // Implement your sensor or actuator logic here.

        // ── 3. Emit telemetry ─────────────────────────────────────────────
        // TelemetryPacket p{{ TimeService::getMET(), getId(), value_ }};
        // telemetry_out.send(Serializer::pack(p));
    }}

private:
    // float value_{{0.0f}};  // example member

    void handleCommand(const CommandPacket& cmd) {{
        switch (cmd.opcode) {{
            // case 1: /* MY_COMMAND — verify DV-XXX-NN */ break;
            default:
                recordError(); // DV-FDIR-08: unknown opcode is FDIR-visible
                event_out.send(EventPacket::create(
                    Severity::WARNING, getId(), "Unknown opcode"));
                break;
        }}
    }}
}};

}} // namespace deltav
"""

_ACTIVE = """\
#pragma once
// ============================================================================
// {cls}.hpp — DELTA-V Active Component (dedicated thread)
// Rate Group : {rate_upper} ({hz} Hz, own Os::Thread)
// ============================================================================
// REQUIREMENTS:
//   DV-ARCH-02 (DAL-C): Active component runs on its own Os::Thread at hz.
//   DV-TIME-01 (DAL-A): Os::Thread uses sleep_until (absolute deadline).
//   <Add DV-XXX-NN IDs for this component's specific requirements>
// ============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <iostream>

namespace deltav {{

class {cls} : public ActiveComponent {{
public:
    // ── Ports ─────────────────────────────────────────────────────────────
    InputPort<CommandPacket>          cmd_in;
    OutputPort<Serializer::ByteArray> telemetry_out;
    OutputPort<EventPacket>           event_out;

    /// hz should match the rate_group declared in topology.yaml
    explicit {cls}(std::string_view comp_name, uint32_t comp_id,
                   uint32_t hz = {hz})
        : ActiveComponent(comp_name, comp_id, hz) {{}}

    void init() override {{
        // Runs BEFORE startThread(). Still single-threaded here.
        // Open sockets, initialise hardware, configure DMA, etc.
        std::cout << "[" << getName() << "] Active component initialized.\\n";
    }}

    void step() override {{
        // ── Called from Os::Thread at `hz` Hz via sleep_until ─────────────
        // MUST return within 1/hz seconds. Use non-blocking I/O only.

        CommandPacket cmd{{}};
        while (cmd_in.tryConsume(cmd)) {{
            handleCommand(cmd);
        }}

        // Implement your high-rate I/O or computation here.
    }}

private:
    void handleCommand(const CommandPacket& cmd) {{
        switch (cmd.opcode) {{
            default:
                recordError();
                event_out.send(EventPacket::create(
                    Severity::WARNING, getId(), "Unknown opcode"));
                break;
        }}
    }}
}};

}} // namespace deltav
"""

# ────────────────────────────────────────────────────────────────────────────
# add-component
# ────────────────────────────────────────────────────────────────────────────
def cmd_add_component(args) -> None:
    cls = args.name
    if not cls.endswith("Component"):
        cls += "Component"

    src = Path("src")
    if not src.exists():
        die("'src/' not found. Run dv-util from the project root.")

    dest = src / f"{cls}.hpp"
    if dest.exists():
        die(f"{dest} already exists. Delete it first if you want to regenerate.")

    rate  = args.rate
    hz    = RATE_HZ.get(rate, 1)
    short = cls.removesuffix("Component")
    tmpl  = _ACTIVE if args.active else _PASSIVE

    dest.write_text(tmpl.format(
        cls=cls, short=short, rate_upper=rate.upper(), hz=hz))

    print(f"{G}[dv-util] Created {dest}{N}")
    print(f"  Type: {'Active' if args.active else 'Passive'}  "
          f"Rate: {rate.upper()} ({hz} Hz)")

    instance = short.lower()
    print(f"""
{C}Next steps:{N}
  1. Add to {B}{TOPOLOGY_FILE}{N}:

     {B}- id: <UNIQUE_INTEGER>
       name: "{short}"
       class: "{cls}"
       instance: "{instance}"
       type: "{'Active' if args.active else 'Passive'}"
       rate_group: "{rate}"{N}

  2. Regenerate:
       {B}python3 tools/autocoder.py{N}

  3. Rebuild:
       {B}cmake --build build{N}
""")

# ────────────────────────────────────────────────────────────────────────────
# add-command
# ────────────────────────────────────────────────────────────────────────────
def cmd_add_command(args) -> None:
    topo_path = Path(TOPOLOGY_FILE)
    if not topo_path.exists():
        die(f"{TOPOLOGY_FILE} not found.")

    with open(topo_path) as f:
        topo = yaml.safe_load(f)

    comp_ids = {c["id"] for c in topo.get("components", [])}
    if args.target_id not in comp_ids:
        die(f"target-id {args.target_id} is not in topology components.")

    existing = {(c["target_id"], c["opcode"])
                for c in topo.get("commands", [])}
    if (args.target_id, args.opcode) in existing:
        die(f"Opcode {args.opcode} already registered for target {args.target_id}.")

    valid_oc = {"HOUSEKEEPING", "OPERATIONAL", "RESTRICTED"}
    if args.op_class not in valid_oc:
        die(f"--op-class must be one of {sorted(valid_oc)}.")

    entry = {
        "name":        args.name,
        "target_id":   args.target_id,
        "opcode":      args.opcode,
        "op_class":    args.op_class,
        "description": f"{args.name} (update description in {TOPOLOGY_FILE})",
    }
    if "commands" not in topo:
        topo["commands"] = []
    topo["commands"].append(entry)

    with open(topo_path, "w") as f:
        yaml.dump(topo, f, default_flow_style=False, sort_keys=False)

    print(f"{G}[dv-util] Added command '{args.name}' to {TOPOLOGY_FILE}{N}")
    print(f"  target={args.target_id}  opcode={args.opcode}  op_class={args.op_class}")
    print(f"\n{C}Regenerate:{N}  python3 tools/autocoder.py")

# ────────────────────────────────────────────────────────────────────────────
# list
# ────────────────────────────────────────────────────────────────────────────
def cmd_list(_args) -> None:
    topo_path = Path(TOPOLOGY_FILE)
    if not topo_path.exists():
        die(f"{TOPOLOGY_FILE} not found.")

    with open(topo_path) as f:
        topo = yaml.safe_load(f)

    s = topo.get("system", {})
    print(f"\n{B}{'═'*66}{N}")
    print(f"{B}  DELTA-V Topology:  {s.get('name','?')}  v{s.get('version','?')}{N}")
    print(f"{B}{'═'*66}{N}")

    comps = topo.get("components", [])
    print(f"\n  {C}Components ({len(comps)}):{N}")
    print(f"  {'ID':>5}  {'Name':<22}  {'Type':<8}  {'Rate':<7}  Class")
    print(f"  {'─'*5}  {'─'*22}  {'─'*8}  {'─'*7}  {'─'*26}")
    for c in comps:
        rg = (c.get("rate_group","norm").upper()
              if c.get("type","Passive") != "Active" else "ACTIVE")
        print(f"  {c['id']:>5}  {c['name']:<22}  {c.get('type','Passive'):<8}"
              f"  {rg:<7}  {c.get('class','')}")

    cmds = topo.get("commands", [])
    print(f"\n  {C}Commands ({len(cmds)}):{N}")
    print(f"  {'Name':<30}  {'Target':>6}  {'Opcode':>6}  Op-Class")
    print(f"  {'─'*30}  {'─'*6}  {'─'*6}  {'─'*16}")
    for c in cmds:
        print(f"  {c['name']:<30}  {c['target_id']:>6}  {c['opcode']:>6}"
              f"  {c.get('op_class','')}")

    params = topo.get("params", [])
    if params:
        print(f"\n  {C}Parameters ({len(params)}):{N}")
        for p in params:
            tag = f" {Y}[TMR]{N}" if p.get("tmr") else ""
            print(f"  {p['name']:<36} default={p.get('default', 0.0)}{tag}")
    print()

# ────────────────────────────────────────────────────────────────────────────
# check
# ────────────────────────────────────────────────────────────────────────────
def cmd_check(_args) -> None:
    ac = Path("tools/autocoder.py")
    if not ac.exists():
        die("tools/autocoder.py not found.")
    result = subprocess.run([sys.executable, str(ac), "--dry-run"])
    sys.exit(result.returncode)

# ────────────────────────────────────────────────────────────────────────────
# Helpers
# ────────────────────────────────────────────────────────────────────────────
def die(msg: str) -> None:
    print(f"[dv-util] ERROR: {msg}", file=sys.stderr)
    sys.exit(1)

# ────────────────────────────────────────────────────────────────────────────
# Main
# ────────────────────────────────────────────────────────────────────────────
def main() -> None:
    parser = argparse.ArgumentParser(
        prog="dv-util",
        description="DELTA-V Developer CLI v4.0",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    sub = parser.add_subparsers(dest="command", required=True)

    ac = sub.add_parser("add-component",
                        help="Scaffold a new FSW component header")
    ac.add_argument("name", help="Component name (e.g. ThermalControl)")
    ac.add_argument("--active", action="store_true",
                    help="Generate an ActiveComponent (own thread)")
    ac.add_argument("--rate", choices=["fast", "norm", "slow"], default="norm",
                    help="Rate group tier (default: norm = 1 Hz)")
    ac.set_defaults(func=cmd_add_component)

    acmd = sub.add_parser("add-command",
                          help="Add a command entry to topology.yaml")
    acmd.add_argument("name", help="Command name (e.g. SET_HEATER_TARGET)")
    acmd.add_argument("--target-id", type=int, required=True, dest="target_id")
    acmd.add_argument("--opcode",    type=int, required=True)
    acmd.add_argument("--op-class",
                      choices=["HOUSEKEEPING", "OPERATIONAL", "RESTRICTED"],
                      default="OPERATIONAL", dest="op_class",
                      help="MissionFsm gate class (default: OPERATIONAL)")
    acmd.set_defaults(func=cmd_add_command)

    ls = sub.add_parser("list", help="Print topology summary")
    ls.set_defaults(func=cmd_list)

    ck = sub.add_parser("check", help="Validate topology.yaml")
    ck.set_defaults(func=cmd_check)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
