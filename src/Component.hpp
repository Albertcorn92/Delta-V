#pragma once
#include <cstdint>
#include <string_view>

namespace deltav {

// Defines the standard health states for FDIR monitoring
enum class HealthStatus {
    NOMINAL,
    WARNING,
    CRITICAL_FAILURE
};

// The absolute base class for all flight software blocks
class Component {
public:
    virtual ~Component() = default;

    // Phase 1: Called once during system boot to lock in memory and ports
    virtual void init() = 0;

    // Phase 2: Called continuously by the framework's execution loop
    virtual void step() = 0;

    // FDIR: Called by the Watchdog to check system stability
    virtual HealthStatus reportHealth() {
        return HealthStatus::NOMINAL; // Default assumption is healthy
    }

    // Returns the name of the component for logging and debugging
    constexpr std::string_view getName() const { return name; }
    constexpr uint32_t getId() const { return id; }

protected:
    // Protected constructor: You must inherit from this to build a block
    constexpr Component(std::string_view comp_name, uint32_t comp_id) 
        : name(comp_name), id(comp_id) {}

private:
    std::string_view name;
    uint32_t id;
};

} // namespace deltav