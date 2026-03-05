#include "Scheduler.hpp"
#include "TelemHub.hpp"
#include "CommandHub.hpp"
#include "EventHub.hpp"
#include "TelemetryBridge.hpp"
#include "LoggerComponent.hpp"
#include "WatchdogComponent.hpp"
#include "SensorComponent.hpp" 
#include "PowerComponent.hpp"  
#include "ParamDb.hpp"
#include "TimeService.hpp"

using namespace deltav;

int main() {
    TimeService::initEpoch();
    ParamDb::getInstance().load();

    TelemHub telem_hub("TelemHub", 10);
    CommandHub cmd_hub("CmdHub", 11);
    EventHub event_hub("EventHub", 12);
    TelemetryBridge radio("RadioLink", 20);
    LoggerComponent recorder("BlackBox", 30);
    WatchdogComponent watchdog("Supervisor", 1);

    SensorComponent star_tracker("StarTracker", 100);
    PowerComponent battery("BatterySystem", 200);

    // FDIR Registration
    watchdog.registerSubsystem(&radio);
    watchdog.registerSubsystem(&star_tracker);
    watchdog.registerSubsystem(&battery);

    // --- TELEMETRY WIRING ---
    telem_hub.connectInput(star_tracker.telemetry_out_ground);
    telem_hub.connectInput(battery.telemetry_out);
    telem_hub.registerListener(&radio.telem_in);
    telem_hub.registerListener(&recorder.telemetry_in);

    // --- COMMAND WIRING (THE FIX) ---
    radio.command_out.connect(&cmd_hub.cmd_input);

    // Persistent ports for the Hub to route through
    static OutputPort<CommandPacket> batt_route;
    static OutputPort<CommandPacket> track_route;

    batt_route.connect(&battery.cmd_in);
    track_route.connect(&star_tracker.cmd_in);

    cmd_hub.registerRoute(200, &batt_route);
    cmd_hub.registerRoute(100, &track_route);

    // --- EVENT WIRING ---
    static InputPort<EventPacket> e1, e2, e3, e4; 
    battery.event_out.connect(&e1);
    star_tracker.event_out.connect(&e2);
    watchdog.event_out.connect(&e3);
    cmd_hub.ack_out.connect(&e4);

    event_hub.registerEventSource(&e1);
    event_hub.registerEventSource(&e2);
    event_hub.registerEventSource(&e3);
    event_hub.registerEventSource(&e4);
    event_hub.event_out.connect(&radio.event_in);

    battery.battery_out_internal.connect(&watchdog.battery_level_in);

    Scheduler sys;
    sys.registerComponent(&star_tracker);
    sys.registerComponent(&battery);
    sys.registerComponent(&telem_hub);
    sys.registerComponent(&cmd_hub);
    sys.registerComponent(&event_hub);
    sys.registerComponent(&radio);
    sys.registerComponent(&recorder);
    sys.registerComponent(&watchdog);

    sys.initAll();
    sys.executeLoop(1); 

    return 0;
}