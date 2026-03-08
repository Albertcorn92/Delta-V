#!/usr/bin/env python3
"""
dv-util - DELTA-V Developer CLI  v4.2
=====================================
The primary scaffolding and project management tool for DELTA-V.
Run from the project root directory.

Commands:
  boot-menu
  guide
  install-standard-apps
  quickstart-component <Name> [options]
  add-component <Name> [--active] [--rate fast|norm|slow] [--profile ...]
  add-command   <NAME> --target-id N --opcode N [--op-class ...] [--description ...]
  list              Print topology summary
  check             Validate topology.yaml (autocoder dry-run)

Examples:
  python3 tools/dv-util.py boot-menu
  python3 tools/dv-util.py guide
  python3 tools/dv-util.py install-standard-apps
  python3 tools/dv-util.py quickstart-component ThermalControl --build
  python3 tools/dv-util.py add-component ThermalControl --profile controller
  python3 tools/dv-util.py add-component AttitudeController --active --rate fast
  python3 tools/dv-util.py add-command SET_HEATER --target-id 400 --opcode 1
  python3 tools/dv-util.py add-command ACTUATOR_ENABLE --target-id 500 --opcode 1 \
      --op-class RESTRICTED --description "Enable actuator output"
  python3 tools/dv-util.py list
  python3 tools/dv-util.py check
"""
import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

try:
    import yaml
except ImportError:
    sys.exit("[dv-util] ERROR: pyyaml not installed. Run: pip3 install pyyaml")

TOPOLOGY_FILE = "topology.yaml"
RATE_DISPLAY_HZ = {"fast": "10", "norm": "1", "slow": "0.1"}
ACTIVE_DEFAULT_HZ = {"fast": 10, "norm": 1, "slow": 1}
PROFILES = ["sensor", "actuator", "controller", "utility"]

# ANSI colours
C = "\033[96m"
G = "\033[92m"
Y = "\033[93m"
B = "\033[1m"
N = "\033[0m"

PROFILE_NOTES = {
    "sensor": "// Profile hint: sample inputs, validate ranges, publish calibrated telemetry.",
    "actuator": "// Profile hint: clamp commands to safe limits and emit state after each actuation.",
    "controller": "// Profile hint: consume sensor feeds, run control law, and command actuators deterministically.",
    "utility": "// Profile hint: implement utility logic (logging, health checks, housekeeping, etc.).",
}

STANDARD_COMPONENTS = [
    {
        "id": 40,
        "name": "CmdSequencer",
        "class": "CommandSequencerComponent",
        "instance": "cmd_sequencer",
        "type": "Passive",
        "rate_group": "norm",
    },
    {
        "id": 41,
        "name": "FileTransfer",
        "class": "FileTransferComponent",
        "instance": "file_transfer",
        "type": "Passive",
        "rate_group": "norm",
    },
    {
        "id": 42,
        "name": "MemoryDwell",
        "class": "MemoryDwellComponent",
        "instance": "memory_dwell",
        "type": "Passive",
        "rate_group": "norm",
    },
    {
        "id": 43,
        "name": "TimeSync",
        "class": "TimeSyncComponent",
        "instance": "time_sync",
        "type": "Passive",
        "rate_group": "norm",
    },
    {
        "id": 44,
        "name": "Playback",
        "class": "PlaybackComponent",
        "instance": "playback",
        "type": "Passive",
        "rate_group": "norm",
    },
    {
        "id": 45,
        "name": "OtaManager",
        "class": "OtaComponent",
        "instance": "ota_manager",
        "type": "Passive",
        "rate_group": "norm",
    },
    {
        "id": 46,
        "name": "AtsRtsSequencer",
        "class": "AtsRtsSequencerComponent",
        "instance": "ats_rts",
        "type": "Passive",
        "rate_group": "norm",
    },
    {
        "id": 47,
        "name": "LimitChecker",
        "class": "LimitCheckerComponent",
        "instance": "limit_checker",
        "type": "Passive",
        "rate_group": "norm",
    },
    {
        "id": 48,
        "name": "CfdpManager",
        "class": "CfdpComponent",
        "instance": "cfdp_manager",
        "type": "Passive",
        "rate_group": "norm",
    },
    {
        "id": 49,
        "name": "ModeManager",
        "class": "ModeManagerComponent",
        "instance": "mode_manager",
        "type": "Passive",
        "rate_group": "norm",
    },
]

