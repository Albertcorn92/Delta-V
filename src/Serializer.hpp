#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include "Types.hpp" 

namespace deltav {

class Serializer {
public:
    // DO-178C Mathematical Compile-Time Proofs
    static_assert(sizeof(TelemetryPacket) == 12, "FATAL: TelemetryPacket memory alignment broken! Must be exactly 12 bytes.");
    static_assert(sizeof(EventPacket) == 36, "FATAL: EventPacket memory alignment broken! Must be exactly 36 bytes.");
    static_assert(sizeof(CommandPacket) == 12, "FATAL: CommandPacket memory alignment broken! Must be exactly 12 bytes.");

    // Define ByteArray based on the guaranteed 12-byte size of the telemetry packet
    using ByteArray = std::array<uint8_t, sizeof(TelemetryPacket)>;

    static constexpr ByteArray pack(const TelemetryPacket& packet) {
        return std::bit_cast<ByteArray>(packet);
    }

    static constexpr TelemetryPacket unpack(const ByteArray& bytes) {
        return std::bit_cast<TelemetryPacket>(bytes);
    }
};

} // namespace deltav