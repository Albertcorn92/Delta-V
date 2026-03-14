#pragma once
// =============================================================================
// TelemetryBridge.hpp — DELTA-V Ground Link (CCSDS-compliant UDP bridge)
// =============================================================================
// DO-178C Compliant:
// - Copy/Move constructors deleted (Rule of 5 for ActiveComponent)
// - Loop conditions fixed to prevent infinite-loop evaluation
// =============================================================================
#include "Component.hpp"
#include "HeapGuard.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#if defined(DELTAV_DISABLE_NETWORK_STACK) && \
    (defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32))
#define DELTAV_EMBEDDED_NETWORK_DISABLED
#endif
#if !defined(DELTAV_EMBEDDED_NETWORK_DISABLED)
#include <arpa/inet.h>
#endif
#include <array>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
#include <fstream>
#include <termios.h>
#else
#include "nvs.h"
#include "nvs_flash.h"
#endif
#include <cstdio>
#if !defined(DELTAV_EMBEDDED_NETWORK_DISABLED)
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include <string>
#include <unistd.h>
#ifdef DELTAV_FAULT_INJECT
#include <vector>
#endif

namespace deltav {

constexpr size_t MAX_UPLINK_PAYLOAD = 64u;
static_assert(sizeof(CommandPacket) <= MAX_UPLINK_PAYLOAD,
    "CommandPacket exceeds MAX_UPLINK_PAYLOAD — increase the constant");

class TelemetryBridge : public ActiveComponent {
public:
    static constexpr uint16_t DEFAULT_DOWNLINK_PORT = 9001;
    static constexpr uint16_t DEFAULT_UPLINK_PORT   = 9002;
    static constexpr uint32_t TELEMETRY_HZ       = 20;
    static constexpr size_t MAX_CMDS_PER_TICK    = 20;
    static constexpr size_t UPLINK_FRAME_SIZE =
        sizeof(CcsdsHeader) + sizeof(CommandPacket);
    static constexpr const char* ENABLE_UNAUTH_UPLINK_ENV = "DELTAV_ENABLE_UNAUTH_UPLINK";
    static constexpr const char* UPLINK_ALLOW_IP_ENV = "DELTAV_UPLINK_ALLOW_IP";
    static constexpr const char* REPLAY_SEQ_FILE_ENV = "DELTAV_REPLAY_SEQ_FILE";
    static constexpr const char* LINK_MODE_ENV = "DELTAV_LINK_MODE";
    static constexpr const char* SERIAL_PORT_ENV = "DELTAV_SERIAL_PORT";
    static constexpr const char* SERIAL_BAUD_ENV = "DELTAV_SERIAL_BAUD";
    static constexpr const char* DOWNLINK_PORT_ENV = "DELTAV_DOWNLINK_PORT";
    static constexpr const char* UPLINK_PORT_ENV = "DELTAV_UPLINK_PORT";
    static constexpr const char* REPLAY_SEQ_NVS_NAMESPACE = "deltav";
    static constexpr const char* REPLAY_SEQ_NVS_KEY = "replay_seq";
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
    static constexpr const char* DEFAULT_REPLAY_SEQ_FILE = "";
#else
    static constexpr const char* DEFAULT_REPLAY_SEQ_FILE = "replay_seq.db";
    static constexpr const char* DEFAULT_SERIAL_PORT = "/dev/ttyUSB0";
#endif
    static constexpr uint32_t DEFAULT_SERIAL_BAUD = 115200u;
    static constexpr uint8_t KISS_FEND  = 0xC0u;
    static constexpr uint8_t KISS_FESC  = 0xDBu;
    static constexpr uint8_t KISS_TFEND = 0xDCu;
    static constexpr uint8_t KISS_TFESC = 0xDDu;
    static constexpr uint8_t KISS_CMD_DATA = 0x00u;

    enum class LinkMode : uint8_t {
        UDP = 0,
        SERIAL_KISS = 1,
    };

