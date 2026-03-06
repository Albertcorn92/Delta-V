#pragma once
// =============================================================================
// HeapGuard.hpp — DELTA-V Post-Initialization Heap Lock
// =============================================================================
// High-assurance flight software practice forbids dynamic memory allocation
// after the boot phase is complete. Once HeapGuard::arm() is called
// (at end of main() init),
// any subsequent call to malloc/new triggers a CRITICAL_FAILURE event and
// halts the system.
//
// Implementation uses GNU malloc hooks (Linux/macOS SITL) and a compile-time
// override of global operator new for the embedded target.
//
// DO-178C Note: This mechanism is a development/integration safeguard. On a
// certified target, static analysis (Polyspace/Coverity) provides the primary
// compliance evidence. HeapGuard provides runtime defense-in-depth.
//
// Usage:
//   // In main(), after sys.initAll():
//   HeapGuard::arm();
//   sys.executeLoop(1); // any malloc here → abort
// =============================================================================
#include <cstdio>
#include <cstdlib>
#include <atomic>

namespace deltav {

class HeapGuard {
public:
    // Call once, after all Component::init() have run.
    static void arm() {
        armed.store(true, std::memory_order_release);
#if !defined(DELTAV_OS_FREERTOS)
        (void)std::fprintf(stderr, "[HeapGuard] Heap LOCKED — dynamic allocation prohibited.\n");
#endif
    }

    static bool isArmed() {
        return armed.load(std::memory_order_acquire);
    }

    static void disarm() {
        armed.store(false, std::memory_order_release);
    }

    // Called by the overridden operator new (below).
    // On embedded: loops forever (safe-mode equivalent).
    // On SITL: aborts with a diagnostic message.
    static void violation(const char* site = nullptr) {
        (void)std::fprintf(stderr,
            "[HeapGuard] FATAL: Dynamic allocation after heap lock%s%s!\n",
            site ? " at " : "", site ? site : "");
        (void)std::fflush(stderr);
        std::abort();
    }

private:
    static inline std::atomic<bool> armed{false};
};

} // namespace deltav

// ---------------------------------------------------------------------------
// Global operator new / delete overrides
// When HeapGuard is armed, any new/delete pair triggers violation().
// We still provide delete so that static objects (destructed at program exit
// AFTER the guard is disarmed via a destructor) do not falsely fire.
// ---------------------------------------------------------------------------
// Global operator new/delete overrides are implemented in HeapGuard.cpp
// so they are linked exactly once per executable.