STANDARD_COMMANDS = [
    ("SEQ_SET_TARGET", 40, 1, "HOUSEKEEPING", "Sequencer stage: set target component ID"),
    ("SEQ_SET_OPCODE", 40, 2, "HOUSEKEEPING", "Sequencer stage: set opcode"),
    ("SEQ_SET_ARGUMENT", 40, 3, "HOUSEKEEPING", "Sequencer stage: set argument"),
    ("SEQ_COMMIT_DELAY", 40, 4, "HOUSEKEEPING", "Sequencer commit: schedule staged command after arg seconds"),
    ("SEQ_CLEAR_QUEUE", 40, 5, "HOUSEKEEPING", "Sequencer: clear queued commands"),
    ("FILE_BEGIN_SESSION", 41, 1, "HOUSEKEEPING", "File transfer: begin session with expected bytes (arg)"),
    ("FILE_PUSH_TEST_BYTE", 41, 2, "HOUSEKEEPING", "File transfer: push one test byte from arg"),
    ("FILE_FINALIZE_SESSION", 41, 3, "HOUSEKEEPING", "File transfer: finalize session"),
    ("FILE_RESET_SESSION", 41, 4, "HOUSEKEEPING", "File transfer: reset session state"),
    ("DWELL_SELECT_CHANNEL", 42, 1, "HOUSEKEEPING", "Dwell: select diagnostic channel"),
    ("DWELL_SET_PERIOD", 42, 2, "HOUSEKEEPING", "Dwell: set period seconds"),
    ("DWELL_SAMPLE_NOW", 42, 3, "HOUSEKEEPING", "Dwell: force immediate sample"),
    ("DWELL_SET_ADDR_HI16", 42, 10, "HOUSEKEEPING", "Dwell/Patch: set address upper 16 bits"),
    ("DWELL_SET_ADDR_LO16", 42, 11, "HOUSEKEEPING", "Dwell/Patch: set address lower 16 bits"),
    ("DWELL_READ_ADDRESS", 42, 12, "HOUSEKEEPING", "Dwell: read staged address word"),
    ("DWELL_SET_VALUE_HI16", 42, 13, "HOUSEKEEPING", "Patch: set value upper 16 bits"),
    ("DWELL_SET_VALUE_LO16", 42, 14, "HOUSEKEEPING", "Patch: set value lower 16 bits"),
    ("DWELL_PATCH_ADDRESS", 42, 15, "HOUSEKEEPING", "Patch: write staged value to staged address"),
    ("TIME_SET_UTC_HI16", 43, 1, "HOUSEKEEPING", "Time sync: set UTC word 0 (hi16)"),
    ("TIME_SET_UTC_MIDHI16", 43, 2, "HOUSEKEEPING", "Time sync: set UTC word 1"),
    ("TIME_SET_UTC_MIDLO16", 43, 3, "HOUSEKEEPING", "Time sync: set UTC word 2"),
    ("TIME_SET_UTC_LO16", 43, 4, "HOUSEKEEPING", "Time sync: set UTC word 3 (lo16)"),
    ("TIME_APPLY_SYNC", 43, 5, "HOUSEKEEPING", "Time sync: apply staged UTC words"),
    ("TIME_PUBLISH_STATUS", 43, 6, "HOUSEKEEPING", "Time sync: publish sync status"),
    ("PLAYBACK_LOAD_LOG", 44, 1, "HOUSEKEEPING", "Playback: load historical telemetry log"),
    ("PLAYBACK_START", 44, 2, "HOUSEKEEPING", "Playback: start continuous replay"),
    ("PLAYBACK_STOP", 44, 3, "HOUSEKEEPING", "Playback: stop replay"),
    ("PLAYBACK_REWIND", 44, 4, "HOUSEKEEPING", "Playback: rewind replay cursor"),
    ("PLAYBACK_STEP_ONCE", 44, 5, "HOUSEKEEPING", "Playback: send one historical sample"),
    ("PLAYBACK_SET_RATE_HZ", 44, 6, "HOUSEKEEPING", "Playback: set replay output rate in Hz"),
    ("OTA_BEGIN_SESSION", 45, 1, "HOUSEKEEPING", "OTA: begin session with expected byte count"),
    ("OTA_PUSH_TEST_BYTE", 45, 2, "HOUSEKEEPING", "OTA: append one test byte"),
    ("OTA_SET_CRC_HI16", 45, 3, "HOUSEKEEPING", "OTA: set expected CRC upper 16 bits"),
    ("OTA_SET_CRC_LO16", 45, 4, "HOUSEKEEPING", "OTA: set expected CRC lower 16 bits"),
    ("OTA_VERIFY_IMAGE", 45, 5, "HOUSEKEEPING", "OTA: verify received image CRC"),
    ("OTA_STAGE_ACTIVATE", 45, 6, "HOUSEKEEPING", "OTA: stage update and request reboot"),
    ("OTA_RESET_SESSION", 45, 7, "HOUSEKEEPING", "OTA: reset session state"),
    ("ATS_SET_TARGET", 46, 1, "HOUSEKEEPING", "ATS/RTS stage: set target component ID"),
    ("ATS_SET_OPCODE", 46, 2, "HOUSEKEEPING", "ATS/RTS stage: set opcode"),
    ("ATS_SET_ARGUMENT", 46, 3, "HOUSEKEEPING", "ATS/RTS stage: set command argument"),
    ("ATS_SET_TRIGGER_TYPE", 46, 4, "HOUSEKEEPING", "ATS/RTS stage: trigger type (0=MET,1=UTC,2=event-delay)"),
    ("ATS_SET_TIME_HI16", 46, 5, "HOUSEKEEPING", "ATS/RTS stage: time word hi16"),
    ("ATS_SET_TIME_LO16", 46, 6, "HOUSEKEEPING", "ATS/RTS stage: time word lo16"),
    ("ATS_SET_EVENT_SOURCE", 46, 7, "HOUSEKEEPING", "ATS/RTS stage: event source component ID"),
    ("ATS_COMMIT_ENTRY", 46, 8, "HOUSEKEEPING", "ATS/RTS: commit staged entry"),
    ("ATS_ARM", 46, 9, "HOUSEKEEPING", "ATS/RTS: arm execution"),
    ("ATS_DISARM", 46, 10, "HOUSEKEEPING", "ATS/RTS: disarm execution"),
    ("ATS_CLEAR", 46, 11, "HOUSEKEEPING", "ATS/RTS: clear all entries"),
    ("LIM_SET_TARGET", 47, 1, "HOUSEKEEPING", "Limit checker stage: target component ID"),
    ("LIM_SET_WARN_LOW", 47, 2, "HOUSEKEEPING", "Limit checker stage: warning low threshold"),
    ("LIM_SET_WARN_HIGH", 47, 3, "HOUSEKEEPING", "Limit checker stage: warning high threshold"),
    ("LIM_SET_CRIT_LOW", 47, 4, "HOUSEKEEPING", "Limit checker stage: critical low threshold"),
    ("LIM_SET_CRIT_HIGH", 47, 5, "HOUSEKEEPING", "Limit checker stage: critical high threshold"),
    ("LIM_APPLY", 47, 6, "HOUSEKEEPING", "Limit checker: apply staged thresholds"),
    ("LIM_CLEAR_TARGET", 47, 7, "HOUSEKEEPING", "Limit checker: clear staged target thresholds"),
    ("LIM_ENABLE", 47, 8, "HOUSEKEEPING", "Limit checker: enable evaluation"),
    ("LIM_DISABLE", 47, 9, "HOUSEKEEPING", "Limit checker: disable evaluation"),
    ("CFDP_BEGIN_SESSION", 48, 1, "HOUSEKEEPING", "CFDP: begin session with expected chunk count"),
    ("CFDP_SET_CHUNK_INDEX", 48, 2, "HOUSEKEEPING", "CFDP: stage chunk index"),
    ("CFDP_PUSH_TEST_BYTE", 48, 3, "HOUSEKEEPING", "CFDP: append one staged test byte"),
    ("CFDP_COMMIT_CHUNK", 48, 4, "HOUSEKEEPING", "CFDP: commit staged chunk data"),
    ("CFDP_REPORT_MISSING_COUNT", 48, 5, "HOUSEKEEPING", "CFDP: report number of missing chunks"),
    ("CFDP_REPORT_NEXT_MISSING", 48, 6, "HOUSEKEEPING", "CFDP: report next missing chunk index"),
    ("CFDP_COMPLETE_SESSION", 48, 7, "HOUSEKEEPING", "CFDP: mark session complete"),
    ("CFDP_RESET_SESSION", 48, 8, "HOUSEKEEPING", "CFDP: reset session state"),
    ("MODE_STAGE_TARGET", 49, 1, "HOUSEKEEPING", "Mode manager stage: target component ID"),
    ("MODE_STAGE_ENABLE_OPCODE", 49, 2, "HOUSEKEEPING", "Mode manager stage: enable opcode"),
    ("MODE_STAGE_ENABLE_ARG", 49, 3, "HOUSEKEEPING", "Mode manager stage: enable argument"),
    ("MODE_STAGE_DISABLE_OPCODE", 49, 4, "HOUSEKEEPING", "Mode manager stage: disable opcode"),
    ("MODE_STAGE_DISABLE_ARG", 49, 5, "HOUSEKEEPING", "Mode manager stage: disable argument"),
    ("MODE_STAGE_MASK", 49, 6, "HOUSEKEEPING", "Mode manager stage: mode mask bitfield"),
    ("MODE_COMMIT_RULE", 49, 7, "HOUSEKEEPING", "Mode manager: commit staged rule"),
    ("MODE_CLEAR_RULES", 49, 8, "HOUSEKEEPING", "Mode manager: clear all rules"),
    ("MODE_SET_MODE", 49, 9, "HOUSEKEEPING", "Mode manager: set mode (1=DETUMBLE,2=SUN,3=SCIENCE,4=DOWNLINK)"),
    ("MODE_APPLY", 49, 10, "HOUSEKEEPING", "Mode manager: apply current mode to all rules"),
    ("MODE_REPORT_STATUS", 49, 11, "HOUSEKEEPING", "Mode manager: publish transition and dispatch counters"),
]

