// =============================================================================
// main.cpp — DELTA-V Flight Software Entry Point  v4.0
// =============================================================================
#include "HeapGuard.hpp"
#include "I2cDriver.hpp"
#include "ParamDb.hpp"
#include "RateGroupExecutive.hpp"
#include "TimeService.hpp"
#include "TopologyManager.hpp"

#include <exception>
#include <cstdio>

#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
#include <csignal>
#endif
#if defined(DELTAV_ESP_SAFE_MODE)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

using namespace deltav;

#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static RateGroupExecutive* g_rge = nullptr;

static auto on_signal(int /*signum*/) -> void {
    (void)std::fprintf(stderr, "\n[Boot] Signal received - initiating clean shutdown.\n");
    if (g_rge != nullptr) {
        g_rge->stopAll();
    }
}
#endif

auto main() -> int {
    try {
        std::printf(
            "\n"
            "  ============================================\n"
            "    DELTA-V Flight Software v4.0\n"
            "    High-Assurance Autonomy Framework\n"
            "  ============================================\n\n");

    TimeService::initEpoch();
    ParamDb::getInstance().load();

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
    // Keep large framework objects off the ESP main task stack.
    static hal::Esp32I2c i2c_bus(I2C_NUM_0);
    std::printf("[Boot] HAL: ESP32 I2C hardware driver\n");
#if defined(DELTAV_ESP_SAFE_MODE)
    std::printf("[Boot] ESP safe mode active: local-only idle loop.\n");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif
#else
    hal::MockI2c i2c_bus;
    std::printf("[Boot] HAL: SITL MockI2c (sinusoidal sensor simulation)\n");
#endif

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
    static TopologyManager topology(i2c_bus);
#else
    TopologyManager topology(i2c_bus);
#endif
    topology.wire();
    topology.registerFdir();

    if (!topology.verify()) {
        (void)std::fprintf(stderr, "[Boot] FATAL: Topology verification failed. Aborting.\n");
        return 1;
    }

    // Static storage avoids stack-address escape through the host signal bridge pointer.
    static RateGroupExecutive rge;
#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
    g_rge = &rge;
#endif

    topology.registerAll(rge);

    rge.initAll();

    // HeapGuard is useful for host/SITL determinism, but on ESP-IDF the C++
    // runtime and RTOS paths may still require heap allocations after boot.
    // Keep it disabled on embedded targets to avoid allocator recursion panics.
#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
    HeapGuard::arm();
#endif

#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
    (void)std::signal(SIGINT,  on_signal);
    (void)std::signal(SIGTERM, on_signal);
#endif

#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
    std::printf("[Boot] Heap locked - dynamic allocation permanently disabled.\n");
#else
    std::printf("[Boot] Heap guard disabled on ESP target (local-only runtime).\n");
#endif
    std::printf("[Boot] Rate groups launching:\n");
    std::printf("[Boot]   FAST    10 Hz  - %zu component(s)\n", rge.fastCount());
    std::printf("[Boot]   NORM     1 Hz  - %zu component(s)\n", rge.normCount());
    std::printf("[Boot]   SLOW   0.1 Hz  - %zu component(s)\n", rge.slowCount());
    std::printf("[Boot]   ACTIVE own thr - %zu component(s)\n\n", rge.activeCount());

    rge.startAll(); 

        std::printf("[Boot] Saving parameter database...\n");
        ParamDb::getInstance().save();
        std::printf("[Boot] Shutdown complete.\n");
        return 0;
    } catch (const std::exception& ex) {
        (void)std::fprintf(stderr, "[Boot] FATAL: unhandled exception: %s\n", ex.what());
        return 2;
    } catch (...) {
        (void)std::fprintf(stderr, "[Boot] FATAL: unknown unhandled exception.\n");
        return 3;
    }
}
