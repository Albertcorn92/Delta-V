#pragma once
#include <cstdint>
#include <string_view>
#include <thread>
#include <atomic>
#include <chrono>

namespace deltav {

// Defines the standard health states for FDIR monitoring
enum class HealthStatus {
    NOMINAL,
    WARNING,
    CRITICAL_FAILURE
};

// The absolute base class for all flight software blocks (Passive by default)
class Component {
public:
    virtual ~Component() = default;

    // Phase 1: Called once during system boot to lock in memory and ports
    virtual void init() = 0;

    // Phase 2: Called continuously by the framework or its own thread
    virtual void step() = 0;

    // Architectural identity methods
    virtual bool isActive() const { return false; }
    virtual void startThread() {}
    virtual void joinThread() {}

    // FDIR: Called by the Watchdog to check system stability
    virtual HealthStatus reportHealth() {
        return HealthStatus::NOMINAL; 
    }

    constexpr std::string_view getName() const { return name; }
    constexpr uint32_t getId() const { return id; }

protected:
    constexpr Component(std::string_view comp_name, uint32_t comp_id) 
        : name(comp_name), id(comp_id) {}

private:
    std::string_view name;
    uint32_t id;
};

// New Class: ActiveComponent for multi-threaded subsystems
class ActiveComponent : public Component {
public:
    virtual ~ActiveComponent() {
        joinThread();
    }

    bool isActive() const override { return true; }

    void startThread() override {
        if (!is_running) {
            is_running = true;
            worker = std::thread(&ActiveComponent::threadLoop, this);
        }
    }

    void joinThread() override {
        if (is_running) {
            is_running = false;
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

protected:
    // Notice the new target_hz parameter. Active components control their own rate.
    ActiveComponent(std::string_view comp_name, uint32_t comp_id, uint32_t hz)
        : Component(comp_name, comp_id), target_hz(hz) {}

private:
    std::thread worker;
    std::atomic<bool> is_running{false};
    uint32_t target_hz;

    // The dedicated execution loop for this specific subsystem
    void threadLoop() {
        auto tick_duration = std::chrono::milliseconds(1000 / (target_hz > 0 ? target_hz : 1));
        
        while (is_running) {
            auto start_time = std::chrono::steady_clock::now();
            
            this->step(); // Execute subsystem logic

            auto end_time = std::chrono::steady_clock::now();
            auto elapsed = end_time - start_time;
            
            if (elapsed < tick_duration) {
                std::this_thread::sleep_for(tick_duration - elapsed);
            }
        }
    }
};

} // namespace deltav