# ---------------------------------------------------------------------------
# Component templates
# ---------------------------------------------------------------------------
_PASSIVE = """\
#pragma once
// ============================================================================
// {cls}.hpp - DELTA-V Passive Component
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
    // -- Ports -------------------------------------------------------------
    InputPort<CommandPacket>          cmd_in;
    OutputPort<Serializer::ByteArray> telemetry_out;
    OutputPort<EventPacket>           event_out;

    {cls}(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {{}}

    void init() override {{
        // Load parameters from ParamDb here (still single-threaded - safe).
        // Example: amplitude = ParamDb::getInstance().getParam(
        //     ParamDb::fnv1a("{short}_amplitude"), 10.0f);
        std::cout << "[" << getName() << "] Initialized.\\n";
    }}

    void step() override {{
        // -- 1. Drain commands (DO-178C: fully drain every tick) -----------
        CommandPacket cmd{{}};
        while (cmd_in.tryConsume(cmd)) {{
            handleCommand(cmd);
        }}

        // -- 2. Cyclic logic ------------------------------------------------
        {profile_note}

        // -- 3. Emit telemetry ---------------------------------------------
        // TelemetryPacket p{{ TimeService::getMET(), getId(), value_ }};
        // telemetry_out.send(Serializer::pack(p));
    }}

private:
    // float value_{{0.0f}};  // example member

    void handleCommand(const CommandPacket& cmd) {{
        switch (cmd.opcode) {{
            // case 1: /* MY_COMMAND - verify DV-XXX-NN */ break;
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
// {cls}.hpp - DELTA-V Active Component (dedicated thread)
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
    // -- Ports -------------------------------------------------------------
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
        // -- Called from Os::Thread at `hz` Hz via sleep_until -------------
        // MUST return within 1/hz seconds. Use non-blocking I/O only.

        CommandPacket cmd{{}};
        while (cmd_in.tryConsume(cmd)) {{
            handleCommand(cmd);
        }}

        {profile_note}
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


def die(msg: str) -> None:
    print(f"[dv-util] ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def validate_cpp_identifier(value: str) -> bool:
    return bool(re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", value))


def normalize_component_class(name: str) -> str:
    cls = name.strip()
    if not cls:
        die("component name cannot be empty")
    if not validate_cpp_identifier(cls):
        die("component name must be a valid C++ identifier")
    if not cls.endswith("Component"):
        cls += "Component"
    return cls


def short_name_from_class(cls: str) -> str:
    return cls.removesuffix("Component")


def instance_name_from_short(short: str) -> str:
    first_pass = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", short)
    second_pass = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", first_pass)
    return second_pass.lower().replace("__", "_")


def upper_snake_from_name(value: str) -> str:
    first_pass = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", value)
    second_pass = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", first_pass)
    return second_pass.upper().replace("-", "_").replace("__", "_")


def load_topology() -> dict:
    topo_path = Path(TOPOLOGY_FILE)
    if not topo_path.exists():
        die(f"{TOPOLOGY_FILE} not found.")
    with open(topo_path) as f:
        topo = yaml.safe_load(f) or {}
    topo.setdefault("components", [])
    topo.setdefault("commands", [])
    return topo


def save_topology(topo: dict) -> None:
    with open(TOPOLOGY_FILE, "w") as f:
        yaml.dump(topo, f, default_flow_style=False, sort_keys=False)


def next_component_id(topo: dict) -> int:
    ids = {int(c.get("id")) for c in topo.get("components", []) if isinstance(c.get("id"), int)}
    candidate = (max(ids) + 1) if ids else 100
    while candidate in ids:
        candidate += 1
    return candidate


def next_opcode_for_target(topo: dict, target_id: int) -> int:
    opcodes = [
        int(c.get("opcode"))
        for c in topo.get("commands", [])
        if c.get("target_id") == target_id and isinstance(c.get("opcode"), int)
    ]
    return (max(opcodes) + 1) if opcodes else 1


def component_entry_error(topo: dict, entry: dict) -> str | None:
    component_ids = {c.get("id") for c in topo.get("components", [])}
    if entry["id"] in component_ids:
        return f"component id {entry['id']} already exists in {TOPOLOGY_FILE}"

    classes = {c.get("class") for c in topo.get("components", [])}
    if entry["class"] in classes:
        return f"component class {entry['class']} already exists in {TOPOLOGY_FILE}"

    instances = {c.get("instance") for c in topo.get("components", [])}
    if entry["instance"] in instances:
        return f"component instance {entry['instance']} already exists in {TOPOLOGY_FILE}"
    return None


def add_component_entry(topo: dict, entry: dict) -> None:
    error = component_entry_error(topo, entry)
    if error:
        die(error)
    topo["components"].append(entry)


def command_entry_error(
    topo: dict,
    name: str,
    target_id: int,
    opcode: int,
    op_class: str,
) -> str | None:
    valid_op_class = {"HOUSEKEEPING", "OPERATIONAL", "RESTRICTED"}
    if op_class not in valid_op_class:
        return f"--op-class must be one of {sorted(valid_op_class)}"

    comp_ids = {c["id"] for c in topo.get("components", [])}
    if target_id not in comp_ids:
        return f"target-id {target_id} is not in topology components"

    existing = {(c["target_id"], c["opcode"]) for c in topo.get("commands", [])}
    if (target_id, opcode) in existing:
        return f"opcode {opcode} already registered for target {target_id}"
    return None


def add_command_entry(
    topo: dict,
    name: str,
    target_id: int,
    opcode: int,
    op_class: str,
    description: str,
) -> None:
    error = command_entry_error(topo, name, target_id, opcode, op_class)
    if error:
        die(error)

    topo["commands"].append(
        {
            "name": name,
            "target_id": target_id,
            "opcode": opcode,
            "op_class": op_class,
            "description": description.strip() or f"{name} command",
        }
    )


def ensure_custom_wiring_line(topo: dict, line: str) -> bool:
    wiring = topo.setdefault("custom_wiring", [])
    if line in wiring:
        return False
    wiring.append(line)
    return True


def install_standard_apps(topo: dict) -> tuple[bool, list[str]]:
    changed = False
    notes: list[str] = []

    component_classes = {c.get("class"): c for c in topo.get("components", [])}
    component_ids = {c.get("id"): c for c in topo.get("components", [])}
    component_instances = {c.get("instance") for c in topo.get("components", [])}

    for comp in STANDARD_COMPONENTS:
        existing_by_class = component_classes.get(comp["class"])
        if existing_by_class is not None:
            notes.append(f"component {comp['class']} already present")
            continue

        requested_id = comp["id"]
        entry = dict(comp)
        if requested_id in component_ids:
            entry["id"] = next_component_id(topo)
            notes.append(
                f"component id {requested_id} in use, assigned {entry['id']} for {entry['class']}"
            )
        while entry["instance"] in component_instances:
            entry["instance"] = f"{entry['instance']}_{entry['id']}"

        add_component_entry(topo, entry)
        changed = True
        component_classes[entry["class"]] = entry
        component_ids[entry["id"]] = entry
        component_instances.add(entry["instance"])
        notes.append(f"added component {entry['class']} (id={entry['id']})")

    class_to_id = {
        c.get("class"): c.get("id")
        for c in topo.get("components", [])
        if isinstance(c.get("id"), int)
    }
    default_target_to_class = {
        40: "CommandSequencerComponent",
        41: "FileTransferComponent",
        42: "MemoryDwellComponent",
        43: "TimeSyncComponent",
        44: "PlaybackComponent",
        45: "OtaComponent",
        46: "AtsRtsSequencerComponent",
        47: "LimitCheckerComponent",
        48: "CfdpComponent",
        49: "ModeManagerComponent",
    }

    existing_cmd_names = {cmd.get("name") for cmd in topo.get("commands", [])}
    existing_pairs = {(cmd.get("target_id"), cmd.get("opcode")) for cmd in topo.get("commands", [])}
    for name, default_target_id, default_opcode, op_class, description in STANDARD_COMMANDS:
        if name in existing_cmd_names:
            notes.append(f"command {name} already present")
            continue

        target_id = default_target_id
        mapped_class = default_target_to_class.get(default_target_id)
        if mapped_class and mapped_class in class_to_id:
            target_id = class_to_id[mapped_class]

        opcode = default_opcode
        while (target_id, opcode) in existing_pairs:
            opcode += 1

        add_command_entry(topo, name, target_id, opcode, op_class, description)
        changed = True
        existing_cmd_names.add(name)
        existing_pairs.add((target_id, opcode))
        notes.append(f"added command {name} (target={target_id}, opcode={opcode})")

    instance_by_class = {
        c.get("class"): c.get("instance")
        for c in topo.get("components", [])
        if c.get("class") and c.get("instance")
    }

    wiring_specs = [
        ("CommandSequencerComponent", "cmd_hub.registerHousekeepingTarget({instance}.getId());", "housekeeping target"),
        ("FileTransferComponent", "cmd_hub.registerHousekeepingTarget({instance}.getId());", "housekeeping target"),
        ("MemoryDwellComponent", "cmd_hub.registerHousekeepingTarget({instance}.getId());", "housekeeping target"),
        ("TimeSyncComponent", "cmd_hub.registerHousekeepingTarget({instance}.getId());", "housekeeping target"),
        ("PlaybackComponent", "cmd_hub.registerHousekeepingTarget({instance}.getId());", "housekeeping target"),
        ("OtaComponent", "cmd_hub.registerHousekeepingTarget({instance}.getId());", "housekeeping target"),
        ("CommandSequencerComponent", "{instance}.command_out.connect(&cmd_hub.cmd_input);", "cmd_hub command ingress"),
        ("AtsRtsSequencerComponent", "{instance}.command_out.connect(&cmd_hub.cmd_input);", "cmd_hub command ingress"),
        ("AtsRtsSequencerComponent", "cmd_hub.registerHousekeepingTarget({instance}.getId());", "housekeeping target"),
        ("LimitCheckerComponent", "cmd_hub.registerHousekeepingTarget({instance}.getId());", "housekeeping target"),
        ("CfdpComponent", "cmd_hub.registerHousekeepingTarget({instance}.getId());", "housekeeping target"),
        ("ModeManagerComponent", "{instance}.command_out.connect(&cmd_hub.cmd_input);", "cmd_hub command ingress"),
        ("ModeManagerComponent", "cmd_hub.registerHousekeepingTarget({instance}.getId());", "housekeeping target"),
        ("AtsRtsSequencerComponent", "event_hub.registerListener(&{instance}.event_in);", "ats/rts event listener"),
        ("LimitCheckerComponent", "telem_hub.registerListener(&{instance}.telem_in);", "limit checker telemetry tap"),
    ]
    for cls_name, template, label in wiring_specs:
        instance = instance_by_class.get(cls_name)
        if not instance:
            continue
        line = template.format(instance=instance)
        if ensure_custom_wiring_line(topo, line):
            changed = True
            notes.append(f"added custom wiring: {label}")

    return changed, notes


def create_component_header(
    name: str,
    active: bool,
    rate: str,
    profile: str,
) -> tuple[Path, str, str, str]:
    cls = normalize_component_class(name)
    src = Path("src")
    if not src.exists():
        die("'src/' not found. Run dv-util from the project root.")

    dest = src / f"{cls}.hpp"
    if dest.exists():
        die(f"{dest} already exists. Delete it first if you want to regenerate.")

    passive_hz = RATE_DISPLAY_HZ.get(rate, "1")
    active_hz = ACTIVE_DEFAULT_HZ.get(rate, 1)
    hz = active_hz if active else passive_hz
    hz_display = str(active_hz if active else passive_hz)
    short = short_name_from_class(cls)
    profile_note = PROFILE_NOTES.get(profile, PROFILE_NOTES["utility"])
    tmpl = _ACTIVE if active else _PASSIVE
    dest.write_text(
        tmpl.format(
            cls=cls,
            short=short,
            rate_upper=rate.upper(),
            hz=hz,
            profile_note=profile_note,
        )
    )
    return dest, cls, hz_display, short


# ---------------------------------------------------------------------------
# Interactive helpers
# ---------------------------------------------------------------------------
def prompt_text(prompt: str, default: str | None = None, allow_empty: bool = False) -> str:
    while True:
        suffix = f" [{default}]" if default is not None else ""
        value = input(f"{prompt}{suffix}: ").strip()
        if not value and default is not None:
            value = default
        if value or allow_empty:
            return value
        print(f"{Y}Input required.{N}")


def prompt_int(prompt: str, default: int | None = None) -> int:
    while True:
        text = prompt_text(prompt, str(default) if default is not None else None)
        try:
            return int(text)
        except ValueError:
            print(f"{Y}Please enter an integer.{N}")


def prompt_choice(prompt: str, options: list[tuple[str, str]], default_index: int = 1) -> str:
    for idx, (_key, label) in enumerate(options, start=1):
        print(f"  {idx}. {label}")
    while True:
        raw = prompt_text(prompt, str(default_index))
        if raw.isdigit():
            idx = int(raw)
            if 1 <= idx <= len(options):
                return options[idx - 1][0]
        print(f"{Y}Choose a valid menu number.{N}")


def prompt_yes_no(prompt: str, default_yes: bool = True) -> bool:
    default = "Y/n" if default_yes else "y/N"
    raw = input(f"{prompt} [{default}]: ").strip().lower()
    if not raw:
        return default_yes
    return raw in {"y", "yes"}


def prompt_identifier(prompt: str, default: str | None = None, uppercase: bool = False) -> str:
    while True:
        value = prompt_text(prompt, default)
        if uppercase:
            value = value.upper().replace("-", "_")
        if validate_cpp_identifier(value):
            return value
        print(f"{Y}Use letters/digits/underscore only, and do not start with a digit.{N}")


def run_subprocess_step(argv: list[str], label: str) -> int:
    pretty = " ".join(argv)
    print(f"{C}[dv-util] Running {label}: {pretty}{N}")
    try:
        result = subprocess.run(argv)
    except FileNotFoundError:
        print(f"{Y}[dv-util] Could not run {label}: command not found.{N}")
        return 127
    if result.returncode == 0:
        print(f"{G}[dv-util] {label} complete.{N}")
    else:
        print(f"{Y}[dv-util] {label} failed (exit {result.returncode}).{N}")
    return result.returncode


def print_guide() -> None:
    print(
        f"""
{B}DELTA-V First-Run Guide (Plug-and-Play){N}

