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
#include <cstddef>
#include "Types.hpp"

namespace deltav {

class Serializer {
public:
    // Canonical byte-array aliases
    using TelemBytes   = std::array<uint8_t, sizeof(TelemetryPacket)>; // 12 B
    using CommandBytes = std::array<uint8_t, sizeof(CommandPacket)>;   // 12 B
    using EventBytes   = std::array<uint8_t, sizeof(EventPacket)>;     // 36 B

    // DO-178C: Named constants for CRC algorithm
    static constexpr uint32_t CRC_INITIAL_VAL = 0xFFFFu;
    static constexpr uint32_t CRC_POLY        = 0x1021u;
    static constexpr uint32_t CRC_HIGH_BIT    = 0x8000u;
    static constexpr uint32_t CRC_MASK        = 0xFFFFu;
    static constexpr size_t   BITS_PER_BYTE   = 8;

    // --- TelemetryPacket ---
    [[nodiscard]] static constexpr auto pack(const TelemetryPacket& p) -> TelemBytes {
        return std::bit_cast<TelemBytes>(p);
    }
    [[nodiscard]] static constexpr auto unpackTelem(const TelemBytes& b) -> TelemetryPacket {
        return std::bit_cast<TelemetryPacket>(b);
    }

    // --- CommandPacket ---
    [[nodiscard]] static constexpr auto pack(const CommandPacket& p) -> CommandBytes {
        return std::bit_cast<CommandBytes>(p);
    }
    [[nodiscard]] static constexpr auto unpackCommand(const CommandBytes& b) -> CommandPacket {
        return std::bit_cast<CommandPacket>(b);
    }

    // --- EventPacket ---
    [[nodiscard]] static constexpr auto pack(const EventPacket& p) -> EventBytes {
        return std::bit_cast<EventBytes>(p);
    }
    [[nodiscard]] static constexpr auto unpackEvent(const EventBytes& b) -> EventPacket {
        return std::bit_cast<EventPacket>(b);
    }

    // -----------------------------------------------------------------------
    // CRC-16/CCITT (polynomial 0x1021, init 0xFFFF)
    // Used to protect every downlink frame. Computed over the payload bytes
    // after the CcsdsHeader, before transmission.
    // -----------------------------------------------------------------------
    [[nodiscard]] static constexpr auto crc16(const uint8_t* data, size_t len) -> uint16_t {
        uint32_t crc = CRC_INITIAL_VAL;
        for (size_t i = 0; i < len; ++i) {
            crc ^= static_cast<uint32_t>(data[i]) << BITS_PER_BYTE;
            for (size_t j = 0; j < BITS_PER_BYTE; ++j) {
                // Explicit != 0u check for strict typing
                crc = ((crc & CRC_HIGH_BIT) != 0u) ? ((crc << 1u) ^ CRC_POLY) : (crc << 1u);
                crc &= CRC_MASK; // keep in 16-bit range without narrowing cast
            }
        }
        return static_cast<uint16_t>(crc);
    }

    // Convenience: compute CRC over a fixed-size array
    template<size_t N>
    [[nodiscard]] static constexpr auto crc16(const std::array<uint8_t, N>& data) -> uint16_t {
        return crc16(data.data(), N);
    }

    // Legacy alias — keeps existing code that uses Serializer::ByteArray compiling.
    // New code should use TelemBytes directly.
    using ByteArray = TelemBytes;
};

} // namespace deltav