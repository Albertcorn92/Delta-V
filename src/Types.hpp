#pragma once
#include <cstdint>
#include <cstring>

namespace deltav {

struct TelemetryPacket {
    uint32_t timestamp_ms;
    uint32_t component_id;
    float data_payload;
};

struct CommandPacket {
    uint32_t target_id;
    uint32_t opcode;
    float argument;
};

struct EventPacket {
    uint32_t severity;
    char message[32];

    static EventPacket create(uint32_t sev, const char* msg) {
        EventPacket p;
        p.severity = sev;
        std::strncpy(p.message, msg, 31);
        p.message[31] = '\0';
        return p;
    }
};

} // namespace deltav