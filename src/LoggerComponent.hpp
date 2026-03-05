#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include <fstream>
#include <iostream>

namespace deltav {

class LoggerComponent : public Component {
public:
    LoggerComponent(std::string_view name, uint32_t id) 
        : Component(name, id) {
        // Constructor no longer needs to initialize a callback
    }

    void init() override {
        log_file.open("flight_log.csv", std::ios::out | std::ios::app);
        if (log_file.is_open()) {
            std::cout << "[" << getName() << "] Flight Data Recorder Initialized. Logging to flight_log.csv" << std::endl;
        }
    }

    // Instead of a callback, we poll the port during the execution step
    void step() override {
        if (telemetry_in.hasNew()) {
            Serializer::ByteArray raw_data = telemetry_in.consume();
            onDataReceived(raw_data);
        }
    }

    InputPort<Serializer::ByteArray> telemetry_in;

private:
    std::ofstream log_file;

    void onDataReceived(const Serializer::ByteArray& data) {
        TelemetryPacket packet = Serializer::unpack(data);
        
        if (log_file.is_open()) {
            log_file << packet.timestamp_ms << "," 
                     << packet.component_id << "," 
                     << packet.data_payload << "\n";
            log_file.flush();
        }

        std::cout << "[" << getName() << "] Logged Packet -> Time: " 
                  << packet.timestamp_ms << "ms | ID: " 
                  << packet.component_id << " | Data: " 
                  << packet.data_payload << std::endl;
    }
};

} // namespace deltav