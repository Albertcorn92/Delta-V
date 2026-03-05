# 🛰️ DELTA-V Autonomy Framework

DELTA-V is a high-performance, zero-copy **C++20 aerospace flight software executive** designed to provide the reliability of NASA’s **F´ (F Prime)** without the overhead of heavy message queues, XML dictionaries, or complex Python autocoders.

The framework focuses on **determinism, compile-time safety, and minimal runtime overhead**, making it suitable for embedded aerospace systems and edge-computing flight hardware.

---

# 🚀 Philosophy: Compile-Time Mission Safety

Traditional aerospace software frameworks rely heavily on **runtime validation**, where wiring errors or incorrect data routing are only detected during execution.

DELTA-V takes a different approach.

Using **C++20 Concepts and Template Metaprogramming**, the framework moves system validation to the **compiler**. Component wiring and data types are mathematically verified during compilation.

If the spacecraft’s internal software architecture is incorrect, **the program will not compile**.

This approach dramatically reduces runtime failure risk and increases mission reliability.

---

# 🧱 Key Architectural Pillars

### Broadcast Data Bus
DELTA-V uses a centralized telemetry distribution hub built on a **subscriber-listener pattern**.

A single telemetry source can broadcast to multiple destinations simultaneously, such as:

- Radio Downlink
- BlackBox Storage
- FDIR Systems (Fault Detection, Isolation, and Recovery)

This architecture prevents pointer conflicts while allowing efficient multi-destination streaming.

---

### Zero-Copy Memory Path
All data movement occurs through `InputPort` and `OutputPort` structures that pass **direct memory references**.

Benefits include:

- Zero data duplication
- Lower CPU overhead
- Reduced latency
- Deterministic communication timing

---

### Static Execution Model
DELTA-V enforces strict static memory usage.

After the boot sequence:

- `malloc` is prohibited  
- `new` is prohibited  

This prevents:

- heap fragmentation  
- runtime allocation failures  
- unpredictable memory behavior during flight

---

### Bit-Cast Serialization
Telemetry packets are packed using **`std::bit_cast` binary serialization**.

Advantages include:

- extremely fast binary packing
- hardware-level data formatting
- zero transformation overhead before radio transmission

---

# 🛠️ System Architecture

DELTA-V is composed of two major layers:

1. **Flight Software (C++20)**
2. **Ground Data System (Python)**

---

# Flight Software (C++20)

### Scheduler.hpp — System Heartbeat

Manages deterministic execution cycles and enforces strict timing across all registered flight components.

Each component executes within a predictable scheduler tick.

---

### TelemHub.hpp — Telemetry Bus

Acts as the **central nervous system** for the spacecraft.

Responsibilities include:

- multiplexing sensor data
- broadcasting telemetry to listeners
- distributing system status information

---

### CommandHub.hpp — Command Router

Receives uplink commands from ground control and routes them to the appropriate component ID within the flight software.

---

### ParamDb.hpp — Parameter Database

Persistent storage for mission configuration values such as:

- PID controller gains
- battery drain constants
- system calibration values

The database persists across system reboots.

---

### TelemetryBridge.hpp — Ground Link

Handles **UDP-based communication** between the flight software and the Ground Data System during Software-in-the-Loop (SITL) testing.

---

# 🖥️ Ground Data System (Python 3.x)

The Ground Data System (GDS) provides a modern **Streamlit-based mission dashboard**.

Capabilities include:

### Real-Time Analytics
Live plotting of telemetry streams including sensor data, battery voltage, and subsystem metrics.

### Mission Clock
Maintains synchronized millisecond-level timing between flight software and ground control.

### Event Console
A color-coded system log displaying alerts, warnings, and heartbeat messages from onboard components.

### Uplink Control
Allows operators to issue commands and tune parameters in real time during testing.

---

# 🏗️ Building and Launching

## Prerequisites

- C++20 compatible compiler  
  - Clang 12+  
  - GCC 11+  

- CMake **3.15 or newer**

- Python **3.9+** (for Ground Data System)

---

# Compilation

Run the following commands from the project root directory:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

---

# Launching Mission Control

## Start the Flight Software

```bash
./flight_software
```

---

## Start the Ground Data System Dashboard

```bash
streamlit run gds_dash.py
```

---

# 📝 Creating a New Component

DELTA-V is designed for **rapid subsystem development**.

To add a new flight subsystem such as a **Thruster Controller** or **Star Tracker**:

### 1. Inherit from `deltav::Component`

Create a new class that derives from the base framework component.

### 2. Define Ports

Add communication endpoints:

- `InputPort` for commands
- `OutputPort` for telemetry

### 3. Implement `step()`

This function executes once per scheduler tick.

### 4. Wire the Component in `main.cpp`

Use the telemetry hub to connect the subsystem to the system bus.

---

# Example Component

```cpp
// Minimal Power System Example

class Battery : public deltav::Component {
public:
    void step() override {
        float voltage = read_sensor();

        TelemetryPacket p = {
            timestamp,
            getId(),
            voltage
        };

        telemetry_out.send(Serializer::pack(p));
    }

    OutputPort<Serializer::ByteArray> telemetry_out;
};
```

---

# Author

**Albert Cornelius**