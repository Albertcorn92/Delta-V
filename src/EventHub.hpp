#pragma once
// =============================================================================
// EventHub.hpp — DELTA-V Event Aggregator with Fan-Out
// =============================================================================
// Collects EventPackets from all registered sources and broadcasts each event
// to ALL registered listeners simultaneously (fan-out).
//
// v2.0 upgrade: single OutputPort replaced with OutputPort array so events
// reach both the TelemetryBridge (downlink) AND LoggerComponent (FDR)
// independently. Adding a third listener (e.g. SafeMode monitor) requires
// only one registerListener() call — no topology rewiring needed.
//
// Architecture:
//   N InputPort sources → EventHub → M OutputPort listeners (broadcast)
//
// DO-178C: No dynamic allocation. All arrays are fixed-size at compile time.
// =============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Types.hpp"
#include <array>
#include <cstdio>

namespace deltav {

constexpr size_t MAX_EVENT_SOURCES   = 32;
constexpr size_t MAX_EVENT_LISTENERS = 8;

class EventHub : public Component {
public:
    EventHub(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    void init() override {}

    void step() override {
        EventPacket evt{};
        for (size_t i = 0; i < input_count; ++i) {
            while (inputs[i]->tryConsume(evt)) {
                // Broadcast to ALL registered listeners
                for (size_t j = 0; j < listener_count; ++j) {
                    listeners[j].send(evt);
                }
            }
        }
    }

    // Register an event source (component output feeds into hub)
    void registerEventSource(InputPort<EventPacket>* port) {
        if (input_count < MAX_EVENT_SOURCES) {
            inputs[input_count++] = port;
        } else {
            (void)std::fprintf(stderr,
                "[FATAL] EventHub: exceeded MAX_EVENT_SOURCES (%zu). Increase limit.\n",
                static_cast<size_t>(MAX_EVENT_SOURCES));
        }
    }

    // Register a listener that receives every event (fan-out broadcast)
    void registerListener(InputPort<EventPacket>* dest) {
        if (listener_count < MAX_EVENT_LISTENERS) {
            listeners[listener_count].connect(dest);
            ++listener_count;
        } else {
            (void)std::fprintf(stderr,
                "[FATAL] EventHub: exceeded MAX_EVENT_LISTENERS (%zu). Increase limit.\n",
                static_cast<size_t>(MAX_EVENT_LISTENERS));
        }
    }

    size_t getSourceCount()   const { return input_count; }
    size_t getListenerCount() const { return listener_count; }

private:
    std::array<InputPort<EventPacket>*,  MAX_EVENT_SOURCES>   inputs{};
    std::array<OutputPort<EventPacket>, MAX_EVENT_LISTENERS> listeners{};
    size_t input_count{0};
    size_t listener_count{0};
};

} // namespace deltav
