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
#include <arpa/inet.h>
#include <array>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32)
#include <fstream>
#endif
#include <cstdio>
#include <netinet/in.h>
#include <sys/socket.h>
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
    static constexpr const char* REPLAY_SEQ_FILE_ENV = "DELTAV_REPLAY_SEQ_FILE";
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
    static constexpr const char* DEFAULT_REPLAY_SEQ_FILE = "";
#else
    static constexpr const char* DEFAULT_REPLAY_SEQ_FILE = "replay_seq.db";
#endif

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
        loadReplayConfiguration();
        loadReplayState();
    }

    ~TelemetryBridge() override {
        if (sock_fd >= 0) {
            close(sock_fd);
            sock_fd = -1;
        }
    }

    // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
    InputPort<Serializer::ByteArray> telem_in{};
    InputPort<EventPacket>           event_in{};
    OutputPort<CommandPacket>        command_out{};
    // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

    [[nodiscard]] auto getRejectedCount() const -> uint32_t { return rejected_count; }
    [[nodiscard]] auto getUplinkPort() const -> uint16_t { return uplink_port_; }
    [[nodiscard]] auto getDownlinkPort() const -> uint16_t { return downlink_port_; }

#ifdef DELTAV_FAULT_INJECT
    auto ingestUplinkFrameForTest(const uint8_t* data, size_t len) -> bool {
        return processIncomingFrame(data, len);
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
#endif

    auto init() -> void override {
#if defined(DELTAV_LOCAL_ONLY)
        const auto name = getName();
        std::printf("[%.*s] Local-only mode: UDP bridge disabled.\n",
            static_cast<int>(name.size()), name.data());
        return;
#else
        sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_fd < 0) {
            const auto name = getName();
            (void)std::fprintf(stderr, "[%.*s] FATAL: socket() failed\n",
                static_cast<int>(name.size()), name.data());
            recordError();
            return;
        }
        fcntl(sock_fd, F_SETFL, O_NONBLOCK);

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

        const auto name = getName();
        std::printf("[%.*s] Bridge online. Downlink:%u Uplink:%u [CCSDS 0x1ACFFC1D | strict frame + anti-replay]\n",
            static_cast<int>(name.size()), name.data(),
            static_cast<unsigned>(downlink_port_), static_cast<unsigned>(uplink_port_));
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
        if (sock_fd < 0) { recordError(); return; }
        sendTelemetry();
        sendEvents();
        receiveCommands();
#endif
    }

private:
    int          sock_fd{-1};
    sockaddr_in  dest_addr{};
    uint16_t     telem_seq{0};
    uint16_t     event_seq{0};
    uint16_t     downlink_port_{DEFAULT_DOWNLINK_PORT};
    uint16_t     uplink_port_{DEFAULT_UPLINK_PORT};

    uint32_t     last_uplink_seq{0};
    bool         first_uplink{true};
    uint32_t     rejected_count{0};
    std::string  replay_seq_path{DEFAULT_REPLAY_SEQ_FILE};

    auto loadReplayConfiguration() -> void {
        const char* replay_path_env = std::getenv(REPLAY_SEQ_FILE_ENV);
        if (replay_path_env != nullptr && replay_path_env[0] != '\0') {
            replay_seq_path = replay_path_env;
        }
    }

    auto loadReplayState() -> void {
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
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
            const ssize_t bytes_sent = sendto(sock_fd, frame.data(), frame.size(), 0,
                reinterpret_cast<const sockaddr*>(&dest_addr), sizeof(dest_addr));
            if (bytes_sent != static_cast<ssize_t>(frame.size())) {
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
            const ssize_t bytes_sent = sendto(sock_fd, frame.data(), frame.size(), 0,
                reinterpret_cast<const sockaddr*>(&dest_addr), sizeof(dest_addr));
            if (bytes_sent != static_cast<ssize_t>(frame.size())) {
                recordError();
            }
        }
    }

    auto receiveCommands() -> void {
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
            processed++;
            (void)processIncomingFrame(buf.data(), static_cast<size_t>(len));
        }
    }

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
