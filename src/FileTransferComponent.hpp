#pragma once
// =============================================================================
// FileTransferComponent.hpp — DELTA-V Chunked File Session Manager
// =============================================================================
// Civilian operations helper:
// - Provides fixed-size chunked uplink/downlink session storage.
// - Uses deterministic bounded buffers for small payload products.
// - CommandPacket path drives control flow; binary chunk ingest APIs are public
//   so teams can bind this component to a UART/SPI radio driver.
// =============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace deltav {

class FileTransferComponent final : public Component {
public:
    static constexpr size_t MAX_FILE_BYTES = 4096;
    static constexpr size_t CHUNK_BYTES = 32;

    // File session command opcodes (target: this component ID)
    static constexpr uint32_t OP_BEGIN_SESSION = 1;    // arg: expected bytes
    static constexpr uint32_t OP_PUSH_TEST_BYTE = 2;   // arg: one byte value [0..255]
    static constexpr uint32_t OP_FINALIZE_SESSION = 3; // arg ignored
    static constexpr uint32_t OP_RESET_SESSION = 4;    // arg ignored

    InputPort<CommandPacket>          cmd_in{};
    OutputPort<Serializer::ByteArray> telemetry_out{};
    OutputPort<EventPacket>           event_out{};

    FileTransferComponent(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    auto init() -> void override {
        resetSession();
        publishEvent(Severity::INFO, "FILE_READY");
    }

    auto step() -> void override {
        CommandPacket cmd{};
        while (cmd_in.tryConsume(cmd)) {
            handleCommand(cmd);
        }

        const TelemetryPacket progress{
            TimeService::getMET(),
            getId(),
            static_cast<float>(received_bytes_),
        };
        (void)sendOrRecordError(telemetry_out, Serializer::pack(progress));
    }

    auto beginSession(size_t expected_bytes) -> bool {
        if (expected_bytes == 0U || expected_bytes > MAX_FILE_BYTES) {
            recordError();
            publishEvent(Severity::WARNING, "FILE_BAD_SIZE");
            return false;
        }
        expected_bytes_ = expected_bytes;
        received_bytes_ = 0U;
        downlink_offset_ = 0U;
        session_active_ = true;
        publishEvent(Severity::INFO, "FILE_SESSION");
        return true;
    }

    auto ingestChunk(const uint8_t* data, size_t len) -> bool {
        if (!session_active_ || data == nullptr || len == 0U || len > CHUNK_BYTES) {
            recordError();
            publishEvent(Severity::WARNING, "FILE_INGEST_BAD");
            return false;
        }
        if (received_bytes_ + len > expected_bytes_) {
            recordError();
            publishEvent(Severity::WARNING, "FILE_EXCEEDS_EXPECTED");
            return false;
        }
        if (received_bytes_ + len > MAX_FILE_BYTES) {
            recordError();
            publishEvent(Severity::WARNING, "FILE_OVERFLOW");
            return false;
        }

        for (size_t i = 0; i < len; ++i) {
            storage_[received_bytes_ + i] = data[i];
        }
        received_bytes_ += len;
        return true;
    }

    auto finalizeSession() -> bool {
        if (!session_active_) {
            recordError();
            publishEvent(Severity::WARNING, "FILE_NOT_ACTIVE");
            return false;
        }
        if (received_bytes_ != expected_bytes_) {
            recordError();
            publishEvent(Severity::WARNING, "FILE_SIZE_MISMATCH");
            return false;
        }
        session_active_ = false;
        publishEvent(Severity::INFO, "FILE_DONE");
        return true;
    }

    auto readNextChunk(uint8_t* out, size_t max_len) -> size_t {
        if (out == nullptr || max_len == 0U || downlink_offset_ >= received_bytes_) {
            return 0U;
        }
        const size_t remain = received_bytes_ - downlink_offset_;
        const size_t n = (remain < max_len) ? remain : max_len;
        for (size_t i = 0; i < n; ++i) {
            out[i] = storage_[downlink_offset_ + i];
        }
        downlink_offset_ += n;
        return n;
    }

    [[nodiscard]] auto receivedBytes() const -> size_t {
        return received_bytes_;
    }

private:
    std::array<uint8_t, MAX_FILE_BYTES> storage_{};
    size_t expected_bytes_{0};
    size_t received_bytes_{0};
    size_t downlink_offset_{0};
    bool session_active_{false};

    auto handleCommand(const CommandPacket& cmd) -> void {
        switch (cmd.opcode) {
        case OP_BEGIN_SESSION: {
            const size_t bytes = clampFloatToBounds(cmd.argument, 1U, MAX_FILE_BYTES);
            (void)beginSession(bytes);
            break;
        }
        case OP_PUSH_TEST_BYTE: {
            const uint8_t v = static_cast<uint8_t>(clampFloatToBounds(cmd.argument, 0U, 255U));
            (void)ingestChunk(&v, 1U);
            break;
        }
        case OP_FINALIZE_SESSION:
            (void)finalizeSession();
            break;
        case OP_RESET_SESSION:
            resetSession();
            publishEvent(Severity::INFO, "FILE_RESET");
            break;
        default:
            recordError();
            publishEvent(Severity::WARNING, "FILE_BAD_OPCODE");
            break;
        }
    }

    auto resetSession() -> void {
        expected_bytes_ = 0U;
        received_bytes_ = 0U;
        downlink_offset_ = 0U;
        session_active_ = false;
    }

    static auto clampFloatToBounds(float value, size_t lo, size_t hi) -> size_t {
        if (!std::isfinite(value)) {
            return lo;
        }
        if (value < static_cast<float>(lo)) {
            return lo;
        }
        if (value > static_cast<float>(hi)) {
            return hi;
        }
        return static_cast<size_t>(value);
    }

    auto publishEvent(uint32_t severity, const char* msg) -> void {
        (void)sendOrRecordError(event_out, EventPacket::create(severity, getId(), msg));
    }
};

} // namespace deltav
