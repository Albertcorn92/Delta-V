# 🛰️ DELTA-V Autonomy Framework  v3.0

DELTA-V is a high-performance, zero-copy **C++20 flight software framework** for civilian aerospace, robotics, and research missions. It is designed for deterministic behavior and high assurance without heavyweight queues, XML dictionaries, or complex toolchains.

The framework targets **DO-178C DAL-B compliance** and is designed for embedded aerospace systems from prototype SITL on macOS/Linux through to FreeRTOS on Cortex-M7 hardware.

Roadmap for raising assurance rigor: `docs/HIGH_ASSURANCE_ROADMAP.md`.
Safety gates and traceability workflow: `docs/SAFETY_ASSURANCE.md`.
ESP32 hardware bring-up plan: `docs/ESP32_BRINGUP.md`.

---

## 🚀 Philosophy: Compile-Time Mission Safety

Using **C++20 Concepts and Template Metaprogramming**, DELTA-V moves system validation to the **compiler**. Component wiring and data types are verified at compile time. If the spacecraft's internal architecture is incorrect, **the program will not compile**.

After boot, the heap is **permanently locked** by `HeapGuard::arm()`. Any inadvertent call to `malloc` or `new` during flight aborts with a fatal diagnostic.

---

## 🧱 What's New in v3.0

| Feature | Description |
|---|---|
| **HeapGuard** | Post-init heap lock: overrides `operator new` to fatal-abort after `arm()` |
| **TmrStore** | Triple Modular Redundancy for critical params — SEU self-healing via majority vote |
| **COBS Framing** | Consistent Overhead Byte Stuffing — guarantees frame sync after bit errors |
| **MissionFsm** | Formal FSM gates uplink commands by state: SAFE_MODE blocks OPERATIONAL opcodes |
| **Requirements.hpp** | RTM header — every DV-XXX-NN requirement linked to tests |
| **FaultInjection** | `WatchdogComponent::injectBatteryLevel()` hook for HIL destructive testing |
| **TMR Scrub** | Watchdog calls `TmrRegistry::scrubAll()` every 30 cycles |

---

## 🏗️ System Architecture

### Flight Software Layers (C++20)

```
┌─────────────────────────────────────────────────┐
│  Ground Data System (Python / Streamlit)         │
│  gds/gds_dash.py ←UDP 9001→ TelemetryBridge      │
│               ←UDP 9002→  (CCSDS + COBS)          │
└───────────────────┬─────────────────────────────┘
                    │
┌───────────────────▼─────────────────────────────┐
│  TelemetryBridge (ActiveComponent, 20 Hz)        │
│  Downlink: CCSDS + CRC-16 + COBS                 │
│  Uplink:   CCSDS command frame validation         │
└────┬──────────────┬──────────────────────────────┘
     │              │
┌────▼────┐   ┌─────▼──────┐   ┌─────────────────┐
│TelemHub │   │ CommandHub │   │   EventHub      │
│ Fan-Out │   │ +MissionFsm│   │  Broadcast      │
└────┬────┘   └─────┬──────┘   └────────┬────────┘
     │              │                   │
┌────▼──────────────▼───────────────────▼────────┐
│           Component Layer (Passive)             │
│  StarTracker  BatterySystem  ImuComponent       │
│  SensorComp   PowerComp      (HAL-injected)     │
└────────────────────┬────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────┐
│  WatchdogComponent (FDIR Supervisor)             │
│  MissionFsm  ·  TmrRegistry  ·  ParamDb CRC     │
└─────────────────────────────────────────────────┘
```

---

## 🛠️ Building

### Prerequisites

- C++20: Clang 14+ or GCC 11+
- CMake 3.15+
- Python 3.9+ (GDS dashboard)
- `pip install streamlit pyyaml`

### Compile

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

### Run SITL

```bash
# Terminal 1 — Flight Software
./build/flight_software

# Terminal 2 — GDS Dashboard
streamlit run gds/gds_dash.py
```

### Run Unit Tests

```bash
cd build && ctest --output-on-failure
```

### Validate Requirements Traceability

```bash
cmake --build build --target traceability
# emits build/requirements_trace_matrix.md and requirements_trace_matrix.json
```

### Run Full Flight Readiness Gate

```bash
cmake --build build --target flight_readiness
```

### Generate Qualification Evidence Bundle

```bash
cmake --build build --target qualification_bundle
# emits build/qualification/qualification_report.{md,json}
```

Process templates for mission certification work are in `docs/process/`.
Open-source release checklist is in `docs/OPEN_SOURCE_RELEASE_CHECKLIST.md`.

---

## 📝 Adding a New Component

### Quick scaffold

```bash
python3 tools/dv-util.py add-component ThermalControl
# or for an Active (threaded) component:
python3 tools/dv-util.py add-component ThermalControl --active
```

### Manual steps

1. Add the component to `topology.yaml`
2. Run `python3 tools/autocoder.py` to regenerate `Types.hpp` and `dictionary.json`
3. Wire ports in `TopologyManager.hpp` (or re-run the autocoder)
4. Add unit tests referencing the relevant DV-XXX-NN requirements

---

## 🔐 Security

Uplink commands use strict CCSDS command-frame validation plus anti-replay sequence checks. The `MissionFsm` additionally gates commands by mission state. Restricted commands (e.g. actuator enable/deploy operations) are blocked in `SAFE_MODE` and `EMERGENCY`.

Set replay-state persistence path before launch:

```bash
export DELTAV_REPLAY_SEQ_FILE="replay_seq.db"
```

---

## 📋 Requirements Traceability

See `src/Requirements.hpp` for the full RTM. Every unit test references its governing requirement ID in a comment (e.g. `// DV-FDIR-01`).

---

## ⚖️ Legal Scope

- DELTA-V is scoped for civilian, scientific, and educational use.
- Contributions that add military, weapons, targeting, or fire-control behavior are out of scope and will be rejected.
- This repository is intended for research/prototyping and is not certified for operational or safety-critical deployment.
- This repository includes compliance guidance, but it is **not legal advice** and does not provide legal clearance by itself.
- See `docs/CIVILIAN_USE_POLICY.md` and `docs/EXPORT_CONTROL_NOTE.md` before release or deployment.

---

## 🧪 Fault Injection / HIL Testing

```cpp
// Simulate battery drain without waiting for real discharge:
topology.watchdog.injectBatteryLevel(4.0f); // → SAFE_MODE
topology.watchdog.injectBatteryLevel(1.5f); // → EMERGENCY

// Simulate a stuck I2C bus via MockI2c override in unit tests
```

---

## Author

**Albert Cornelius** · Apache License 2.0

## Project Governance

- Contributing guide: `CONTRIBUTING.md`
- Security policy: `SECURITY.md`
- Code of conduct: `CODE_OF_CONDUCT.md`
- Release notes: `CHANGELOG.md`
- Disclaimer: `DISCLAIMER.md`
- Civilian use policy: `docs/CIVILIAN_USE_POLICY.md`
- Export-control note: `docs/EXPORT_CONTROL_NOTE.md`
