#pragma once
#include <concepts>
#include <type_traits>

namespace deltav {

template<typename T>
concept FlightData = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

template<FlightData T>
class InputPort {
public:
    // High-efficiency "push" mechanism for Hub broadcasting
    void receive(const T& data) {
        _data = data;
        _has_new = true;
    }

    bool hasNew() const { return _has_new; }

    T consume() {
        _has_new = false;
        return _data;
    }

private:
    T _data;
    bool _has_new = false;
};

template<FlightData T>
class OutputPort {
public:
    void connect(InputPort<T>* input) {
        _connected_input = input;
    }

    void send(const T& data) {
        if (_connected_input) {
            _connected_input->receive(data);
        }
    }

private:
    InputPort<T>* _connected_input = nullptr;
};

} // namespace deltav