    static constexpr size_t MAX_DOWNLINK_PAYLOAD =
        (sizeof(EventPacket) > sizeof(TelemetryPacket))
            ? sizeof(EventPacket)
            : sizeof(TelemetryPacket);
    static constexpr size_t MAX_DOWNLINK_FRAME_SIZE =
        sizeof(CcsdsHeader) + MAX_DOWNLINK_PAYLOAD + sizeof(CcsdsCrc);
    static constexpr size_t MAX_KISS_WIRE_FRAME =
        2u + 1u + (MAX_DOWNLINK_FRAME_SIZE * 2u);
    static constexpr size_t MAX_KISS_DECODED_FRAME = UPLINK_FRAME_SIZE + 1u;

    // DO-178C: Rule of 5 for thread-owning classes
    TelemetryBridge(const TelemetryBridge&) = delete;
    auto operator=(const TelemetryBridge&) -> TelemetryBridge& = delete;
    TelemetryBridge(TelemetryBridge&&) = delete;
    auto operator=(TelemetryBridge&&) -> TelemetryBridge& = delete;

    /* NOLINTNEXTLINE(bugprone-easily-swappable-parameters) */
    TelemetryBridge(std::string_view comp_name, uint32_t comp_id,
                    uint16_t downlink_port = DEFAULT_DOWNLINK_PORT,
                    uint16_t uplink_port   = DEFAULT_UPLINK_PORT)
        : ActiveComponent(comp_name, comp_id, TELEMETRY_HZ),
          downlink_port_(downlink_port),
          uplink_port_(uplink_port) {
        loadLinkConfiguration();
        loadUplinkConfiguration();
        loadReplayConfiguration();
        loadReplayState();
    }

    ~TelemetryBridge() override {
        if (sock_fd >= 0) {
            close(sock_fd);
            sock_fd = -1;
        }
#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
        if (serial_fd_ >= 0) {
            close(serial_fd_);
            serial_fd_ = -1;
        }
#endif
    }

    // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
    InputPort<Serializer::ByteArray> telem_in{};
    InputPort<EventPacket>           event_in{};
    OutputPort<CommandPacket>        command_out{};
    // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

    [[nodiscard]] auto getRejectedCount() const -> uint32_t { return rejected_count; }
    [[nodiscard]] auto getUplinkPort() const -> uint16_t { return uplink_port_; }
    [[nodiscard]] auto getDownlinkPort() const -> uint16_t { return downlink_port_; }
    [[nodiscard]] auto getLinkMode() const -> LinkMode { return link_mode_; }

#ifdef DELTAV_FAULT_INJECT
    auto ingestUplinkFrameForTest(const uint8_t* data, size_t len) -> bool {
        return processIncomingFrame(data, len);
    }

    auto ingestKissFrameForTest(const uint8_t* data, size_t len) -> bool {
#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
        return processDecodedKissFrame(data, len);
#else
        (void)data;
        (void)len;
        return false;
#endif
    }

    auto ingestUplinkBatchForTest(const std::vector<std::vector<uint8_t>>& frames) -> size_t {
        size_t processed = 0;
        for (const auto& frame : frames) {
            if (processed >= MAX_CMDS_PER_TICK) {
                break;
            }
            (void)processIncomingFrame(frame.data(), frame.size());
            ++processed;
        }
        return processed;
    }

    auto ingestSerialBytesForTest(const uint8_t* data, size_t len) -> size_t {
#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
        if (data == nullptr || len == 0U) {
            return 0U;
        }
        size_t processed = 0U;
        for (size_t i = 0; i < len; ++i) {
            if (processSerialByte(data[i])) {
                ++processed;
            }
        }
        return processed;
#else
        (void)data;
        (void)len;
        return 0U;
#endif
    }
#endif

    auto init() -> void override {
#if defined(DELTAV_LOCAL_ONLY)
        const auto name = getName();
        std::printf("[%.*s] Local-only mode: UDP bridge disabled.\n",
            static_cast<int>(name.size()), name.data());
        return;
#else
        const auto name = getName();
        if (link_mode_ == LinkMode::SERIAL_KISS) {
            if (!initSerialLink()) {
                recordError();
                return;
            }
            std::printf("[%.*s] Bridge online. Downlink/Uplink serial-KISS @ %s (%u baud)\n",
                static_cast<int>(name.size()), name.data(),
                serial_port_.c_str(), static_cast<unsigned>(serial_baud_));
        } else {
#if defined(DELTAV_EMBEDDED_NETWORK_DISABLED)
            (void)std::fprintf(stderr,
                "[%.*s] FATAL: UDP bridge unavailable (network stack disabled).\n",
                static_cast<int>(name.size()), name.data());
            recordError();
            return;
#else
            if (!initUdpLink()) {
                recordError();
                return;
            }
            std::printf(
                "[%.*s] Bridge online. Downlink:%u Uplink:%u [CCSDS 0x1ACFFC1D | strict frame + anti-replay]\n",
                static_cast<int>(name.size()), name.data(),
                static_cast<unsigned>(downlink_port_), static_cast<unsigned>(uplink_port_));
            if (!uplink_enabled_) {
                std::printf(
                    "[%.*s] Uplink command ingest DISABLED by default. "
                    "Set DELTAV_ENABLE_UNAUTH_UPLINK=1 for local SITL command tests.\n",
                    static_cast<int>(name.size()), name.data());
            }
#endif
        }
#endif
    }

