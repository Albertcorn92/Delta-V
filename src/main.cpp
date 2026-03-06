// =============================================================================
// main.cpp — DELTA-V Flight Software Entry Point  v4.0
// =============================================================================
#include "HeapGuard.hpp"
#include "I2cDriver.hpp"
#include "ParamDb.hpp"
#include "RateGroupExecutive.hpp"
#include "TimeService.hpp"
#include "TopologyManager.hpp"

#include <csignal>
#include <exception>
#include <iostream>

using namespace deltav;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static RateGroupExecutive* g_rge = nullptr;

static auto on_signal(int /*signum*/) -> void {
    std::cerr << "\n[Boot] Signal received — initiating clean shutdown.\n";
    if (g_rge != nullptr) {
        g_rge->stopAll();
    }
}

auto main() -> int {
    try {
        std::cout
        << "\n"
        << "  ╔══════════════════════════════════════════╗\n"
        << "  ║     DELTA-V Flight Software  v4.0        ║\n"
        << "  ║   High-Assurance Autonomy Framework      ║\n"
        << "  ╚══════════════════════════════════════════╝\n\n";

    TimeService::initEpoch();
    ParamDb::getInstance().load();

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
    hal::Esp32I2c i2c_bus(I2C_NUM_0);
    std::cout << "[Boot] HAL: ESP32 I2C hardware driver\n";
#else
    hal::MockI2c  i2c_bus;
    std::cout << "[Boot] HAL: SITL MockI2c (sinusoidal sensor simulation)\n";
#endif

    TopologyManager topology(i2c_bus);
    topology.wire();
    topology.registerFdir();

    if (!topology.verify()) {
        std::cerr << "[Boot] FATAL: Topology verification failed. Aborting.\n";
        return 1;
    }

    RateGroupExecutive rge;
    g_rge = &rge;

    topology.registerAll(rge);

    rge.initAll();

    HeapGuard::arm();

    (void)std::signal(SIGINT,  on_signal);
    (void)std::signal(SIGTERM, on_signal);

    std::cout << "[Boot] Heap locked — dynamic allocation permanently disabled.\n";
    std::cout << "[Boot] Rate groups launching:\n";
    std::cout << "[Boot]   FAST    10 Hz  — " << rge.fastCount()   << " component(s)\n";
    std::cout << "[Boot]   NORM     1 Hz  — " << rge.normCount()   << " component(s)\n";
    std::cout << "[Boot]   SLOW   0.1 Hz  — " << rge.slowCount()   << " component(s)\n";
    std::cout << "[Boot]   ACTIVE own thr — " << rge.activeCount() << " component(s)\n\n";

    rge.startAll(); 

        std::cout << "[Boot] Saving parameter database...\n";
        ParamDb::getInstance().save();
        std::cout << "[Boot] Shutdown complete.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[Boot] FATAL: unhandled exception: " << ex.what() << "\n";
        return 2;
    } catch (...) {
        std::cerr << "[Boot] FATAL: unknown unhandled exception.\n";
        return 3;
    }
}
