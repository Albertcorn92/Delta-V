#pragma once
// =============================================================================
// TelemetryBridge.hpp — DELTA-V Ground Link (CCSDS-compliant UDP bridge)
// =============================================================================
// Runs as an ActiveComponent at 20 Hz.
//
// Downlink (flight → ground, port 9001):
//   [CcsdsHeader 10B][Payload NB][CRC-16 2B]
//   APID 10 = Telemetry, APID 20 = Events
//   Sync word: 0x1ACFFC1D (CCSDS standard — was 0xDEADBEEF)
//
// Uplink (ground → flight, port 9002):
//   [CcsdsHeader 10B][CommandPacket 12B]
//   Validated: sync word, APID==30, payload_len==12, sequence monotonicity
//
// DO-178C: No dynamic allocation. Socket is non-blocking to prevent thread stall.
// =============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include <arpa/inet.h>
#include <array>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace deltav {

class TelemetryBridge : public ActiveComponent {
public:
    static constexpr uint16_t GDS_DOWNLINK_PORT = 9001;
    static constexpr uint16_t GDS_UPLINK_PORT   = 9002;

    TelemetryBridge(std::string_view comp_name, uint32_t comp_id)
        : ActiveComponent(comp_name, comp_id, 20) {}

    InputPort<Serializer::ByteArray> telem_in;
    InputPort<EventPacket>           event_in;
    OutputPort<CommandPacket>        command_out;

    // FDIR: count of rejected uplink packets (corrupt / replayed)
    uint32_t getRejectedCount() const { return rejected_count; }

    void init() override {
        sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_fd < 0) {
            std::cerr << "[" << getName() << "] FATAL: socket() failed\n";
            recordError();
            return;
        }
        fcntl(sock_fd, F_SETFL, O_NONBLOCK);

        sockaddr_in bind_addr{};
        bind_addr.sin_family      = AF_INET;
        bind_addr.sin_port        = htons(GDS_UPLINK_PORT);
        bind_addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(sock_fd, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
            std::cerr << "[" << getName() << "] WARN: bind() on port "
                      << GDS_UPLINK_PORT << " failed — uplink disabled\n";
        }

        dest_addr.sin_family      = AF_INET;
        dest_addr.sin_port        = htons(GDS_DOWNLINK_PORT);
        dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        std::cout << "[" << getName() << "] Bridge online. "
                  << "Downlink:" << GDS_DOWNLINK_PORT
                  << " Uplink:" << GDS_UPLINK_PORT
                  << " [CCSDS 0x1ACFFC1D | CRC-16/CCITT]\n";
    }

    void step() override {
        if (sock_fd < 0) { recordError(); return; }
        sendTelemetry();
        sendEvents();
        receiveCommands();
    }

    ~TelemetryBridge() override {
        if (sock_fd >= 0) {
            close(sock_fd);
            sock_fd = -1;
        }
    }

private:
    int          sock_fd{-1};
    sockaddr_in  dest_addr{};
    uint16_t     telem_seq{0};
    uint16_t     event_seq{0};
    uint16_t     last_uplink_seq{0};
    bool         first_uplink{true};
    uint32_t     rejected_count{0};

    // -----------------------------------------------------------------------
    // Build a CCSDS frame: [Header][Payload][CRC-16]
    // -----------------------------------------------------------------------
    template<size_t PayloadSize>
    static std::array<uint8_t, sizeof(CcsdsHeader) + PayloadSize + sizeof(CcsdsCrc)>
    buildFrame(uint16_t apid, uint16_t seq, const uint8_t* payload) {
        constexpr size_t FRAME_SIZE =
            sizeof(CcsdsHeader) + PayloadSize + sizeof(CcsdsCrc);
        std::array<uint8_t, FRAME_SIZE> frame{};

        CcsdsHeader hdr{};
        hdr.sync_word   = CCSDS_SYNC_WORD;
        hdr.apid        = apid;
        hdr.seq_count   = seq;
        hdr.payload_len = static_cast<uint16_t>(PayloadSize);
        std::memcpy(frame.data(), &hdr, sizeof(CcsdsHeader));
        std::memcpy(frame.data() + sizeof(CcsdsHeader), payload, PayloadSize);

        // CRC covers payload only (header is self-synchronising via sync word)
        uint16_t crc = Serializer::crc16(payload, PayloadSize);
        frame[sizeof(CcsdsHeader) + PayloadSize]     = static_cast<uint8_t>(crc >> 8);
        frame[sizeof(CcsdsHeader) + PayloadSize + 1] = static_cast<uint8_t>(crc & 0xFF);
        return frame;
    }

    void sendTelemetry() {
        Serializer::ByteArray data{};
        while (telem_in.tryConsume(data)) {
            auto frame = buildFrame<sizeof(TelemetryPacket)>(
                Apid::TELEMETRY, telem_seq++, data.data());
            sendto(sock_fd, frame.data(), frame.size(), 0,
                reinterpret_cast<const sockaddr*>(&dest_addr), sizeof(dest_addr));
        }
    }

    void sendEvents() {
        EventPacket evt{};
        while (event_in.tryConsume(evt)) {
            auto bytes = Serializer::pack(evt);
            auto frame = buildFrame<sizeof(EventPacket)>(
                Apid::EVENT, event_seq++, bytes.data());
            sendto(sock_fd, frame.data(), frame.size(), 0,
                reinterpret_cast<const sockaddr*>(&dest_addr), sizeof(dest_addr));
        }
    }

    void receiveCommands() {
        // Uplink frame: [CcsdsHeader 10B][CommandPacket 12B]
        constexpr size_t UPLINK_SIZE = sizeof(CcsdsHeader) + sizeof(CommandPacket);
        std::array<uint8_t, 128> buf{};
        sockaddr_in client{};
        socklen_t   client_len = sizeof(client);

        ssize_t len = recvfrom(sock_fd, buf.data(), buf.size(), 0,
            reinterpret_cast<sockaddr*>(&client), &client_len);

        if (len < static_cast<ssize_t>(UPLINK_SIZE)) return;

        const auto* hdr = reinterpret_cast<const CcsdsHeader*>(buf.data());

        // 1. Sync word validation
        if (hdr->sync_word != CCSDS_SYNC_WORD) {
            ++rejected_count;
            return;
        }
        // 2. APID validation
        if (hdr->apid != Apid::COMMAND) {
            ++rejected_count;
            return;
        }
        // 3. Payload length validation
        if (hdr->payload_len != sizeof(CommandPacket)) {
            ++rejected_count;
            return;
        }
        // 4. Sequence monotonicity (reject replay attacks / duplicates)
        if (!first_uplink && hdr->seq_count <= last_uplink_seq) {
            ++rejected_count;
            std::cerr << "[" << getName() << "] Rejected replayed command seq="
                      << hdr->seq_count << "\n";
            return;
        }
        first_uplink     = false;
        last_uplink_seq  = hdr->seq_count;

        CommandPacket cmd{};
        std::memcpy(&cmd, buf.data() + sizeof(CcsdsHeader), sizeof(CommandPacket));
        command_out.send(cmd);
        std::cout << "[" << getName() << "] CMD seq=" << hdr->seq_count
                  << " op=" << cmd.opcode << " -> ID " << cmd.target_id << "\n";
    }
};

} // namespace deltav