    auto step() -> void override {
#if defined(DELTAV_LOCAL_ONLY)
        // In strict local-only mode, drain and discard link traffic to keep the
        // internal queues healthy without opening sockets.
        Serializer::ByteArray data{};
        while (telem_in.tryConsume(data)) {}
        EventPacket evt{};
        while (event_in.tryConsume(evt)) {}
        return;
#else
        if (link_mode_ == LinkMode::SERIAL_KISS) {
#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
            if (serial_fd_ < 0) { recordError(); return; }
#endif
        } else {
#if defined(DELTAV_EMBEDDED_NETWORK_DISABLED)
            recordError();
            return;
#else
            if (sock_fd < 0) {
                recordError();
                return;
            }
#endif
        }
        sendTelemetry();
        sendEvents();
        receiveCommands();
#endif
    }

private:
    enum class SerialParseState : uint8_t {
        IDLE = 0,
        IN_FRAME = 1,
    };

    int          sock_fd{-1};
#if !defined(DELTAV_EMBEDDED_NETWORK_DISABLED)
    sockaddr_in  dest_addr{};
#endif
    uint16_t     telem_seq{0};
    uint16_t     event_seq{0};
    uint16_t     downlink_port_{DEFAULT_DOWNLINK_PORT};
    uint16_t     uplink_port_{DEFAULT_UPLINK_PORT};
    LinkMode     link_mode_{LinkMode::UDP};

    uint32_t     last_uplink_seq{0};
    bool         first_uplink{true};
    uint32_t     rejected_count{0};
    std::string  replay_seq_path{DEFAULT_REPLAY_SEQ_FILE};
    bool         uplink_enabled_{true};
#if !defined(DELTAV_EMBEDDED_NETWORK_DISABLED)
    uint32_t     allowed_uplink_addr_{INADDR_LOOPBACK};
#else
    uint32_t     allowed_uplink_addr_{0U};
#endif
#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
    int          serial_fd_{-1};
    std::string  serial_port_{DEFAULT_SERIAL_PORT};
    uint32_t     serial_baud_{DEFAULT_SERIAL_BAUD};
    SerialParseState serial_parse_state_{SerialParseState::IDLE};
    bool         serial_escape_{false};
    std::array<uint8_t, MAX_KISS_DECODED_FRAME> serial_frame_buf_{};
    size_t       serial_frame_len_{0};
#endif
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
    bool         replay_store_init_attempted_{false};
    bool         replay_store_ready_{false};
#endif

    auto maybeOverrideDefaultPortFromEnv(
        const char* env_name,
        uint16_t default_port,
        uint16_t& current_port) -> void {
        if (current_port != default_port) {
            return;
        }

        const char* port_env = std::getenv(env_name);
        if (port_env == nullptr || port_env[0] == '\0') {
            return;
        }

        char* end = nullptr;
        const unsigned long parsed = std::strtoul(port_env, &end, 10);
        if (end != nullptr && *end == '\0' && parsed > 0UL && parsed <= 65535UL) {
            current_port = static_cast<uint16_t>(parsed);
            return;
        }

        const auto name = getName();
        (void)std::fprintf(
            stderr,
            "[%.*s] WARN: invalid %s=%s; keeping %u\n",
            static_cast<int>(name.size()), name.data(),
            env_name, port_env, static_cast<unsigned>(default_port));
        recordError();
    }