{C}Goal:{N}
  Add your own component, auto-wire it into topology and dictionary, build,
  and view it in GDS with minimal manual edits.

{C}Recommended route:{N}
  1) python3 tools/dv-util.py boot-menu
  2) Choose: Install standard civilian ops apps (one time)
  3) Choose: Quickstart: component + command + regenerate
  4) Accept defaults unless you need custom IDs/names
  5) Say YES to:
     - Run autocoder now
     - Run topology dry-run check now
     - Build flight_software now

{C}What each stage does:{N}
  - Create component: writes src/<YourComponent>.hpp scaffold
  - Register component: updates topology.yaml
  - Create command: updates topology.yaml command table
  - Autocoder: regenerates src/Types.hpp, src/TopologyManager.hpp, dictionary.json
  - Dry-run/build: validates topology and compiles flight_software

{C}Command classes (MissionFsm):{N}
  HOUSEKEEPING -> allowed in all mission states
  OPERATIONAL  -> blocked in SAFE_MODE and EMERGENCY
  RESTRICTED   -> NOMINAL only

{C}How to view your new component in GDS:{N}
  Terminal 1: ./build/flight_software
  Terminal 2: streamlit run gds/gds_dash.py

{C}Legit framework workflow after changes:{N}
  - cmake --build build --target run_tests
  - cmake --build build --target run_system_tests
  - cmake --build build --target quickstart_10min
