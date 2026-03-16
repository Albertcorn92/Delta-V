#pragma once
// =============================================================================
// OtaComponent.hpp — DELTA-V OTA Session Manager (CRC32-verified)
// =============================================================================
// Civilian operations helper:
// - Receives staged update bytes from command path or future radio chunk bridge.
// - Verifies full image with CRC32.
// - Stages candidate artifact and raises reboot request event for supervisors.
// =============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
#include <fstream>
#endif

namespace deltav {

class OtaComponent final : public Component {
public:
    static constexpr size_t MAX_IMAGE_BYTES = 65536U;
    static constexpr const char* OTA_ARTIFACT_PATH_ENV = "DELTAV_OTA_ARTIFACT_FILE";
    static constexpr const char* DEFAULT_OTA_ARTIFACT_PATH = "ota_candidate.bin";
    static constexpr const char* OTA_MANIFEST_PATH_ENV = "DELTAV_OTA_MANIFEST_FILE";
    static constexpr const char* DEFAULT_OTA_MANIFEST_PATH = "ota_candidate.meta";

    // target_id: this component
    static constexpr uint32_t OP_BEGIN_SESSION = 1;
    static constexpr uint32_t OP_PUSH_TEST_BYTE = 2;
    static constexpr uint32_t OP_SET_EXPECTED_CRC_HI16 = 3;
    static constexpr uint32_t OP_SET_EXPECTED_CRC_LO16 = 4;
    static constexpr uint32_t OP_VERIFY_IMAGE = 5;
    static constexpr uint32_t OP_STAGE_ACTIVATE = 6;
    static constexpr uint32_t OP_RESET_SESSION = 7;

    InputPort<CommandPacket>          cmd_in{};
    OutputPort<Serializer::ByteArray> telemetry_out{};
    OutputPort<EventPacket>           event_out{};

    OtaComponent(std::string_view comp_name, uint32_t comp_id)
        : Component(comp_name, comp_id) {}

