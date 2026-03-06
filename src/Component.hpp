#pragma once
// =============================================================================
// Component.hpp — DELTA-V Base Component Classes
// =============================================================================
// Every flight software block inherits from Component (passive) or
// ActiveComponent (threaded). This file owns:
//   - The HealthStatus enum used by the Watchdog FDIR loop
//   - Error counting with thresholds → WARNING / CRITICAL_FAILURE transitions
//   - ActiveComponent using Os::Thread for deterministic absolute-deadline scheduling
//   - No raw std::thread or sleep_for anywhere in this file
// =============================================================================
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string_view>
#include "Os.hpp"

namespace deltav {

// ---------------------------------------------------------------------------
// HealthStatus — FDIR state machine values
// Components transition UP only on explicit recovery, DOWN automatically
// when error thresholds are crossed.
// ---------------------------------------------------------------------------
enum class HealthStatus : uint8_t {
    NOMINAL          = 0,
    WARNING          = 1,  // Degraded — still operating
    CRITICAL_FAILURE = 2,  // Must be restarted or mission enters safe mode
};

// ---------------------------------------------------------------------------
// Component — Passive base class
// Passive components are called by the Scheduler in its master loop.
// ---------------------------------------------------------------------------
class Component {
public:
    virtual ~Component() = default;

    virtual void init() = 0;
    virtual void step() = 0;

    virtual bool        isActive()    const { return false; }
    virtual void        startThread()       {}
    virtual void        joinThread()        {}

    // FDIR interface — Watchdog calls this every supervision cycle.
    virtual HealthStatus reportHealth() {
        // Default: healthy unless error threshold is crossed
        uint32_t errs = error_count.load(std::memory_order_acquire);
        if (errs >= CRITICAL_THRESHOLD) return HealthStatus::CRITICAL_FAILURE;
        if (errs >= WARNING_THRESHOLD)  return HealthStatus::WARNING;
        return HealthStatus::NOMINAL;
    }

    // Called by subclass step() implementations when a recoverable fault occurs.
    void recordError() {
        error_count.fetch_add(1, std::memory_order_acq_rel);
    }

    // Called by Watchdog after successful restart to clear fault state.
    void clearErrors() {
        error_count.store(0, std::memory_order_release);
    }

    uint32_t getErrorCount() const {
        return error_count.load(std::memory_order_acquire);
    }

    constexpr std::string_view getName() const { return name; }
    constexpr uint32_t         getId()   const { return id; }

    // Thresholds — tunable per mission
    static constexpr uint32_t WARNING_THRESHOLD  = 3;
    static constexpr uint32_t CRITICAL_THRESHOLD = 10;

protected:
    constexpr Component(std::string_view comp_name, uint32_t comp_id)
        : name(comp_name), id(comp_id) {}

private:
    std::string_view      name;
    uint32_t              id;
    std::atomic<uint32_t> error_count{0};
};

// ---------------------------------------------------------------------------
// ActiveComponent — Threaded base class
//
// Uses Os::Thread with absolute-deadline scheduling (sleep_until, not sleep_for)
// so rate accuracy does not degrade under load over time.
// ---------------------------------------------------------------------------
class ActiveComponent : public Component {
public:
    virtual ~ActiveComponent() { thread.stop(); }

    bool isActive()    const override { return true; }

    void startThread() override {
        thread.start(target_hz, [this]() { this->step(); });
    }

    void joinThread() override {
        thread.stop();
    }

    bool isThreadRunning() const { return thread.isRunning(); }

protected:
    ActiveComponent(std::string_view comp_name, uint32_t comp_id, uint32_t hz)
        : Component(comp_name, comp_id), target_hz(hz) {}

private:
    Os::Thread thread;
    uint32_t   target_hz;
};

} // namespace deltav