"""
    )


def verify_dictionary_entries(component_name: str | None = None, command_name: str | None = None) -> None:
    path = Path("dictionary.json")
    if not path.exists():
        print(f"{Y}[dv-util] dictionary.json not found after autocoder run.{N}")
        return
    try:
        payload = json.loads(path.read_text())
    except Exception:
        print(f"{Y}[dv-util] Could not parse dictionary.json.{N}")
        return

    if component_name:
        components = payload.get("components", {})
        found = any(
            isinstance(v, dict) and v.get("name") == component_name
            for v in components.values()
        )
        msg = "visible" if found else "not found"
        color = G if found else Y
        print(f"{color}[dv-util] GDS component check: '{component_name}' {msg} in dictionary.json{N}")

    if command_name:
        commands = payload.get("commands", {})
        found = command_name in commands
        msg = "visible" if found else "not found"
        color = G if found else Y
        print(f"{color}[dv-util] GDS command check: '{command_name}' {msg} in dictionary.json{N}")


def post_creation_pipeline(
    changed_topology: bool,
    component_name: str | None = None,
    command_name: str | None = None,
) -> None:
    if not changed_topology:
        print(f"{Y}[dv-util] No topology updates to regenerate.{N}")
        return

    if not prompt_yes_no("Run autocoder now (updates generated code + dictionary)?", default_yes=True):
        print(f"{Y}[dv-util] Skipped autocoder. Run manually: python3 tools/autocoder.py{N}")
        return

    rc = run_subprocess_step([sys.executable, "tools/autocoder.py"], "autocoder")
    if rc != 0:
        return

    verify_dictionary_entries(component_name=component_name, command_name=command_name)

    if prompt_yes_no("Run topology dry-run check now?", default_yes=True):
        run_subprocess_step([sys.executable, "tools/autocoder.py", "--dry-run"], "topology dry-run")

    if prompt_yes_no("Build flight_software now?", default_yes=False):
        run_subprocess_step(["cmake", "--build", "build", "--target", "flight_software"], "flight_software build")

    print(
        f"""\n{C}To see it live in GDS:{N}
  1. ./build/flight_software
  2. streamlit run gds/gds_dash.py
