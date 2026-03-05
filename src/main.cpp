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

using namespace deltav;

int main() {
    // 1. INFRASTRUCTURE INSTANTIATION
    TelemHub telem_hub("TelemHub", 10);
    CommandHub cmd_hub("CmdHub", 11);
    EventHub event_hub("EventHub", 12);
    TelemetryBridge radio("RadioLink", 20);
    LoggerComponent recorder("BlackBox", 30);
    WatchdogComponent watchdog("Supervisor", 1);

    // 2. USER COMPONENTS
    SensorComponent star_tracker("StarTracker", 100);
    PowerComponent battery("BatterySystem", 200);

    // 3. WIRING: TELEMETRY (Upward Path)
    auto* t1 = new InputPort<Serializer::ByteArray>();
    auto* t2 = new InputPort<Serializer::ByteArray>();
    star_tracker.telemetry_out_ground.connect(t1);
    battery.telemetry_out.connect(t2);
    telem_hub.registerInput(t1);
    telem_hub.registerInput(t2);
    
    telem_hub.output.connect(&radio.telem_in);
    telem_hub.output.connect(&recorder.telemetry_in);

    // 4. WIRING: COMMANDS (Downward Path)
    radio.command_out.connect(&cmd_hub.cmd_input);

    auto* hub_to_battery = new OutputPort<CommandPacket>();
    hub_to_battery->connect(&battery.cmd_in);
    cmd_hub.registerRoute(200, hub_to_battery);

    auto* hub_to_sensor = new OutputPort<CommandPacket>();
    hub_to_sensor->connect(&star_tracker.cmd_in);
    cmd_hub.registerRoute(100, hub_to_sensor);

    // 5. WIRING: EVENTS (Status Path)
    // Create ports for all components that emit events
    auto* e1 = new InputPort<EventPacket>();
    auto* e2 = new InputPort<EventPacket>();
    auto* e3 = new InputPort<EventPacket>(); // For the Watchdog
    
    battery.event_out.connect(e1);
    star_tracker.event_out.connect(e2);
    watchdog.event_out.connect(e3);
    
    event_hub.registerEventSource(e1);
    event_hub.registerEventSource(e2);
    event_hub.registerEventSource(e3);

    // FINAL LINK: Connect the EventHub to the Radio for Downlink
    event_hub.event_out.connect(&radio.event_in);

    // 6. EXECUTION ENGINE
    Scheduler sys;
    sys.registerComponent(&star_tracker);
    sys.registerComponent(&battery);
    sys.registerComponent(&telem_hub);
    sys.registerComponent(&cmd_hub);
    sys.registerComponent(&event_hub);
    sys.registerComponent(&radio);
    sys.registerComponent(&recorder);
    sys.registerComponent(&watchdog);

    // Start 1Hz Real-Time Loop
    sys.executeLoop(1); 

    return 0;
}