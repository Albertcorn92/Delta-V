// =============================================================================
// main.cpp — DELTA-V Flight Software Entry Point
// =============================================================================
#include "ParamDb.hpp"
#include "Scheduler.hpp"
#include "TimeService.hpp"
#include "TopologyManager.hpp"
#include "I2cDriver.hpp" // Added for HAL support
#include <iostream>

using namespace deltav;

int main() {
    std::cout << "=== DELTA-V Flight Software Boot ===\n\n";

    // Phase 0: Global services
    TimeService::initEpoch();
    ParamDb::getInstance().load();

    // Phase 0.5: Hardware Drivers (HAL)
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
    hal::Esp32I2c i2c_bus(I2C_NUM_0); 
#else
    hal::MockI2c  i2c_bus; // Used for SITL on macOS
#endif

    // Phase 1: Instantiate and wire topology
    // We pass the i2c_bus reference so the topology can inject it into components
    TopologyManager topology(i2c_bus); 
    topology.wire();
    topology.registerFdir();

    if (!topology.verify()) {
        std::cerr << "[FATAL] Topology verification failed. Aborting boot.\n";
        return 1;
    }

    // Phase 2 & 3: Scheduler
    Scheduler sys;
    topology.registerAll(sys);
    sys.initAll();
    
    // 1 Hz master loop — Active components (like TelemBridge) run at their own rates
    sys.executeLoop(1); 

    return 0;
}