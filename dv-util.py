#!/usr/bin/env python3
"""
DELTA-V Utility CLI
===================
Scaffolding tool for the DELTA-V Flight Software Framework.
Generates clean, production-ready component boilerplate.

Usage:
  python3 dv-util.py add-component <ComponentName> [--active]
"""
import argparse
import os
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# C++ Component Templates
# ---------------------------------------------------------------------------
PASSIVE_TEMPLATE = """#pragma once
// =============================================================================
// {name}.hpp
// =============================================================================

#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <iostream>

namespace deltav {{

class {name} : public Component {{
public:
    // --- Ports ---
    InputPort<CommandPacket>          cmd_in;
    OutputPort<Serializer::ByteArray> telemetry_out;
    OutputPort<EventPacket>           event_out;

    {name}(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {{}}

    void init() override {{
        // Setup initial state, read from ParamDb, etc.
        std::cout << "[" << getName() << "] Initialized.\\n";
    }}

    void step() override {{
        // 1. Drain incoming commands
        CommandPacket cmd{{}};
        while (cmd_in.tryConsume(cmd)) {{
            handleCommand(cmd);
        }}

        // 2. Execute primary cyclic logic here

        // 3. Emit telemetry (Example)
        // TelemetryPacket p{{ TimeService::getMET(), getId(), 0.0f }};
        // telemetry_out.send(Serializer::pack(p));
    }}

private:
    void handleCommand(const CommandPacket& cmd) {{
        switch (cmd.opcode) {{
            // case 1:
            //     break;
            default:
                recordError(); // Unknown opcode -> FDIR visible
                event_out.send(EventPacket::create(Severity::WARNING, getId(),
                    "Unknown opcode received"));
                break;
        }}
    }}
}};

}} // namespace deltav
"""

ACTIVE_TEMPLATE = """#pragma once
// =============================================================================
// {name}.hpp (Active)
// =============================================================================

#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <iostream>

namespace deltav {{

class {name} : public ActiveComponent {{
public:
    // --- Ports ---
    InputPort<CommandPacket>          cmd_in;
    OutputPort<Serializer::ByteArray> telemetry_out;
    OutputPort<EventPacket>           event_out;

    {name}(std::string_view comp_name, uint32_t comp_id, uint32_t hz)
        : ActiveComponent(comp_name, comp_id, hz) {{}}

    void init() override {{
        // Setup initial state, network sockets, or hardware drivers
        std::cout << "[" << getName() << "] Active thread initialized.\\n";
    }}

    void step() override {{
        // This runs in an independent Os::Thread loop at target_hz.
        // Ensure all hardware I/O here uses non-blocking or timed-timeout calls.
        
        CommandPacket cmd{{}};
        while (cmd_in.tryConsume(cmd)) {{
            handleCommand(cmd);
        }}
    }}

private:
    void handleCommand(const CommandPacket& cmd) {{
        switch (cmd.opcode) {{
            default:
                recordError();
                event_out.send(EventPacket::create(Severity::WARNING, getId(),
                    "Unknown opcode received"));
                break;
        }}
    }}
}};

}} // namespace deltav
"""

def add_component(name: str, is_active: bool) -> None:
    # Ensure name ends with 'Component' for consistency, unless explicitly named otherwise
    if not name.endswith("Component"):
        name += "Component"

    src_dir = Path("src")
    if not src_dir.exists():
        print(f"[dv-util] ERROR: 'src/' directory not found. Run from project root.", file=sys.stderr)
        sys.exit(1)

    file_path = src_dir / f"{name}.hpp"
    
    if file_path.exists():
        print(f"[dv-util] ERROR: {file_path} already exists. Aborting to prevent overwrite.", file=sys.stderr)
        sys.exit(1)

    template = ACTIVE_TEMPLATE if is_active else PASSIVE_TEMPLATE
    content = template.format(name=name)

    with open(file_path, "w") as f:
        f.write(content)

    print(f"[dv-util] SUCCESS: Generated {'Active' if is_active else 'Passive'} component at {file_path}")
    print(f"[dv-util] NEXT STEPS:")
    print(f"  1. Add '{name}' to topology.yaml")
    print(f"  2. Instantiate and wire ports in src/TopologyManager.hpp")

def main():
    parser = argparse.ArgumentParser(description="DELTA-V Utility CLI")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # Command: add-component
    comp_parser = subparsers.add_parser("add-component", help="Generate a new FSW component")
    comp_parser.add_argument("name", type=str, help="Name of the component (e.g. ThermalControl)")
    comp_parser.add_argument("--active", action="store_true", help="Make this an ActiveComponent (runs on its own thread)")

    args = parser.parse_args()

    if args.command == "add-component":
        add_component(args.name, args.active)

if __name__ == "__main__":
    main()