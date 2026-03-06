#pragma once
// =============================================================================
// TelemetryBridge.hpp — DELTA-V Ground Link (CCSDS-compliant UDP bridge)
// =============================================================================
// Runs as an ActiveComponent at 20 Hz.
//
// Downlink (flight → ground, port 9001):
//   [CcsdsHeader 10B][Payload NB][CRC-16 2B]
//   APID 10 = Telemetry, APID 20 = Events
//
// Uplink (ground → flight, port 9002):
//   [CcsdsHeader 10B][CommandPacket 12B][SipHash-2-4 MAC 8B]
//   Validated: sync word, APID==30, payload_len==12, sequence monotonicity,
//              and cryptographic signature (SipHash).
//
// Fixes applied (v2.2):
//   F-03: last_uplink_seq promoted to uint32_t for wrap-around detection.
//   F-10: receiveCommands() drains the OS socket buffer in a loop.
//   F-19: Uplink buffer sized with static_assert guards.
//   F-20: Added SipHash-2-4 cryptographic authentication and anti-jamming limits.
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

namespace crypto {
    // -----------------------------------------------------------------------
    // SipHash-2-4: Cryptographically secure MAC for short inputs
    // -----------------------------------------------------------------------
    inline uint64_t rotl(uint64_t x, int b) { return (x << b) | (x >> (64 - b)); }
    inline void sipround(uint64_t& v0, uint64_t& v1, uint64_t& v2, uint64_t& v3) {
        v0 += v1; v1 = rotl(v1, 13); v1 ^= v0; v0 = rotl(v0, 32);
        v2 += v3; v3 = rotl(v3, 16); v3 ^= v2;
        v0 += v3; v3 = rotl(v3, 21); v3 ^= v0;
        v2 += v1; v1 = rotl(v1, 17); v1 ^= v2; v2 = rotl(v2, 32);
    }
    inline uint64_t siphash24(const uint8_t* key, const uint8_t* data, size_t len) {
        uint64_t k0, k1;
        std::memcpy(&k0, key, 8);
        std::memcpy(&k1, key + 8, 8);
        uint64_t v0 = k0 ^ 0x736f6d6570736575ULL;
        uint64_t v1 = k1 ^ 0x646f72616e646f6dULL;
        uint64_t v2 = k0 ^ 0x6c7967656e657261ULL;
        uint64_t v3 = k1 ^ 0x7465646279746573ULL;
        uint64_t b = static_cast<uint64_t>(len) << 56;
        const uint8_t* m = data;
        
        // Use static_cast to prevent -Wsign-conversion warnings on bitwise literals
        const uint8_t* end = m + (len & ~static_cast<size_t>(7));
        for (; m != end; m += 8) {
            uint64_t mi; std::memcpy(&mi, m, 8);
            v3 ^= mi;
            sipround(v0, v1, v2, v3); sipround(v0, v1, v2, v3);
            v0 ^= mi;
        }
        uint64_t t = 0;
        for (size_t i = 0; i < (len & static_cast<size_t>(7)); ++i) {
            t |= static_cast<uint64_t>(m[i]) << (static_cast<size_t>(8) * i);
        }
        b |= t;
        v3 ^= b;
        sipround(v0, v1, v2, v3); sipround(v0, v1, v2, v3);
        v0 ^= b;
        v2 ^= 0xff;
        sipround(v0, v1, v2, v3); sipround(v0, v1, v2, v3);
        sipround(v0, v1, v2, v3); sipround(v0, v1, v2, v3);
        return v0 ^ v1 ^ v2 ^ v3;
    }
} // namespace crypto

constexpr size_t MAX_UPLINK_PAYLOAD = 64u;
static_assert(sizeof(CommandPacket) <= MAX_UPLINK_PAYLOAD,
    "CommandPacket exceeds MAX_UPLINK_PAYLOAD — increase the constant");

class TelemetryBridge : public ActiveComponent {
public:
    static constexpr uint16_t GDS_DOWNLINK_PORT = 9001;
    static constexpr uint16_t GDS_UPLINK_PORT   = 9002;

    TelemetryBridge(std::string_view comp_name, uint32_t comp_id)
        : ActiveComponent(comp_name, comp_id, 20) {}

    InputPort<Serializer::ByteArray> telem_in;
    InputPort<EventPacket>           event_in;
    OutputPort<CommandPacket>        command_out;

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
                  << " Uplink:"  << GDS_UPLINK_PORT
                  << " [CCSDS 0x1ACFFC1D | SipHash Auth]\n";
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

    uint32_t     last_uplink_seq{0};
    bool         first_uplink{true};
    uint32_t     rejected_count{0};

    // 128-bit Pre-Shared Key for Command Authentication
    static constexpr uint8_t UPLINK_KEY[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };

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
        constexpr size_t BUF_SIZE = sizeof(CcsdsHeader) + MAX_UPLINK_PAYLOAD + 8; // +8 for MAC
        std::array<uint8_t, BUF_SIZE> buf{};

        sockaddr_in client{};
        socklen_t   client_len = sizeof(client);

        size_t processed = 0;
        constexpr size_t MAX_CMDS_PER_TICK = 20; // Anti-jamming flood limit

        while (processed < MAX_CMDS_PER_TICK) {
            ssize_t len = recvfrom(sock_fd, buf.data(), buf.size(), 0,
                reinterpret_cast<sockaddr*>(&client), &client_len);

            if (len <= 0) break; // EAGAIN
            processed++;

            constexpr size_t SIGNED_PAYLOAD_LEN = sizeof(CcsdsHeader) + sizeof(CommandPacket);
            constexpr size_t MIN_UPLINK = SIGNED_PAYLOAD_LEN + 8; // Header + Cmd + 8B MAC

            if (static_cast<size_t>(len) < MIN_UPLINK) {
                ++rejected_count;
                continue;
            }

            const auto* hdr = reinterpret_cast<const CcsdsHeader*>(buf.data());

            // 1. Structural Validation
            if (hdr->sync_word != CCSDS_SYNC_WORD) { ++rejected_count; continue; }
            if (hdr->apid != Apid::COMMAND) { ++rejected_count; continue; }
            if (hdr->payload_len != sizeof(CommandPacket)) { ++rejected_count; continue; }

            // 2. Cryptographic Authentication (SipHash-2-4)
            uint64_t received_mac;
            std::memcpy(&received_mac, buf.data() + SIGNED_PAYLOAD_LEN, 8);
            uint64_t expected_mac = crypto::siphash24(UPLINK_KEY, buf.data(), SIGNED_PAYLOAD_LEN);

            if (received_mac != expected_mac) {
                ++rejected_count;
                std::cerr << "[" << getName() << "] SECURITY FAULT: Invalid command signature!\n";
                continue;
            }

            // 3. Replay Protection
            const uint32_t new_seq  = hdr->seq_count;
            const bool     wrapped  = (last_uplink_seq > 0xFF00u) && (new_seq < 0x0100u);
            if (!first_uplink && !wrapped && new_seq <= last_uplink_seq) {
                ++rejected_count;
                std::cerr << "[" << getName() << "] Rejected replayed command seq=" << new_seq << "\n";
                continue;
            }
            first_uplink    = false;
            last_uplink_seq = new_seq;

            // 4. Dispatch Command
            CommandPacket cmd{};
            std::memcpy(&cmd, buf.data() + sizeof(CcsdsHeader), sizeof(CommandPacket));
            command_out.send(cmd);
            std::cout << "[" << getName() << "] CMD seq=" << new_seq
                      << " op=" << cmd.opcode << " -> ID " << cmd.target_id << "\n";
        }
    }
};

} // namespace deltav