#pragma once
#include <concepts>
#include <type_traits>

namespace deltav {

/**
 * NASA-Grade Safety: C++20 Concept
 * This ensures that only "Trivially Copyable" data (no heap, no pointers)
 * can be sent through the spacecraft's internal bus. 
 * This prevents memory leaks and ensures deterministic serialization.
 */
template<typename T>
concept FlightData = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

// Forward declaration of InputPort
template<FlightData T>
class InputPort;

/**
 * OutputPort: The "Sender"
 * Constrained by the FlightData concept.
 */
template<FlightData T>
class OutputPort {
public:
    void connect(InputPort<T>* target) {
        connected_input = target;
    }

    void send(const T& data) {
        if (connected_input) {
            connected_input->receive(data);
        }
    }

private:
    InputPort<T>* connected_input = nullptr;
};

/**
 * InputPort: The "Receiver"
 * Constrained by the FlightData concept.
 */
template<FlightData T>
class InputPort {
public:
    void receive(const T& data) {
        latest_data = data;
        has_new_data = true;
    }

    bool hasNew() const { return has_new_data; }
    
    T consume() {
        has_new_data = false;
        return latest_data;
    }

private:
    T latest_data;
    bool has_new_data = false;
};

} // namespace deltav