    auto loadLinkConfiguration() -> void {
        maybeOverrideDefaultPortFromEnv(
            DOWNLINK_PORT_ENV, DEFAULT_DOWNLINK_PORT, downlink_port_);
        maybeOverrideDefaultPortFromEnv(
            UPLINK_PORT_ENV, DEFAULT_UPLINK_PORT, uplink_port_);
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
        link_mode_ = LinkMode::UDP;
#else
        const char* mode_env = std::getenv(LINK_MODE_ENV);
        if (mode_env != nullptr && std::strcmp(mode_env, "serial_kiss") == 0) {
            link_mode_ = LinkMode::SERIAL_KISS;
        } else {
            link_mode_ = LinkMode::UDP;
        }

        const char* serial_port_env = std::getenv(SERIAL_PORT_ENV);
        if (serial_port_env != nullptr && serial_port_env[0] != '\0') {
            serial_port_ = serial_port_env;
        }

        const char* baud_env = std::getenv(SERIAL_BAUD_ENV);
        if (baud_env != nullptr && baud_env[0] != '\0') {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(baud_env, &end, 10);
            if (end != nullptr && *end == '\0' && parsed > 0UL && parsed <= 2000000UL) {
                serial_baud_ = static_cast<uint32_t>(parsed);
            } else {
                const auto name = getName();
                (void)std::fprintf(
                    stderr,
                    "[%.*s] WARN: invalid DELTAV_SERIAL_BAUD=%s; using %u\n",
                    static_cast<int>(name.size()), name.data(), baud_env,
                    static_cast<unsigned>(DEFAULT_SERIAL_BAUD));
                recordError();
                serial_baud_ = DEFAULT_SERIAL_BAUD;
            }
        }
#endif
    }

    auto initUdpLink() -> bool {
#if defined(DELTAV_EMBEDDED_NETWORK_DISABLED)
        return false;
#else
        sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_fd < 0) {
            const auto name = getName();
            (void)std::fprintf(stderr, "[%.*s] FATAL: socket() failed\n",
                static_cast<int>(name.size()), name.data());
            return false;
        }
        (void)fcntl(sock_fd, F_SETFL, O_NONBLOCK);

        sockaddr_in bind_addr{};
        bind_addr.sin_family      = AF_INET;
        bind_addr.sin_port        = htons(uplink_port_);
        bind_addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(sock_fd, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
            const auto name = getName();
            (void)std::fprintf(stderr, "[%.*s] WARN: bind() on port %u failed - uplink disabled\n",
                static_cast<int>(name.size()), name.data(),
                static_cast<unsigned>(uplink_port_));
        }

        dest_addr.sin_family      = AF_INET;
        dest_addr.sin_port        = htons(downlink_port_);
        dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        return true;
#endif
    }

#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
    static auto mapSerialBaud(uint32_t baud) -> speed_t {
        switch (baud) {
        case 9600u: return B9600;
        case 19200u: return B19200;
        case 38400u: return B38400;
        case 57600u: return B57600;
        case 115200u: return B115200;
#ifdef B230400
        case 230400u: return B230400;
#endif
#ifdef B460800
        case 460800u: return B460800;
#endif
#ifdef B921600
        case 921600u: return B921600;
#endif
        default:
            return B115200;
        }
    }