"""
    )


def choose_target_component(topo: dict, default_target_id: int | None = None) -> int:
    components = topo.get("components", [])
    if not components:
        die("no components found in topology; add one first")

    print(f"\n{C}Available targets:{N}")
    for comp in components:
        comp_type = comp.get("type", "Passive")
        print(f"  {comp['id']:>4}  {comp['name']:<22} ({comp_type})")

    valid_ids = {c["id"] for c in components}
    default_id = default_target_id if default_target_id in valid_ids else min(valid_ids)
    while True:
        target_id = prompt_int("Target component id", default=default_id)
        if target_id in valid_ids:
            return target_id
        print(f"{Y}ID {target_id} is not in topology components.{N}")


def run_component_wizard(
    force_register: bool = False,
    force_create_command: bool = False,
) -> tuple[bool, str | None, str | None]:
    print(f"\n{B}Create Component Wizard{N}")
    raw_name = prompt_identifier("Component class/name (e.g. ThermalControl)")

    active_choice = prompt_choice(
        "Component type",
        [("passive", "Passive (scheduler rate group)"), ("active", "Active (dedicated thread)")],
        default_index=1,
    )
    is_active = active_choice == "active"

    rate = prompt_choice(
        "Rate group",
        [("fast", "FAST (10 Hz)"), ("norm", "NORM (1 Hz)"), ("slow", "SLOW (0.1 Hz)")],
        default_index=2,
    )

    profile = prompt_choice(
        "Profile",
        [
            ("sensor", "Sensor"),
            ("actuator", "Actuator"),
            ("controller", "Controller"),
            ("utility", "Utility"),
        ],
        default_index=4,
    )

    cls_preview = normalize_component_class(raw_name)
    dest_preview = Path("src") / f"{cls_preview}.hpp"
    while dest_preview.exists():
        print(f"{Y}{dest_preview} already exists. Pick a different component name.{N}")
        raw_name = prompt_identifier("Component class/name", raw_name)
        cls_preview = normalize_component_class(raw_name)
        dest_preview = Path("src") / f"{cls_preview}.hpp"

    print(f"\n{C}Review:{N}")
    print(f"  Class: {cls_preview}")
    print(f"  Type: {'Active' if is_active else 'Passive'}")
    print(f"  Rate: {rate.upper()}")
    print(f"  Profile: {profile}")
    if not prompt_yes_no("Create component header now?", default_yes=True):
        print(f"{Y}[dv-util] Component creation cancelled.{N}")
        return False, None, None

    dest, cls, hz, short = create_component_header(raw_name, is_active, rate, profile)
    print(f"{G}[dv-util] Created {dest}{N}")
    print(f"  Type: {'Active' if is_active else 'Passive'}  Rate: {rate.upper()} ({hz} Hz)  Profile: {profile}")
    if is_active and rate == "slow":
        print(f"{Y}  Note: Active defaults are integer-Hz; tune constructor hz manually for sub-Hz loops.{N}")

    registered_id = None
    registered_name = None
    changed_topology = False
    wants_register = force_register or prompt_yes_no("Register component in topology.yaml now?", default_yes=True)

    if wants_register:
        topo = load_topology()
        suggested_id = next_component_id(topo)
        display_default = short
        instance_default = instance_name_from_short(short)

        while True:
            comp_id = prompt_int("Component id", suggested_id)
            display_name = prompt_text("Display name", display_default)
            instance = prompt_identifier("Instance name", instance_default)

            entry = {
                "id": comp_id,
                "name": display_name,
                "class": cls,
                "instance": instance,
                "type": "Active" if is_active else "Passive",
            }
            if not is_active:
                entry["rate_group"] = rate

            error = component_entry_error(topo, entry)
            if error:
                print(f"{Y}[dv-util] {error}{N}")
                suggested_id = next_component_id(topo)
                display_default = display_name
                instance_default = instance
                continue
            break

        add_component_entry(topo, entry)
        save_topology(topo)
        registered_id = entry["id"]
        registered_name = entry["name"]
        changed_topology = True
        print(f"{G}[dv-util] Registered component id={registered_id} in {TOPOLOGY_FILE}{N}")

    created_command = None
    wants_command = force_create_command or prompt_yes_no("Create a command for this component now?", default_yes=False)
    if wants_command:
        command_changed, command_name = run_command_wizard(default_target_id=registered_id)
        changed_topology = changed_topology or command_changed
        created_command = command_name

    return changed_topology, registered_name, created_command


def run_command_wizard(default_target_id: int | None = None) -> tuple[bool, str | None]:
    print(f"\n{B}Create Command Wizard{N}")
    topo = load_topology()
    if not topo.get("components"):
        print(f"{Y}[dv-util] No components in topology. Add/register a component first.{N}")
        return False, None

    name = prompt_identifier("Command name (UPPER_SNAKE_CASE)", "NEW_COMMAND", uppercase=True)
    target_id = choose_target_component(topo, default_target_id=default_target_id)
    op_class = prompt_choice(
        "Op class",
        [
            ("HOUSEKEEPING", "HOUSEKEEPING (allowed in all mission states)"),
            ("OPERATIONAL", "OPERATIONAL (blocked in SAFE_MODE and EMERGENCY)"),
            ("RESTRICTED", "RESTRICTED (NOMINAL only)"),
        ],
        default_index=2,
    )
    description = prompt_text("Description", f"{name} command")

    suggested_opcode = next_opcode_for_target(topo, target_id)
    while True:
        opcode = prompt_int("Opcode", default=suggested_opcode)
        error = command_entry_error(topo, name, target_id, opcode, op_class)
        if error:
            print(f"{Y}[dv-util] {error}{N}")
            suggested_opcode = max(opcode + 1, suggested_opcode + 1)
            continue
        break

    print(f"\n{C}Review:{N}")
    print(f"  Name: {name}")
    print(f"  Target id: {target_id}")
    print(f"  Opcode: {opcode}")
    print(f"  Op class: {op_class}")
    print(f"  Description: {description}")
    if not prompt_yes_no("Add command to topology.yaml now?", default_yes=True):
        print(f"{Y}[dv-util] Command creation cancelled.{N}")
        return False, None

    add_command_entry(topo, name, target_id, opcode, op_class, description)
    save_topology(topo)

    print(f"{G}[dv-util] Added command '{name}' to {TOPOLOGY_FILE}{N}")
    print(f"  target={target_id}  opcode={opcode}  op_class={op_class}")
    return True, name


def cmd_install_standard_apps(_args) -> bool:
    topo = load_topology()
    changed, notes = install_standard_apps(topo)
    if changed:
        save_topology(topo)
        print(f"{G}[dv-util] Standard civilian ops apps installed/updated in {TOPOLOGY_FILE}{N}")
    else:
        print(f"{Y}[dv-util] Standard civilian ops apps already present; no topology changes.{N}")
    for line in notes:
        print(f"  - {line}")
    return changed


def cmd_boot_menu(_args) -> None:
    print(f"\n{B}{'=' * 68}{N}")
    print(f"{B}  DELTA-V Boot Menu  -  Civilian Open Framework Scaffolder{N}")
    print(f"{B}{'=' * 68}{N}")

    actions = [
        ("install_standard", "Install standard civilian ops apps"),
        ("quickstart", "Quickstart: component + command + regenerate"),
        ("component", "Create new component"),
        ("command", "Create new command"),
        ("list", "Show topology summary"),
        ("check", "Run topology check (autocoder dry-run)"),
        ("guide", "Help: how the boot flow works"),
        ("exit", "Exit"),
    ]

    while True:
        print(f"\n{C}Select action:{N}")
        action = prompt_choice("Menu", actions, default_index=1)

        if action == "install_standard":
            changed = cmd_install_standard_apps(argparse.Namespace())
            post_creation_pipeline(changed)
        elif action == "quickstart":
            changed, component_name, command_name = run_component_wizard(
                force_register=True,
                force_create_command=True,
            )
            post_creation_pipeline(changed, component_name=component_name, command_name=command_name)
        elif action == "component":
            changed, component_name, command_name = run_component_wizard()
            post_creation_pipeline(changed, component_name=component_name, command_name=command_name)
        elif action == "command":
            changed, command_name = run_command_wizard()
            post_creation_pipeline(changed, command_name=command_name)
        elif action == "list":
            cmd_list(argparse.Namespace())
        elif action == "check":
            rc = run_check()
            if rc == 0:
                print(f"{G}[dv-util] topology check passed{N}")
            else:
                print(f"{Y}[dv-util] topology check failed with exit code {rc}{N}")
        elif action == "guide":
            print_guide()
        else:
            print("[dv-util] Exiting boot menu.")
            return


# ---------------------------------------------------------------------------
# add-component
# ---------------------------------------------------------------------------
def cmd_add_component(args) -> None:
    dest, cls, hz, short = create_component_header(
        name=args.name,
        active=args.active,
        rate=args.rate,
        profile=args.profile,
    )

    print(f"{G}[dv-util] Created {dest}{N}")
    print(
        f"  Type: {'Active' if args.active else 'Passive'}  "
        f"Rate: {args.rate.upper()} ({hz} Hz)  Profile: {args.profile}"
    )
    if args.active and args.rate == "slow":
        print(f"{Y}  Note: Active defaults are integer-Hz; tune constructor hz manually for sub-Hz loops.{N}")

    if not args.register:
        instance = instance_name_from_short(short)
        print(
            f"""
{C}Next steps:{N}
  1. Add to {B}{TOPOLOGY_FILE}{N}:

     {B}- id: <UNIQUE_INTEGER>
       name: "{short}"
       class: "{cls}"
       instance: "{instance}"
       type: "{'Active' if args.active else 'Passive'}"
       rate_group: "{args.rate}"{N}

  2. Regenerate:
       {B}python3 tools/autocoder.py{N}

  3. Rebuild:
       {B}cmake --build build{N}
