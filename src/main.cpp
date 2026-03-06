// =============================================================================
// main.cpp — DELTA-V Flight Software Entry Point
// =============================================================================
// Boot sequence (mirrors NASA FSW boot phases):
//   Phase 0: Hardware/service init (TimeService, ParamDb)
//   Phase 1: Topology wiring   (TopologyManager::wire)
//   Phase 2: Subsystem init    (Scheduler::initAll → Component::init)
//   Phase 3: Real-time loop    (Scheduler::executeLoop)
// =============================================================================
#include "ParamDb.hpp"
#include "Scheduler.hpp"
#include "TimeService.hpp"
#include "TopologyManager.hpp"
#include <iostream>

using namespace deltav;

int main() {
    std::cout << "=== DELTA-V Flight Software Boot ===\n\n";

    // Phase 0: Global services
    TimeService::initEpoch();
    ParamDb::getInstance().load();

    // Phase 1: Instantiate and wire topology
    TopologyManager topology;
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
    sys.executeLoop(1); // 1 Hz master loop — Active components run at their own rates

    return 0;
}