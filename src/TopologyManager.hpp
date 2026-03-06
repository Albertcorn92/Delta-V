#pragma once
// =============================================================================
// TopologyManager.hpp — UPDATED FOR HAL SUPPORT
// =============================================================================

#include "Port.hpp"
#include "Types.hpp"
#include "Scheduler.hpp"
#include "Hal.hpp"  // Added
#include <iostream>
#include "WatchdogComponent.hpp"
#include "TelemHub.hpp"
#include "CommandHub.hpp"
#include "EventHub.hpp"
#include "TelemetryBridge.hpp"
#include "LoggerComponent.hpp"
#include "SensorComponent.hpp"
#include "PowerComponent.hpp"
#include "ImuComponent.hpp" // Added

namespace deltav {

class TopologyManager {
public:
    // We now pass hardware references into the components that need them
    TopologyManager(hal::I2cBus& i2c) 
        : imu_unit("IMU_01", 300, i2c) {} 

    WatchdogComponent    watchdog       {"Supervisor", 1};
    TelemHub             telem_hub      {"TelemHub", 10};
    CommandHub           cmd_hub        {"CmdHub", 11};
    EventHub             event_hub      {"EventHub", 12};
    TelemetryBridge      radio          {"RadioLink", 20};
    LoggerComponent      recorder       {"BlackBox", 30};
    SensorComponent      star_tracker   {"StarTracker", 100};
    PowerComponent       battery        {"BatterySystem", 200};
    ImuComponent         imu_unit; // New hardware component

    void wire() {
        wireTelemetry();
        wireCommands();
        wireEvents();
        wireCustom();
        std::cout << "[Topology] All ports connected (including Hardware HAL).\n";
    }

    void registerAll(Scheduler& sys) {
        sys.registerComponent(&watchdog);
        sys.registerComponent(&telem_hub);
        sys.registerComponent(&cmd_hub);
        sys.registerComponent(&event_hub);
        sys.registerComponent(&radio);
        sys.registerComponent(&recorder);
        sys.registerComponent(&star_tracker);
        sys.registerComponent(&battery);
        sys.registerComponent(&imu_unit); // Register new IMU
    }

    void registerFdir() {
        watchdog.registerSubsystem(&watchdog);
        watchdog.registerSubsystem(&cmd_hub);
        watchdog.registerSubsystem(&event_hub);
        watchdog.registerSubsystem(&radio);
        watchdog.registerSubsystem(&star_tracker);
        watchdog.registerSubsystem(&battery);
        watchdog.registerSubsystem(&imu_unit); // Protect the IMU
    }

    bool verify() {
        bool ok = true;
        auto check = [&](bool condition, const char* name) {
            if (!condition) {
                std::cerr << "[Topology] UNCONNECTED/INVALID: " << name << "\n";
                ok = false;
            }
        };

        check(radio.command_out.isConnected(), "radio.command_out -> cmd_hub");
        check(imu_unit.telemetry_out.isConnected(), "imu.telemetry_out -> telem_hub");
        
        if (!ok) std::cerr << "[Topology] FATAL: wiring incomplete.\n";
        return ok;
    }

private:
    OutputPort<CommandPacket> battery_cmd_route;
    OutputPort<CommandPacket> imu_cmd_route; // Route for IMU

    InputPort<EventPacket> e_watchdog;
    InputPort<EventPacket> e_cmd_hub;
    InputPort<EventPacket> e_star_tracker;
    InputPort<EventPacket> e_battery;
    InputPort<EventPacket> e_imu; // IMU event input

    void wireTelemetry() {
        telem_hub.registerListener(&radio.telem_in);
        telem_hub.registerListener(&recorder.telemetry_in);
        telem_hub.connectInput(star_tracker.telemetry_out);
        telem_hub.connectInput(battery.telemetry_out);
        telem_hub.connectInput(imu_unit.telemetry_out); // Connect IMU
    }

    void wireCommands() {
        radio.command_out.connect(&cmd_hub.cmd_input);
        
        battery_cmd_route.connect(&battery.cmd_in);
        cmd_hub.registerRoute(200, &battery_cmd_route);

        imu_cmd_route.connect(&imu_unit.cmd_in);
        cmd_hub.registerRoute(300, &imu_cmd_route); // Route 300 to IMU
    }

    void wireEvents() {
        event_hub.registerListener(&radio.event_in);
        event_hub.registerListener(&recorder.event_in);
        
        watchdog.event_out.connect(&e_watchdog);
        event_hub.registerEventSource(&e_watchdog);
        
        cmd_hub.ack_out.connect(&e_cmd_hub);
        event_hub.registerEventSource(&e_cmd_hub);
        
        star_tracker.event_out.connect(&e_star_tracker);
        event_hub.registerEventSource(&e_star_tracker);
        
        battery.event_out.connect(&e_battery);
        event_hub.registerEventSource(&e_battery);

        imu_unit.event_out.connect(&e_imu); // Connect IMU events
        event_hub.registerEventSource(&e_imu);
    }

    void wireCustom() {
        battery.battery_out_internal.connect(&watchdog.battery_level_in);
    }
};

} // namespace deltav