"""
        )
        return

    topo = load_topology()
    comp_id = args.component_id if args.component_id is not None else next_component_id(topo)
    display_name = args.display_name or short
    instance = args.instance or instance_name_from_short(short)
    if not validate_cpp_identifier(instance):
        die("--instance must be a valid C++ identifier")

    entry = {
        "id": comp_id,
        "name": display_name,
        "class": cls,
        "instance": instance,
        "type": "Active" if args.active else "Passive",
    }
    if not args.active:
        entry["rate_group"] = args.rate

    add_component_entry(topo, entry)
    save_topology(topo)
    print(f"{G}[dv-util] Registered component id={comp_id} in {TOPOLOGY_FILE}{N}")


# ---------------------------------------------------------------------------
# add-command
# ---------------------------------------------------------------------------
def cmd_add_command(args) -> None:
    topo = load_topology()
    description = args.description.strip() if args.description else f"{args.name} command"
    add_command_entry(
        topo,
        args.name,
        args.target_id,
        args.opcode,
        args.op_class,
        description,
    )
    save_topology(topo)

    print(f"{G}[dv-util] Added command '{args.name}' to {TOPOLOGY_FILE}{N}")
    print(f"  target={args.target_id}  opcode={args.opcode}  op_class={args.op_class}")
    print(f"\n{C}Regenerate:{N}  python3 tools/autocoder.py")


# ---------------------------------------------------------------------------
# quickstart-component
# ---------------------------------------------------------------------------
def cmd_quickstart_component(args) -> None:
    dest, cls, hz, short = create_component_header(
        name=args.name,
        active=args.active,
        rate=args.rate,
        profile=args.profile,
    )
    print(f"{G}[dv-util] Created {dest}{N}")
    print(
        f"  Type: {'Active' if args.active else 'Passive'}  "
        f"Rate: {args.rate.upper()} ({hz} Hz)  Profile: {args.profile}"
    )

    topo = load_topology()
    comp_id = args.component_id if args.component_id is not None else next_component_id(topo)
    display_name = args.display_name or short
    instance = args.instance or instance_name_from_short(short)
    if not validate_cpp_identifier(instance):
        die("--instance must be a valid C++ identifier")

    entry = {
        "id": comp_id,
        "name": display_name,
        "class": cls,
        "instance": instance,
        "type": "Active" if args.active else "Passive",
    }
    if not args.active:
        entry["rate_group"] = args.rate

    add_component_entry(topo, entry)
    print(f"{G}[dv-util] Registered component id={comp_id} in {TOPOLOGY_FILE}{N}")

    command_name = None
    if not args.no_command:
        command_name = args.command_name or f"SET_{upper_snake_from_name(short)}"
        command_name = command_name.upper().replace("-", "_")
        if not validate_cpp_identifier(command_name):
            die("--command-name must be UPPER_SNAKE_CASE letters/digits/underscore")
        opcode = args.opcode if args.opcode is not None else next_opcode_for_target(topo, comp_id)
        description = args.description.strip() if args.description else f"{command_name} command"
        add_command_entry(
            topo,
            command_name,
            comp_id,
            opcode,
            args.op_class,
            description,
        )
        print(f"{G}[dv-util] Added command '{command_name}' target={comp_id} opcode={opcode}{N}")

    save_topology(topo)

    if args.skip_autocoder:
        print(f"{Y}[dv-util] Skipped autocoder. Run manually: python3 tools/autocoder.py{N}")
        return

    rc = run_subprocess_step([sys.executable, "tools/autocoder.py"], "autocoder")
    if rc != 0:
        return

    verify_dictionary_entries(component_name=display_name, command_name=command_name)

    if not args.skip_topology_check:
        run_subprocess_step([sys.executable, "tools/autocoder.py", "--dry-run"], "topology dry-run")

    if args.build:
        run_subprocess_step(
            ["cmake", "--build", args.build_dir, "--target", "flight_software"],
            "flight_software build",
        )

    if args.quickstart_gate:
        run_subprocess_step(
            ["cmake", "--build", args.build_dir, "--target", "quickstart_10min"],
            "quickstart_10min gate",
        )

    print(
        f"""\n{C}To see it live in GDS:{N}
  1. ./build/flight_software
  2. streamlit run gds/gds_dash.py