    auto initSerialLink() -> bool {
        serial_fd_ = open(serial_port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (serial_fd_ < 0) {
            const auto name = getName();
            (void)std::fprintf(stderr, "[%.*s] FATAL: open(%s) failed: %s\n",
                static_cast<int>(name.size()), name.data(),
                serial_port_.c_str(), std::strerror(errno));
            return false;
        }

        termios tty{};
        if (tcgetattr(serial_fd_, &tty) != 0) {
            const auto name = getName();
            (void)std::fprintf(stderr, "[%.*s] FATAL: tcgetattr(%s) failed: %s\n",
                static_cast<int>(name.size()), name.data(),
                serial_port_.c_str(), std::strerror(errno));
            close(serial_fd_);
            serial_fd_ = -1;
            return false;
        }

        cfmakeraw(&tty);
        const speed_t speed = mapSerialBaud(serial_baud_);
        (void)cfsetispeed(&tty, speed);
        (void)cfsetospeed(&tty, speed);
        tty.c_cflag |= CLOCAL | CREAD;
#ifdef CRTSCTS
        tty.c_cflag &= static_cast<tcflag_t>(~static_cast<tcflag_t>(CRTSCTS));
#endif
        tty.c_cflag &= static_cast<tcflag_t>(~static_cast<tcflag_t>(CSTOPB));
        tty.c_cflag &= static_cast<tcflag_t>(~static_cast<tcflag_t>(PARENB));
        tty.c_cflag &= static_cast<tcflag_t>(~static_cast<tcflag_t>(CSIZE));
        tty.c_cflag |= CS8;
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 0;

        if (tcsetattr(serial_fd_, TCSANOW, &tty) != 0) {
            const auto name = getName();
            (void)std::fprintf(stderr, "[%.*s] FATAL: tcsetattr(%s) failed: %s\n",
                static_cast<int>(name.size()), name.data(),
                serial_port_.c_str(), std::strerror(errno));
            close(serial_fd_);
            serial_fd_ = -1;
            return false;
        }
        return true;
    }
#else
    auto initSerialLink() -> bool {
        return false;
    }
#endif

    auto loadUplinkConfiguration() -> void {
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
        uplink_enabled_ = true;
#else
        if (link_mode_ == LinkMode::SERIAL_KISS) {
            uplink_enabled_ = true;
        } else {
            const char* enable_env = std::getenv(ENABLE_UNAUTH_UPLINK_ENV);
            uplink_enabled_ = enable_env != nullptr && std::strcmp(enable_env, "1") == 0;
        }

        const char* allow_ip_env = std::getenv(UPLINK_ALLOW_IP_ENV);
        if (allow_ip_env != nullptr && allow_ip_env[0] != '\0') {
            in_addr parsed{};
            if (inet_aton(allow_ip_env, &parsed) != 0) {
                allowed_uplink_addr_ = parsed.s_addr;
            } else {
                const auto name = getName();
                (void)std::fprintf(
                    stderr,
                    "[%.*s] WARN: invalid DELTAV_UPLINK_ALLOW_IP=%s; keeping default 127.0.0.1\n",
                    static_cast<int>(name.size()), name.data(), allow_ip_env);
                recordError();
            }
        }
#endif
    }

#if !defined(DELTAV_EMBEDDED_NETWORK_DISABLED)
    [[nodiscard]] auto isTrustedUplinkSource(const sockaddr_in& client) const -> bool {
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
        (void)client;
        return true;
#else
        return client.sin_addr.s_addr == allowed_uplink_addr_;
#endif
    }
#endif

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
    auto ensureReplayStoreReadyEsp() -> bool {
        if (replay_store_init_attempted_) {
            return replay_store_ready_;
        }
        replay_store_init_attempted_ = true;

        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            const esp_err_t erase_err = nvs_flash_erase();
            if (erase_err == ESP_OK) {
                err = nvs_flash_init();
            } else {
                err = erase_err;
            }
        }

        replay_store_ready_ = (err == ESP_OK);
        if (!replay_store_ready_) {
            const auto name = getName();
            (void)std::fprintf(
                stderr,
                "[%.*s] WARN: replay NVS init failed (err=%d)\n",
                static_cast<int>(name.size()), name.data(), static_cast<int>(err));
            recordError();
        }
        return replay_store_ready_;
    }
#endif

    auto loadReplayConfiguration() -> void {
        const char* replay_path_env = std::getenv(REPLAY_SEQ_FILE_ENV);
        if (replay_path_env != nullptr && replay_path_env[0] != '\0') {
            replay_seq_path = replay_path_env;
        }
    }