    auto init() -> void override {
        resetSession();
        publishEvent(Severity::INFO, "OTA_READY");
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

    auto ingestChunk(const uint8_t* data, size_t len) -> bool {
        if (!session_active_ || data == nullptr || len == 0U) {
            recordError();
            return false;
        }
        if ((received_bytes_ + len) > MAX_IMAGE_BYTES) {
            recordError();
            return false;
        }
        for (size_t i = 0; i < len; ++i) {
            image_[received_bytes_ + i] = data[i];
        }
        received_bytes_ += len;
        return true;
    }

    [[nodiscard]] auto isRebootRequested() const -> bool {
        return reboot_requested_;
    }

private:
    std::array<uint8_t, MAX_IMAGE_BYTES> image_{};
    size_t expected_bytes_{0U};
    size_t received_bytes_{0U};
    uint32_t expected_crc_{0U};
    uint16_t crc_hi16_{0U};
    uint16_t crc_lo16_{0U};
    bool session_active_{false};
    bool image_verified_{false};
    bool reboot_requested_{false};

    static auto crc32(const uint8_t* data, size_t len) -> uint32_t {
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; ++i) {
            crc ^= data[i];
            for (int b = 0; b < 8; ++b) {
                const uint32_t mask = static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1U)));
                crc = (crc >> 1U) ^ (0xEDB88320U & mask);
            }
        }
        return ~crc;
    }

    auto handleCommand(const CommandPacket& cmd) -> void {
        switch (cmd.opcode) {
        case OP_BEGIN_SESSION:
            beginSession(static_cast<size_t>(sanitizeByteCount(cmd.argument)));
            break;
        case OP_PUSH_TEST_BYTE: {
            const uint8_t v = static_cast<uint8_t>(sanitizeU16(cmd.argument) & 0xFFU);
            (void)ingestChunk(&v, 1U);
            break;
        }
        case OP_SET_EXPECTED_CRC_HI16:
            crc_hi16_ = sanitizeU16(cmd.argument);
            expected_crc_ = (static_cast<uint32_t>(crc_hi16_) << 16U) | crc_lo16_;
            publishEvent(Severity::INFO, "OTA_CRC_HI");
            break;
        case OP_SET_EXPECTED_CRC_LO16:
            crc_lo16_ = sanitizeU16(cmd.argument);
            expected_crc_ = (static_cast<uint32_t>(crc_hi16_) << 16U) | crc_lo16_;
            publishEvent(Severity::INFO, "OTA_CRC_LO");
            break;
        case OP_VERIFY_IMAGE:
            verifyImage();
            break;
        case OP_STAGE_ACTIVATE:
            stageActivate();
            break;
        case OP_RESET_SESSION:
            resetSession();
            publishEvent(Severity::INFO, "OTA_RESET");
            break;
        default:
            recordError();
            publishEvent(Severity::WARNING, "OTA_BAD_OPCODE");
            break;
        }
    }

    auto beginSession(size_t expected) -> void {
        if (expected == 0U || expected > MAX_IMAGE_BYTES) {
            recordError();
            publishEvent(Severity::WARNING, "OTA_BAD_SIZE");
            return;
        }
        expected_bytes_ = expected;
        received_bytes_ = 0U;
        image_verified_ = false;
        reboot_requested_ = false;
        session_active_ = true;
        publishEvent(Severity::INFO, "OTA_SESSION");
    }

    auto verifyImage() -> void {
        if (!session_active_ || received_bytes_ == 0U) {
            recordError();
            publishEvent(Severity::WARNING, "OTA_VERIFY_FAIL");
            return;
        }
        if (expected_bytes_ != 0U && received_bytes_ != expected_bytes_) {
            recordError();
            publishEvent(Severity::WARNING, "OTA_SIZE_MISMATCH");
            image_verified_ = false;
            return;
        }
        const uint32_t actual_crc = crc32(image_.data(), received_bytes_);
        image_verified_ = (actual_crc == expected_crc_);
        if (image_verified_) {
            publishEvent(Severity::INFO, "OTA_VERIFIED");
        } else {
            recordError();
            publishEvent(Severity::WARNING, "OTA_CRC_FAIL");
        }
    }

    auto stageActivate() -> void {
        if (!image_verified_) {
            recordError();
            publishEvent(Severity::WARNING, "OTA_NOT_VERIFIED");
            return;
        }

#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
        const char* artifact_path = std::getenv(OTA_ARTIFACT_PATH_ENV);
        if (artifact_path == nullptr || artifact_path[0] == '\0') {
            artifact_path = DEFAULT_OTA_ARTIFACT_PATH;
        }
        std::ofstream out(artifact_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            recordError();
            publishEvent(Severity::WARNING, "OTA_STAGE_FAIL");
            return;
        }
        out.write(reinterpret_cast<const char*>(image_.data()),
            static_cast<std::streamsize>(received_bytes_));
        if (!out.good()) {
            recordError();
            publishEvent(Severity::WARNING, "OTA_STAGE_FAIL");
            return;
        }

        const char* manifest_path = std::getenv(OTA_MANIFEST_PATH_ENV);
        if (manifest_path == nullptr || manifest_path[0] == '\0') {
            manifest_path = DEFAULT_OTA_MANIFEST_PATH;
        }
        std::ofstream manifest(manifest_path, std::ios::trunc);
        if (!manifest.is_open()) {
            recordError();
            publishEvent(Severity::WARNING, "OTA_STAGE_FAIL");
            return;
        }
        manifest << "bytes=" << received_bytes_ << "\n";
        manifest << "crc32=0x" << std::hex << expected_crc_ << std::dec << "\n";
        if (!manifest.good()) {
            recordError();
            publishEvent(Severity::WARNING, "OTA_STAGE_FAIL");
            return;
        }
#endif
        reboot_requested_ = true;
        publishEvent(Severity::INFO, "OTA_REBOOT_REQ");
    }

    auto resetSession() -> void {
        expected_bytes_ = 0U;
        received_bytes_ = 0U;
        expected_crc_ = 0U;
        crc_hi16_ = 0U;
        crc_lo16_ = 0U;
        session_active_ = false;
        image_verified_ = false;
        reboot_requested_ = false;
    }

    static auto sanitizeU16(float value) -> uint16_t {
        if (value < 0.0F) {
            return 0U;
        }
        if (value > 65535.0F) {
            return 65535U;
        }
        return static_cast<uint16_t>(value);
    }

    static auto sanitizeByteCount(float value) -> uint32_t {
        if (value < 0.0F) {
            return 0U;
        }
        if (value > static_cast<float>(MAX_IMAGE_BYTES)) {
            return static_cast<uint32_t>(MAX_IMAGE_BYTES);
        }
        return static_cast<uint32_t>(value);
    }

    auto publishEvent(uint32_t severity, const char* msg) -> void {
        (void)sendOrRecordError(event_out, EventPacket::create(severity, getId(), msg));
    }
};

} // namespace deltav