"""
    )


# ---------------------------------------------------------------------------
# list
# ---------------------------------------------------------------------------
def cmd_list(_args) -> None:
    topo = load_topology()

    s = topo.get("system", {})
    print(f"\n{B}{'=' * 66}{N}")
    print(f"{B}  DELTA-V Topology:  {s.get('name', '?')}  v{s.get('version', '?')}{N}")
    print(f"{B}{'=' * 66}{N}")

    comps = topo.get("components", [])
    print(f"\n  {C}Components ({len(comps)}):{N}")
    print(f"  {'ID':>5}  {'Name':<22}  {'Type':<8}  {'Rate':<7}  Class")
    print(f"  {'-' * 5}  {'-' * 22}  {'-' * 8}  {'-' * 7}  {'-' * 26}")
    for comp in comps:
        rg = comp.get("rate_group", "norm").upper() if comp.get("type", "Passive") != "Active" else "ACTIVE"
        print(
            f"  {comp['id']:>5}  {comp['name']:<22}  {comp.get('type', 'Passive'):<8}"
            f"  {rg:<7}  {comp.get('class', '')}"
        )

    cmds = topo.get("commands", [])
    print(f"\n  {C}Commands ({len(cmds)}):{N}")
    print(f"  {'Name':<30}  {'Target':>6}  {'Opcode':>6}  Op-Class")
    print(f"  {'-' * 30}  {'-' * 6}  {'-' * 6}  {'-' * 16}")
    for cmd in cmds:
        print(
            f"  {cmd['name']:<30}  {cmd['target_id']:>6}  {cmd['opcode']:>6}"
            f"  {cmd.get('op_class', '')}"
        )

    params = topo.get("params", [])
    if params:
        print(f"\n  {C}Parameters ({len(params)}):{N}")
        for param in params:
            tag = f" {Y}[TMR]{N}" if param.get("tmr") else ""
            print(f"  {param['name']:<36} default={param.get('default', 0.0)}{tag}")
    print()


# ---------------------------------------------------------------------------
# check
# ---------------------------------------------------------------------------
def run_check() -> int:
    ac = Path("tools/autocoder.py")
    if not ac.exists():
        die("tools/autocoder.py not found.")
    result = subprocess.run([sys.executable, str(ac), "--dry-run"])
    return result.returncode


def cmd_check(_args) -> None:
    rc = run_check()
    sys.exit(rc)


def cmd_guide(_args) -> None:
    print_guide()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main() -> None:
    parser = argparse.ArgumentParser(
        prog="dv-util",
        description="DELTA-V Developer CLI v4.2",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    sub = parser.add_subparsers(dest="command", required=True)

    boot = sub.add_parser("boot-menu", help="Interactive scaffolding wizard")
    boot.set_defaults(func=cmd_boot_menu)

    guide = sub.add_parser(
        "guide",
        help="Print first-run plug-and-play workflow",
    )
    guide.set_defaults(func=cmd_guide)

    install_standard = sub.add_parser(
        "install-standard-apps",
        help="Install/verify standard civilian ops apps in topology.yaml",
    )
    install_standard.set_defaults(func=cmd_install_standard_apps)

    quick = sub.add_parser(
        "quickstart-component",
        help="One-command scaffold + register + command + regenerate pipeline",
    )
    quick.add_argument("name", help="Component name (e.g. ThermalControl)")
    quick.add_argument("--active", action="store_true", help="Generate an ActiveComponent (own thread)")
    quick.add_argument(
        "--rate",
        choices=["fast", "norm", "slow"],
        default="norm",
        help="Rate group tier (default: norm = 1 Hz)",
    )
    quick.add_argument(
        "--profile",
        choices=PROFILES,
        default="utility",
        help="Scaffold profile hint for cyclic logic (default: utility)",
    )
    quick.add_argument("--id", type=int, dest="component_id", help="Component ID (default: next available)")
    quick.add_argument("--instance", help="Topology instance name (default: generated from class name)")
    quick.add_argument("--display-name", dest="display_name", help="Topology display name (default: short class name)")
    quick.add_argument("--no-command", action="store_true", help="Skip creating an initial command")
    quick.add_argument("--command-name", help="Initial command name (default: SET_<COMPONENT_NAME>)")
    quick.add_argument("--opcode", type=int, help="Initial command opcode (default: next opcode for target)")
    quick.add_argument(
        "--op-class",
        choices=["HOUSEKEEPING", "OPERATIONAL", "RESTRICTED"],
        default="HOUSEKEEPING",
        dest="op_class",
        help="MissionFsm gate class for generated command (default: HOUSEKEEPING)",
    )
    quick.add_argument(
        "--description",
        default="",
        help="Human-readable command description for topology/dictionary",
    )
    quick.add_argument(
        "--skip-autocoder",
        action="store_true",
        help="Skip running tools/autocoder.py after topology updates",
    )
    quick.add_argument(
        "--skip-topology-check",
        action="store_true",
        help="Skip autocoder dry-run topology validation",
    )
    quick.add_argument(
        "--build",
        action="store_true",
        help="Build flight_software after regeneration",
    )
    quick.add_argument(
        "--quickstart-gate",
        action="store_true",
        help="Run cmake --build <build-dir> --target quickstart_10min after regeneration",
    )
    quick.add_argument(
        "--build-dir",
        default="build",
        help="Build directory used by --build/--quickstart-gate (default: build)",
    )
    quick.set_defaults(func=cmd_quickstart_component)

    comp = sub.add_parser("add-component", help="Scaffold a new FSW component header")
    comp.add_argument("name", help="Component name (e.g. ThermalControl)")
    comp.add_argument("--active", action="store_true", help="Generate an ActiveComponent (own thread)")
    comp.add_argument(
        "--rate",
        choices=["fast", "norm", "slow"],
        default="norm",
        help="Rate group tier (default: norm = 1 Hz)",
    )
    comp.add_argument(
        "--profile",
        choices=PROFILES,
        default="utility",
        help="Scaffold profile hint for cyclic logic (default: utility)",
    )
    comp.add_argument(
        "--register",
        action="store_true",
        help="Also append the new component to topology.yaml",
    )
    comp.add_argument("--id", type=int, dest="component_id", help="Component ID (used with --register)")
    comp.add_argument("--instance", help="Topology instance name (used with --register)")
    comp.add_argument("--display-name", dest="display_name", help="Topology display name (used with --register)")
    comp.set_defaults(func=cmd_add_component)

    cmd = sub.add_parser("add-command", help="Add a command entry to topology.yaml")
    cmd.add_argument("name", help="Command name (e.g. SET_HEATER_TARGET)")
    cmd.add_argument("--target-id", type=int, required=True, dest="target_id")
    cmd.add_argument("--opcode", type=int, required=True)
    cmd.add_argument(
        "--op-class",
        choices=["HOUSEKEEPING", "OPERATIONAL", "RESTRICTED"],
        default="OPERATIONAL",
        dest="op_class",
        help="MissionFsm gate class (default: OPERATIONAL)",
    )
    cmd.add_argument(
        "--description",
        default="",
        help="Human-readable command description for topology/dictionary",
    )
    cmd.set_defaults(func=cmd_add_command)

    ls = sub.add_parser("list", help="Print topology summary")
    ls.set_defaults(func=cmd_list)

    check = sub.add_parser("check", help="Validate topology.yaml")
    check.set_defaults(func=cmd_check)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