    auto loadReplayState() -> void {
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
        if (!ensureReplayStoreReadyEsp()) {
            return;
        }

        nvs_handle_t handle{};
        if (nvs_open(REPLAY_SEQ_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
            return;
        }

        uint16_t stored_seq = 0;
        const esp_err_t get_err = nvs_get_u16(handle, REPLAY_SEQ_NVS_KEY, &stored_seq);
        if (get_err == ESP_OK) {
            last_uplink_seq = stored_seq;
            first_uplink = false;
        }
        nvs_close(handle);
        return;
#else
        if (replay_seq_path.empty()) {
            return;
        }

        std::ifstream replay_file(replay_seq_path);
        if (!replay_file.is_open()) {
            return;
        }

        uint32_t stored_seq = 0;
        if (replay_file >> stored_seq) {
            if (stored_seq <= 0xFFFFu) {
                last_uplink_seq = stored_seq;
                first_uplink = false;
            } else {
                const auto name = getName();
                (void)std::fprintf(stderr, "[%.*s] WARN: replay sequence out of range; ignoring state.\n",
                    static_cast<int>(name.size()), name.data());
                recordError();
            }
        }
#endif
    }

    auto persistReplayState() -> void {
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
        if (!ensureReplayStoreReadyEsp()) {
            return;
        }

        nvs_handle_t handle{};
        esp_err_t err = nvs_open(REPLAY_SEQ_NVS_NAMESPACE, NVS_READWRITE, &handle);
        if (err != ESP_OK) {
            recordError();
            return;
        }

        err = nvs_set_u16(handle, REPLAY_SEQ_NVS_KEY, static_cast<uint16_t>(last_uplink_seq));
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
        if (err != ESP_OK) {
            recordError();
        }
        nvs_close(handle);
        return;
#else
        // Host heap lock intentionally disallows post-start dynamic allocation.
        // Replay protection still works in-memory when persistence is disabled.
        if (HeapGuard::isArmed()) {
            return;
        }

        if (replay_seq_path.empty()) {
            return;
        }

        std::ofstream replay_file(replay_seq_path, std::ios::trunc);
        if (!replay_file.is_open()) {
            const auto name = getName();
            (void)std::fprintf(stderr, "[%.*s] WARN: cannot persist replay state to %s\n",
                static_cast<int>(name.size()), name.data(), replay_seq_path.c_str());
            recordError();
            return;
        }
        replay_file << last_uplink_seq << "\n";
        if (!replay_file.good()) {
            recordError();
        }
#endif
    }

    template<size_t PayloadSize>
    /* NOLINTNEXTLINE(bugprone-easily-swappable-parameters) */
    static auto buildFrame(uint16_t apid, uint16_t seq, const uint8_t* payload) 
        -> std::array<uint8_t, sizeof(CcsdsHeader) + PayloadSize + sizeof(CcsdsCrc)> {
        
        constexpr size_t FRAME_SIZE = sizeof(CcsdsHeader) + PayloadSize + sizeof(CcsdsCrc);
        std::array<uint8_t, FRAME_SIZE> frame{};

        CcsdsHeader hdr{};
        hdr.sync_word   = CCSDS_SYNC_WORD;
        hdr.apid        = apid;
        hdr.seq_count   = seq;
        hdr.payload_len = static_cast<uint16_t>(PayloadSize);
        std::memcpy(frame.data(), &hdr, sizeof(CcsdsHeader));
        std::memcpy(frame.data() + sizeof(CcsdsHeader), payload, PayloadSize);

        uint16_t crc = Serializer::crc16(payload, PayloadSize);
        frame.at(sizeof(CcsdsHeader) + PayloadSize)     = static_cast<uint8_t>(crc >> 8);
        frame.at(sizeof(CcsdsHeader) + PayloadSize + 1) = static_cast<uint8_t>(crc & 0xFF);
        return frame;
    }

    auto sendTelemetry() -> void {
        Serializer::ByteArray data{};
        while (telem_in.tryConsume(data)) {
            auto frame = buildFrame<sizeof(TelemetryPacket)>(
                Apid::TELEMETRY, telem_seq++, data.data());
            if (!sendFrame(frame.data(), frame.size())) {
                recordError();
            }
        }
    }

    auto sendEvents() -> void {
        EventPacket evt{};
        while (event_in.tryConsume(evt)) {
            auto bytes = Serializer::pack(evt);
            auto frame = buildFrame<sizeof(EventPacket)>(
                Apid::EVENT, event_seq++, bytes.data());
            if (!sendFrame(frame.data(), frame.size())) {
                recordError();
            }
        }
    }

    auto sendFrame(const uint8_t* frame, size_t frame_len) -> bool {
        if (frame == nullptr || frame_len == 0U) {
            return false;
        }

        if (link_mode_ == LinkMode::SERIAL_KISS) {
#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
            if (serial_fd_ < 0) {
                return false;
            }
            std::array<uint8_t, MAX_KISS_WIRE_FRAME> wire{};
            const size_t wire_len = encodeKissFrame(frame, frame_len, wire.data(), wire.size());
            if (wire_len == 0U) {
                return false;
            }
            const ssize_t written = write(serial_fd_, wire.data(), wire_len);
            return written == static_cast<ssize_t>(wire_len);
#else
            return false;
#endif
        }

#if defined(DELTAV_EMBEDDED_NETWORK_DISABLED)
        return false;
#else
        const ssize_t bytes_sent = sendto(sock_fd, frame, frame_len, 0,
            reinterpret_cast<const sockaddr*>(&dest_addr), sizeof(dest_addr));
        return bytes_sent == static_cast<ssize_t>(frame_len);
#endif
    }

#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
    static auto pushKissEscaped(
        uint8_t byte,
        uint8_t* out,
        size_t out_cap,
        size_t& out_len) -> bool {
        if (byte == KISS_FEND) {
            if (out_len + 2U > out_cap) {
                return false;
            }
            out[out_len++] = KISS_FESC;
            out[out_len++] = KISS_TFEND;
            return true;
        }
        if (byte == KISS_FESC) {
            if (out_len + 2U > out_cap) {
                return false;
            }
            out[out_len++] = KISS_FESC;
            out[out_len++] = KISS_TFESC;
            return true;
        }
        if (out_len + 1U > out_cap) {
            return false;
        }
        out[out_len++] = byte;
        return true;
    }

    static auto encodeKissFrame(
        const uint8_t* frame,
        size_t frame_len,
        uint8_t* out,
        size_t out_cap) -> size_t {
        if (frame == nullptr || out == nullptr || frame_len == 0U || out_cap < 3U) {
            return 0U;
        }

        size_t out_len = 0U;
        out[out_len++] = KISS_FEND;
        out[out_len++] = KISS_CMD_DATA;
        for (size_t i = 0; i < frame_len; ++i) {
            if (!pushKissEscaped(frame[i], out, out_cap, out_len)) {
                return 0U;
            }
        }
        if (out_len + 1U > out_cap) {
            return 0U;
        }
        out[out_len++] = KISS_FEND;
        return out_len;
    }

    auto processDecodedKissFrame(const uint8_t* decoded, size_t decoded_len) -> bool {
        if (decoded == nullptr || decoded_len < 2U) {
            ++rejected_count;
            return false;
        }
        if ((decoded[0] & 0x0FU) != KISS_CMD_DATA) {
            ++rejected_count;
            return false;
        }
        return processIncomingFrame(decoded + 1U, decoded_len - 1U);
    }

    auto processSerialByte(uint8_t byte) -> bool {
        if (byte == KISS_FEND) {
            if (serial_parse_state_ == SerialParseState::IN_FRAME && serial_frame_len_ > 0U) {
                (void)processDecodedKissFrame(serial_frame_buf_.data(), serial_frame_len_);
                serial_frame_len_ = 0U;
                serial_escape_ = false;
                serial_parse_state_ = SerialParseState::IN_FRAME;
                return true;
            }
            serial_frame_len_ = 0U;
            serial_escape_ = false;
            serial_parse_state_ = SerialParseState::IN_FRAME;
            return false;
        }

        if (serial_parse_state_ != SerialParseState::IN_FRAME) {
            return false;
        }

        uint8_t value = byte;
        if (serial_escape_) {
            if (byte == KISS_TFEND) {
                value = KISS_FEND;
            } else if (byte == KISS_TFESC) {
                value = KISS_FESC;
            } else {
                ++rejected_count;
                recordError();
                serial_parse_state_ = SerialParseState::IDLE;
                serial_frame_len_ = 0U;
                serial_escape_ = false;
                return false;
            }
            serial_escape_ = false;
        } else if (byte == KISS_FESC) {
            serial_escape_ = true;
            return false;
        }

        if (serial_frame_len_ >= serial_frame_buf_.size()) {
            ++rejected_count;
            recordError();
            serial_parse_state_ = SerialParseState::IDLE;
            serial_frame_len_ = 0U;
            serial_escape_ = false;
            return false;
        }
        serial_frame_buf_[serial_frame_len_++] = value;
        return false;
    }
#endif

    auto receiveCommands() -> void {
        if (!uplink_enabled_) {
            return;
        }

        if (link_mode_ == LinkMode::SERIAL_KISS) {
#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
            receiveCommandsSerial();
#endif
            return;
        }

#if defined(DELTAV_EMBEDDED_NETWORK_DISABLED)
        return;
#else
        receiveCommandsUdp();
#endif
    }

#if !defined(DELTAV_EMBEDDED_NETWORK_DISABLED)
    auto receiveCommandsUdp() -> void {
        constexpr size_t BUF_SIZE = sizeof(CcsdsHeader) + MAX_UPLINK_PAYLOAD;
        std::array<uint8_t, BUF_SIZE> buf{};

        sockaddr_in client{};
        socklen_t   client_len = sizeof(client);

        size_t processed = 0;

        while (processed < MAX_CMDS_PER_TICK) {
            client_len = sizeof(client);
            ssize_t len = recvfrom(sock_fd, buf.data(), buf.size(), 0,
                reinterpret_cast<sockaddr*>(&client), &client_len);

            if (len <= 0) break; 
            if (!isTrustedUplinkSource(client)) {
                ++rejected_count;
                recordError();
                continue;
            }
            processed++;
            (void)processIncomingFrame(buf.data(), static_cast<size_t>(len));
        }
    }
#endif

#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
    auto receiveCommandsSerial() -> void {
        if (serial_fd_ < 0) {
            return;
        }

        std::array<uint8_t, 128> rx{};
        size_t processed = 0U;
        while (processed < MAX_CMDS_PER_TICK) {
            const ssize_t len = read(serial_fd_, rx.data(), rx.size());
            if (len <= 0) {
                break;
            }
            for (ssize_t i = 0; i < len; ++i) {
                if (processSerialByte(rx[static_cast<size_t>(i)])) {
                    ++processed;
                    if (processed >= MAX_CMDS_PER_TICK) {
                        break;
                    }
                }
            }
        }
    }
#endif

    auto processIncomingFrame(const uint8_t* data, size_t len) -> bool {
        if (len != UPLINK_FRAME_SIZE) {
            ++rejected_count;
            return false;
        }

        CcsdsHeader hdr{};
        std::memcpy(&hdr, data, sizeof(hdr));
        if (hdr.sync_word != CCSDS_SYNC_WORD) {
            ++rejected_count;
            return false;
        }
        if (hdr.apid != Apid::COMMAND) {
            ++rejected_count;
            return false;
        }
        if (hdr.payload_len != sizeof(CommandPacket)) {
            ++rejected_count;
            return false;
        }

        const uint32_t new_seq = hdr.seq_count;
        constexpr uint32_t SEQ_WRAP_HIGH = 0xFF00u;
        constexpr uint32_t SEQ_WRAP_LOW  = 0x0100u;
        const bool wrapped = (last_uplink_seq > SEQ_WRAP_HIGH) && (new_seq < SEQ_WRAP_LOW);

        if (!first_uplink && !wrapped && new_seq <= last_uplink_seq) {
            ++rejected_count;
            const auto name = getName();
            (void)std::fprintf(stderr, "[%.*s] Rejected replayed command seq=%u\n",
                static_cast<int>(name.size()), name.data(), static_cast<unsigned>(new_seq));
            return false;
        }

        CommandPacket cmd{};
        std::memcpy(&cmd, data + sizeof(CcsdsHeader), sizeof(CommandPacket));
        if (!command_out.send(cmd)) {
            ++rejected_count;
            recordError();
            const auto name = getName();
            (void)std::fprintf(stderr, "[%.*s] WARN: command dispatch failed seq=%u\n",
                static_cast<int>(name.size()), name.data(), static_cast<unsigned>(new_seq));
            return false;
        }

        first_uplink = false;
        last_uplink_seq = new_seq;
        persistReplayState();
        const auto name = getName();
        std::printf("[%.*s] CMD seq=%u op=%u -> ID %u\n",
            static_cast<int>(name.size()), name.data(),
            static_cast<unsigned>(new_seq),
            static_cast<unsigned>(cmd.opcode),
            static_cast<unsigned>(cmd.target_id));
        return true;
    }
};

} // namespace deltav

#if defined(DELTAV_EMBEDDED_NETWORK_DISABLED)
#undef DELTAV_EMBEDDED_NETWORK_DISABLED
#endif
