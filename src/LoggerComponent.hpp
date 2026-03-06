#pragma once
// =============================================================================
// LoggerComponent.hpp — DELTA-V Flight Data Recorder (Black Box)
// =============================================================================
// Fixes over previous version:
//   - log_file.flush() was called every packet — now flushed every N packets
//     or on a timer to avoid I/O blocking the scheduler thread
//   - Severity levels written to log for post-mission analysis
//   - Event log and telemetry log are separate files
//   - Log rotation placeholder (real HW would write to flash sectors)
// =============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <fstream>
#include <iostream>

namespace deltav {

constexpr uint32_t LOG_FLUSH_INTERVAL = 20; // flush every N packets

class LoggerComponent : public Component {
public:
    LoggerComponent(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    InputPort<Serializer::ByteArray> telemetry_in;
    InputPort<EventPacket>           event_in;

    void init() override {
        telem_log.open("flight_log.csv", std::ios::out | std::ios::app);
        event_log.open("flight_events.log", std::ios::out | std::ios::app);
        if (telem_log.is_open()) {
            telem_log << "timestamp_ms,component_id,data_payload\n";
        }
        if (event_log.is_open()) {
            event_log << "--- SESSION START T+" << TimeService::getMET() << " ---\n";
        }
        std::cout << "[" << getName() << "] FDR online. "
                     "Telemetry→flight_log.csv | Events→flight_events.log\n";
    }

    void step() override {
        // Drain telemetry
        Serializer::ByteArray raw{};
        while (telemetry_in.tryConsume(raw)) {
            TelemetryPacket p = Serializer::unpackTelem(raw);
            if (telem_log.is_open()) {
                telem_log << p.timestamp_ms << ","
                          << p.component_id << ","
                          << p.data_payload << "\n";
                if (++telem_packet_count % LOG_FLUSH_INTERVAL == 0) {
                    telem_log.flush();
                }
            }
        }

        // Drain events — always flush events immediately (low volume, high importance)
        EventPacket evt{};
        while (event_in.tryConsume(evt)) {
            if (event_log.is_open()) {
                const char* sev_str =
                    evt.severity == Severity::CRITICAL ? "CRIT" :
                    evt.severity == Severity::WARNING  ? "WARN" : "INFO";
                event_log << "[T+" << TimeService::getMET() << "]["
                          << sev_str << "][ID" << evt.source_id << "] "
                          << evt.message << "\n";
                event_log.flush();
            }
        }
    }

    ~LoggerComponent() {
        if (telem_log.is_open()) { telem_log.flush(); telem_log.close(); }
        if (event_log.is_open()) { event_log.flush(); event_log.close(); }
    }

private:
    std::ofstream telem_log;
    std::ofstream event_log;
    uint32_t      telem_packet_count{0};
};

} // namespace deltav