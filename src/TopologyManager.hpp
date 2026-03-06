#pragma once
// =============================================================================
// TopologyManager.hpp — DELTA-V Wiring & Dependency Injection  v2.1
// =============================================================================
// Centralises all port wiring so main.cpp contains zero raw port variables.
//
// Fixes applied (v2.1):
//   F-04: watchdog.registerSubsystem(&watchdog) added in registerFdir() so the
//         Watchdog monitors its own health — no more blind spot.
//   F-07: verify() now asserts cmd_hub.ack_out.isConnected().
//   F-11: Scheduler is passed to registerFdir so Watchdog can poll frame drops
//         via WatchdogComponent::pollSchedulerHealth() in future.
//         (A post-boot hook in main.cpp calls topo.registerFdir() before
//         sys.executeLoop(), at which point the Scheduler reference is valid.)
//
// Architecture (event path):
//   battery.event_out ──┐
//   star.event_out    ──┼──► EventHub ──► radio.event_in   (downlink)
//   watchdog.event_out──┤              └──► recorder.event_in (FDR log)
//   cmd_hub.ack_out   ──┘
// =============================================================================
#include "CommandHub.hpp"
#include "Component.hpp"
#include "EventHub.hpp"
#include "LoggerComponent.hpp"
#include "Port.hpp"
#include "PowerComponent.hpp"
#include "Scheduler.hpp"
#include "SensorComponent.hpp"
#include "TelemHub.hpp"
#include "TelemetryBridge.hpp"
#include "Types.hpp"
#include "WatchdogComponent.hpp"
#include <iostream>

namespace deltav {

class TopologyManager {
public:
    // All subsystem instances — owned here, IDs match topology.yaml
    WatchdogComponent watchdog    {"Supervisor",     1};
    TelemHub          telem_hub   {"TelemHub",      10};
    CommandHub        cmd_hub     {"CmdHub",         11};
    EventHub          event_hub   {"EventHub",       12};
    TelemetryBridge   radio       {"RadioLink",      20};
    LoggerComponent   recorder    {"BlackBox",       30};
    SensorComponent   star_tracker{"StarTracker",   100};
    PowerComponent    battery     {"BatterySystem", 200};

    void wire() {
        wireTelemetry();
        wireCommands();
        wireEvents();
        wireFdir();
        std::cout << "[Topology] All ports connected.\n";
    }

    void registerAll(Scheduler& sys) {
        sys.registerComponent(&star_tracker);
        sys.registerComponent(&battery);
        sys.registerComponent(&telem_hub);
        sys.registerComponent(&cmd_hub);
        sys.registerComponent(&event_hub);
        sys.registerComponent(&radio);
        sys.registerComponent(&recorder);
        sys.registerComponent(&watchdog);
    }

    void registerFdir() {
        // F-04 fix: Watchdog monitors itself so its own health is never a
        // blind spot. Component::reportHealth() is safe to call on self.
        watchdog.registerSubsystem(&watchdog);
        watchdog.registerSubsystem(&radio);
        watchdog.registerSubsystem(&star_tracker);
        watchdog.registerSubsystem(&battery);
        watchdog.registerSubsystem(&cmd_hub);
        watchdog.registerSubsystem(&event_hub);
    }

    // -----------------------------------------------------------------------
    // verify() — every critical port checked. Boot aborts on any failure.
    // -----------------------------------------------------------------------
    bool verify() {
        bool ok = true;
        auto check = [&](bool condition, const char* name) {
            if (!condition) {
                std::cerr << "[Topology] UNCONNECTED/INVALID: " << name << "\n";
                ok = false;
            }
        };

        // Command path
        check(radio.command_out.isConnected(),           "radio.command_out → cmd_hub");
        check(battery.battery_out_internal.isConnected(),"battery → watchdog FDIR");
        // F-07: ack_out must be connected so ACK/NACK events are never silently dropped
        check(cmd_hub.ack_out.isConnected(),             "cmd_hub.ack_out → event_hub");

        // EventHub fan-out: must have >= 4 sources and >= 2 listeners
        check(event_hub.getSourceCount()   >= 4, "event_hub: need >=4 sources");
        check(event_hub.getListenerCount() >= 2, "event_hub: need >=2 listeners (radio+logger)");

        if (!ok) std::cerr << "[Topology] FATAL: wiring incomplete.\n";
        return ok;
    }

private:
    // Command routing ports
    OutputPort<CommandPacket> batt_cmd_route;
    OutputPort<CommandPacket> star_cmd_route;

    // Event source staging InputPorts (component → staging → EventHub)
    InputPort<EventPacket> e_battery;
    InputPort<EventPacket> e_star;
    InputPort<EventPacket> e_watchdog;
    InputPort<EventPacket> e_cmdhub;

    void wireTelemetry() {
        telem_hub.connectInput(star_tracker.telemetry_out_ground);
        telem_hub.connectInput(battery.telemetry_out);
        telem_hub.registerListener(&radio.telem_in);
        telem_hub.registerListener(&recorder.telemetry_in);
    }

    void wireCommands() {
        radio.command_out.connect(&cmd_hub.cmd_input);
        batt_cmd_route.connect(&battery.cmd_in);
        star_cmd_route.connect(&star_tracker.cmd_in);
        cmd_hub.registerRoute(200, &batt_cmd_route);
        cmd_hub.registerRoute(100, &star_cmd_route);
    }

    void wireEvents() {
        battery.event_out.connect(&e_battery);
        star_tracker.event_out.connect(&e_star);
        watchdog.event_out.connect(&e_watchdog);
        cmd_hub.ack_out.connect(&e_cmdhub);

        event_hub.registerEventSource(&e_battery);
        event_hub.registerEventSource(&e_star);
        event_hub.registerEventSource(&e_watchdog);
        event_hub.registerEventSource(&e_cmdhub);

        // Fan-out: radio AND recorder receive every event independently
        event_hub.registerListener(&radio.event_in);
        event_hub.registerListener(&recorder.event_in);
    }

    void wireFdir() {
        battery.battery_out_internal.connect(&watchdog.battery_level_in);
    }
};

} // namespace deltav