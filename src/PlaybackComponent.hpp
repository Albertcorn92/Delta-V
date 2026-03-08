#pragma once
// =============================================================================
// PlaybackComponent.hpp — DELTA-V Store-and-Forward Telemetry Playback
// =============================================================================
// Civilian operations helper:
// - Loads historical telemetry log samples from local recorder file.
// - Replays samples into TelemHub when commanded.
// - Uses bounded in-memory storage to keep runtime deterministic.
// =============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <array>
#include <cstdint>
#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
#include <cstdio>
#include <fstream>
#include <string>
#endif

namespace deltav {

class PlaybackComponent final : public Component {
public:
    static constexpr size_t MAX_SAMPLES = 2048;
    static constexpr uint32_t MIN_INTERVAL_MS = 20U;

    // target_id: this component
    static constexpr uint32_t OP_LOAD_LOG = 1;
    static constexpr uint32_t OP_START = 2;
    static constexpr uint32_t OP_STOP = 3;
    static constexpr uint32_t OP_REWIND = 4;
    static constexpr uint32_t OP_STEP_ONCE = 5;
    static constexpr uint32_t OP_SET_RATE_HZ = 6;

    InputPort<CommandPacket>          cmd_in{};
    OutputPort<Serializer::ByteArray> telemetry_out{};
    OutputPort<EventPacket>           event_out{};

    PlaybackComponent(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    auto init() -> void override {
        active_ = false;
        cursor_ = 0U;
        sample_count_ = 0U;
        interval_ms_ = 200U;
        last_emit_ms_ = 0U;
        publishEvent(Severity::INFO, "PLAYBACK_READY");
    }

    auto step() -> void override {
        CommandPacket cmd{};
        while (cmd_in.tryConsume(cmd)) {
            handleCommand(cmd);
        }

        if (!active_) {
            return;
        }

        const uint32_t now = TimeService::getMET();
        if ((now - last_emit_ms_) < interval_ms_) {
            return;
        }
        last_emit_ms_ = now;
        emitNext();
    }

private:
    struct LoggedSample {
        uint32_t timestamp_ms{0U};
        uint32_t component_id{0U};
        float value{0.0F};
    };

    std::array<LoggedSample, MAX_SAMPLES> samples_{};
    size_t sample_count_{0U};
    size_t cursor_{0U};
    bool active_{false};
    uint32_t interval_ms_{200U};
    uint32_t last_emit_ms_{0U};

    auto handleCommand(const CommandPacket& cmd) -> void {
        switch (cmd.opcode) {
        case OP_LOAD_LOG:
            if (loadTelemetryLog()) {
                publishEvent(Severity::INFO, "PLAYBACK_LOADED");
            } else {
                publishEvent(Severity::WARNING, "PLAYBACK_LOAD_FAIL");
            }
            break;
        case OP_START:
            active_ = true;
            publishEvent(Severity::INFO, "PLAYBACK_START");
            break;
        case OP_STOP:
            active_ = false;
            publishEvent(Severity::INFO, "PLAYBACK_STOP");
            break;
        case OP_REWIND:
            cursor_ = 0U;
            publishEvent(Severity::INFO, "PLAYBACK_REWIND");
            break;
        case OP_STEP_ONCE:
            emitNext();
            break;
        case OP_SET_RATE_HZ:
            setRateHz(cmd.argument);
            break;
        default:
            recordError();
            publishEvent(Severity::WARNING, "PLAYBACK_BAD_OP");
            break;
        }
    }

    auto setRateHz(float hz) -> void {
        if (hz <= 0.0F) {
            interval_ms_ = 1000U;
        } else {
            const float ms = 1000.0F / hz;
            const uint32_t bounded = static_cast<uint32_t>(ms);
            interval_ms_ = bounded < MIN_INTERVAL_MS ? MIN_INTERVAL_MS : bounded;
        }
        publishEvent(Severity::INFO, "PLAYBACK_RATE");
    }

    auto emitNext() -> void {
        if (sample_count_ == 0U || cursor_ >= sample_count_) {
            active_ = false;
            publishEvent(Severity::INFO, "PLAYBACK_DONE");
            return;
        }
        const auto& s = samples_[cursor_++];
        const TelemetryPacket p{s.timestamp_ms, s.component_id, s.value};
        if (!telemetry_out.send(Serializer::pack(p))) {
            recordError();
            publishEvent(Severity::WARNING, "PLAYBACK_SEND_FAIL");
        }
    }

    auto loadTelemetryLog() -> bool {
        sample_count_ = 0U;
        cursor_ = 0U;
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
        recordError();
        return false;
#else
        std::ifstream in("flight_log.csv");
        if (!in.is_open()) {
            recordError();
            return false;
        }

        std::string line{};
        while (std::getline(in, line) && sample_count_ < MAX_SAMPLES) {
            if (line.empty() || line[0] < '0' || line[0] > '9') {
                continue; // skip header/non-data lines
            }
            unsigned t = 0U;
            unsigned id = 0U;
            float value = 0.0F;
            if (std::sscanf(line.c_str(), "%u,%u,%f", &t, &id, &value) == 3) {
                samples_[sample_count_++] = LoggedSample{
                    static_cast<uint32_t>(t),
                    static_cast<uint32_t>(id),
                    value};
            }
        }
        return sample_count_ > 0U;
#endif
    }

    auto publishEvent(uint32_t severity, const char* msg) -> void {
        (void)event_out.send(EventPacket::create(severity, getId(), msg));
    }
};

} // namespace deltav
