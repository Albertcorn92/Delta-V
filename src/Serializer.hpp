#pragma once
// =============================================================================
// Serializer.hpp — DELTA-V Binary Packet Serialization
// =============================================================================
// Uses std::bit_cast for zero-copy, zero-overhead binary packing.
// All three packet types (Telemetry, Command, Event) are supported.
// CRC-16/CCITT is provided for frame integrity checking on the wire.
//
// DO-178C: All sizeof() proofs live as static_assert in Types.hpp.
//          This file only performs safe, constexpr-proven operations.
// =============================================================================
#include <array>
#include <bit>
#include <cstdint>
#include "Types.hpp"

namespace deltav {

class Serializer {
public:
    // Canonical byte-array aliases
    using TelemBytes   = std::array<uint8_t, sizeof(TelemetryPacket)>; // 12 B
    using CommandBytes = std::array<uint8_t, sizeof(CommandPacket)>;   // 12 B
    using EventBytes   = std::array<uint8_t, sizeof(EventPacket)>;     // 36 B

    // --- TelemetryPacket ---
    static constexpr TelemBytes pack(const TelemetryPacket& p) {
        return std::bit_cast<TelemBytes>(p);
    }
    static constexpr TelemetryPacket unpackTelem(const TelemBytes& b) {
        return std::bit_cast<TelemetryPacket>(b);
    }

    // --- CommandPacket ---
    static constexpr CommandBytes pack(const CommandPacket& p) {
        return std::bit_cast<CommandBytes>(p);
    }
    static constexpr CommandPacket unpackCommand(const CommandBytes& b) {
        return std::bit_cast<CommandPacket>(b);
    }

    // --- EventPacket ---
    static constexpr EventBytes pack(const EventPacket& p) {
        return std::bit_cast<EventBytes>(p);
    }
    static constexpr EventPacket unpackEvent(const EventBytes& b) {
        return std::bit_cast<EventPacket>(b);
    }

    // -----------------------------------------------------------------------
    // CRC-16/CCITT (polynomial 0x1021, init 0xFFFF)
    // Used to protect every downlink frame. Computed over the payload bytes
    // after the CcsdsHeader, before transmission.
    // -----------------------------------------------------------------------
    static constexpr uint16_t crc16(const uint8_t* data, size_t len) {
        uint32_t crc = 0xFFFFu;
        for (size_t i = 0; i < len; ++i) {
            crc ^= static_cast<uint32_t>(data[i]) << 8u;
            for (int j = 0; j < 8; ++j) {
                crc = (crc & 0x8000u) ? ((crc << 1u) ^ 0x1021u) : (crc << 1u);
                crc &= 0xFFFFu; // keep in 16-bit range without narrowing cast
            }
        }
        return static_cast<uint16_t>(crc);
    }

    // Convenience: compute CRC over a fixed-size array
    template<size_t N>
    static constexpr uint16_t crc16(const std::array<uint8_t, N>& data) {
        return crc16(data.data(), N);
    }

    // Legacy alias — keeps existing code that uses Serializer::ByteArray compiling.
    // New code should use TelemBytes directly.
    using ByteArray = TelemBytes;
};

} // namespace deltav