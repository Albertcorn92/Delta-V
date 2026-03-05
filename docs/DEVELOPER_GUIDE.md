DEVELOPER_GUIDE.md - DELTA-V User Manual
1. Introduction
Welcome to the DELTA-V development environment. This guide will walk you through creating a custom Flight Software (FSW) component, wiring it into the system, and exposing it to the Ground Data System (GDS).

2. Creating a Custom Component
Every subsystem in DELTA-V is a class that inherits from deltav::Component.

Step 1: Define your Component
Create a new header file (e.g., src/ThermalComponent.hpp). You will need to inherit from the base Component class, define your Input and Output ports, and override the init() and step() functions.

Inside the step() function, you should place your cyclic logic—such as reading sensor data or checking for incoming commands via cmd_in.consume(). To send telemetry, you simply populate a TelemetryPacket struct and call telem_out.send(p).

3. The Blueprint: topology.yaml
To make the ground system aware of your new component and its commands, you must update the topology.yaml file in the root directory. Add your component name and ID to the components list. If your component accepts commands, add those to the commands list with a unique opcode and the matching target_id.

4. The Autocoder Pipeline
After updating the YAML file, you must run the autocoder to synchronize the C++ types and the Ground Dictionary. Run the following command in your terminal:

python3 autocoder.py

This will update src/Types.hpp and dictionary.json automatically. Never edit these files by hand.

5. System Integration (main.cpp)
Finally, you must instantiate your component in your main.cpp. There are four standard steps for integration:

Instantiate: Create the object with a name and ID.

Wire Telemetry: Use telem_hub.connectInput() to pipe your component's telemetry to the radio.

Register Commands: Connect an OutputPort from the CommandHub to your component's input port.

Schedule: Call sys.registerComponent() to add it to the 1Hz execution loop.

6. Testing with the GDS
To see your data live:

Compile & Run FSW: Use CMake to build and run the flight_software executable.

Launch Dashboard: Run streamlit run gds_dash.py in a separate terminal.

Your new subsystem will automatically appear in the Telemetry Analytics and the Raw Buffer without any further Python coding required.