#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include "Types.hpp" // CRITICAL: Serializer needs to know what it's packing!

namespace deltav {

class Serializer {
public:
    // Define ByteArray based on the size of the telemetry packet
    using ByteArray = std::array<uint8_t, sizeof(TelemetryPacket)>;

    static constexpr ByteArray pack(const TelemetryPacket& packet) {
        return std::bit_cast<ByteArray>(packet);
    }

    static constexpr TelemetryPacket unpack(const ByteArray& bytes) {
        return std::bit_cast<TelemetryPacket>(bytes);
    }
};

} // namespace deltav