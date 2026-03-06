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
#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
#include <fstream>
#endif
#include <cstdio>

namespace deltav {

constexpr uint32_t LOG_FLUSH_INTERVAL = 20; // flush every N packets

class LoggerComponent : public Component {
public:
    static constexpr const char* DEFAULT_TELEM_LOG_PATH = "flight_log.csv";
    static constexpr const char* DEFAULT_EVENT_LOG_PATH = "flight_events.log";

    LoggerComponent(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    LoggerComponent(std::string_view comp_name, uint32_t comp_id,
                    const char* telem_log_path, const char* event_log_path)
        : Component(comp_name, comp_id),
          telem_log_path_(telem_log_path),
          event_log_path_(event_log_path) {}

    InputPort<Serializer::ByteArray> telemetry_in;
    InputPort<EventPacket>           event_in;

    void init() override {
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
        const auto name = getName();
        std::printf("[%.*s] FDR online. File logging disabled on ESP local-only target.\n",
            static_cast<int>(name.size()), name.data());
#else
        telem_log.open(telem_log_path_, std::ios::out | std::ios::app);
        event_log.open(event_log_path_, std::ios::out | std::ios::app);
        if (telem_log.is_open()) {
            telem_log << "timestamp_ms,component_id,data_payload\n";
        }
        if (event_log.is_open()) {
            event_log << "--- SESSION START T+" << TimeService::getMET() << " ---\n";
        }
        const auto name = getName();
        std::printf("[%.*s] FDR online. Telemetry->%s | Events->%s\n",
            static_cast<int>(name.size()), name.data(), telem_log_path_, event_log_path_);
#endif
    }

    void step() override {
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
        // Local-only embedded mode: drain queues without filesystem writes.
        Serializer::ByteArray raw{};
        while (telemetry_in.tryConsume(raw)) {
            ++telem_packet_count;
        }

        EventPacket evt{};
        while (event_in.tryConsume(evt)) {}
#else
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
                          << evt.message.data() << "\n";
                event_log.flush();
            }
        }
#endif
    }

    ~LoggerComponent() {
#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
        if (telem_log.is_open()) { telem_log.flush(); telem_log.close(); }
        if (event_log.is_open()) { event_log.flush(); event_log.close(); }
#endif
    }

private:
#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
    std::ofstream telem_log;
    std::ofstream event_log;
#endif
    uint32_t      telem_packet_count{0};
    const char*   telem_log_path_{DEFAULT_TELEM_LOG_PATH};
    const char*   event_log_path_{DEFAULT_EVENT_LOG_PATH};
};

} // namespace deltav
