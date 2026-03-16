#pragma once
// =============================================================================
// CfdpComponent.hpp — DELTA-V CFDP-Style Chunk Reliability Manager
// =============================================================================
// Civilian operations helper:
// - Tracks received file chunks and missing-chunk set.
// - Supports selective retransmission workflows (request missing indexes).
// - Fixed-size chunk table for deterministic operation on embedded targets.
// =============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace deltav {

class CfdpComponent final : public Component {
public:
    static constexpr size_t MAX_CHUNKS = 128U;
    static constexpr size_t CHUNK_BYTES = 64U;

    // target_id: this component
    static constexpr uint32_t OP_BEGIN_SESSION = 1;
    static constexpr uint32_t OP_SET_CHUNK_INDEX = 2;
    static constexpr uint32_t OP_PUSH_TEST_BYTE = 3;
    static constexpr uint32_t OP_COMMIT_CHUNK = 4;
    static constexpr uint32_t OP_REPORT_MISSING_COUNT = 5;
    static constexpr uint32_t OP_REPORT_NEXT_MISSING = 6;
    static constexpr uint32_t OP_COMPLETE_SESSION = 7;
    static constexpr uint32_t OP_RESET_SESSION = 8;

    InputPort<CommandPacket>          cmd_in{};
    OutputPort<Serializer::ByteArray> telemetry_out{};
    OutputPort<EventPacket>           event_out{};

    CfdpComponent(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    auto init() -> void override {
        resetSession();
        publishEvent(Severity::INFO, "CFDP_READY");
    }

    auto step() -> void override {
        CommandPacket cmd{};
        while (cmd_in.tryConsume(cmd)) {
            handleCommand(cmd);
        }

        const TelemetryPacket tlm{
            TimeService::getMET(),
            getId(),
            static_cast<float>(missingCount()),
        };
        (void)sendOrRecordError(telemetry_out, Serializer::pack(tlm));
    }

    [[nodiscard]] auto missingCount() const -> size_t {
        if (!session_active_) {
            return 0U;
        }
        size_t missing = 0U;
        for (size_t i = 0; i < expected_chunks_; ++i) {
            if (!chunk_received_[i]) {
                ++missing;
            }
        }
        return missing;
    }

private:
    std::array<std::array<uint8_t, CHUNK_BYTES>, MAX_CHUNKS> chunks_{};
    std::array<uint8_t, MAX_CHUNKS> chunk_len_{};
    std::array<bool, MAX_CHUNKS> chunk_received_{};
    std::array<uint8_t, CHUNK_BYTES> staged_chunk_{};

    size_t expected_chunks_{0U};
    size_t staged_index_{0U};
    size_t staged_len_{0U};
    bool session_active_{false};

    auto handleCommand(const CommandPacket& cmd) -> void {
        switch (cmd.opcode) {
        case OP_BEGIN_SESSION:
            beginSession(toIndex(cmd.argument));
            break;
        case OP_SET_CHUNK_INDEX:
            staged_index_ = toIndex(cmd.argument);
            break;
        case OP_PUSH_TEST_BYTE:
            pushTestByte(static_cast<uint8_t>(toIndex(cmd.argument) & 0xFFU));
            break;
        case OP_COMMIT_CHUNK:
            commitChunk();
            break;
        case OP_REPORT_MISSING_COUNT:
            reportValue(static_cast<float>(missingCount()));
            break;
        case OP_REPORT_NEXT_MISSING:
            reportValue(static_cast<float>(nextMissingIndex()));
            break;
        case OP_COMPLETE_SESSION:
            completeSession();
            break;
        case OP_RESET_SESSION:
            resetSession();
            publishEvent(Severity::INFO, "CFDP_RESET");
            break;
        default:
            recordError();
            publishEvent(Severity::WARNING, "CFDP_BAD_OP");
            break;
        }
    }

    auto beginSession(size_t chunks) -> void {
        if (chunks == 0U || chunks > MAX_CHUNKS) {
            recordError();
            publishEvent(Severity::WARNING, "CFDP_BAD_SIZE");
            return;
        }
        expected_chunks_ = chunks;
        for (size_t i = 0; i < MAX_CHUNKS; ++i) {
            chunk_received_[i] = false;
            chunk_len_[i] = 0U;
        }
        staged_index_ = 0U;
        staged_len_ = 0U;
        session_active_ = true;
        publishEvent(Severity::INFO, "CFDP_SESSION");
    }

    auto pushTestByte(uint8_t value) -> void {
        if (!session_active_ || staged_len_ >= CHUNK_BYTES) {
            recordError();
            return;
        }
        staged_chunk_[staged_len_++] = value;
    }

    auto commitChunk() -> void {
        if (!session_active_ || staged_index_ >= expected_chunks_) {
            recordError();
            publishEvent(Severity::WARNING, "CFDP_IDX_FAIL");
            return;
        }
        for (size_t i = 0; i < staged_len_; ++i) {
            chunks_[staged_index_][i] = staged_chunk_[i];
        }
        chunk_len_[staged_index_] = static_cast<uint8_t>(staged_len_);
        chunk_received_[staged_index_] = true;
        staged_len_ = 0U;
        publishEvent(Severity::INFO, "CFDP_CHUNK");
    }

    auto completeSession() -> void {
        if (!session_active_) {
            return;
        }
        if (missingCount() == 0U) {
            publishEvent(Severity::INFO, "CFDP_DONE");
        } else {
            publishEvent(Severity::WARNING, "CFDP_MISSING");
        }
    }

    [[nodiscard]] auto nextMissingIndex() const -> int {
        if (!session_active_) {
            return -1;
        }
        for (size_t i = 0; i < expected_chunks_; ++i) {
            if (!chunk_received_[i]) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    auto resetSession() -> void {
        expected_chunks_ = 0U;
        staged_index_ = 0U;
        staged_len_ = 0U;
        session_active_ = false;
        for (size_t i = 0; i < MAX_CHUNKS; ++i) {
            chunk_received_[i] = false;
            chunk_len_[i] = 0U;
        }
    }

    static auto toIndex(float value) -> size_t {
        if (value <= 0.0F) {
            return 0U;
        }
        if (value >= static_cast<float>(MAX_CHUNKS)) {
            return MAX_CHUNKS;
        }
        return static_cast<size_t>(value);
    }

    auto reportValue(float v) -> void {
        const TelemetryPacket tlm{TimeService::getMET(), getId(), v};
        (void)sendOrRecordError(telemetry_out, Serializer::pack(tlm));
    }

    auto publishEvent(uint32_t severity, const char* msg) -> void {
        (void)sendOrRecordError(event_out, EventPacket::create(severity, getId(), msg));
    }
};

} // namespace deltav
