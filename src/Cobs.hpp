#pragma once
// =============================================================================
// Cobs.hpp — Consistent Overhead Byte Stuffing (COBS) Framing
// =============================================================================
// COBS ensures that 0x00 appears ONLY as a frame delimiter, never inside the
// payload. This allows a receiver to always find valid frame boundaries even
// after a stream desynchronisation or partial packet loss.
//
// Why COBS over raw CCSDS?
//   The CCSDS sync word (0x1ACFFC1D) can occur inside payload data by chance.
//   COBS eliminates this ambiguity: the only 0x00 byte in the stream IS the
//   delimiter. A receiver that loses sync simply reads until the next 0x00.
//
// Encoding overhead: at most 1 byte per 254 payload bytes (< 0.4%).
//
// References:
//   - Cheshire & Baker, "Consistent Overhead Byte Stuffing", 1999
//   - ECSS-E-ST-70-41C (used for fault-tolerant uplink framing)
//
// DO-178C: No dynamic allocation. Fixed-size static encode/decode buffers.
//          All functions return error codes (never throw).
// =============================================================================
#include <cstdint>
#include <cstddef>
#include <array>

namespace deltav {
namespace cobs {

// ---------------------------------------------------------------------------
// Maximum encoded size for a given payload length.
// COBS adds at most 1 overhead byte per 254 payload bytes, plus a delimiter.
// ---------------------------------------------------------------------------
constexpr size_t encodedMaxSize(size_t payload_len) {
    return payload_len + (payload_len / 254) + 2; // +1 prefix +1 delimiter(0x00)
}

// ---------------------------------------------------------------------------
// encode() — Encode payload bytes into a COBS frame.
//
// Parameters:
//   input     : pointer to raw payload
//   input_len : number of payload bytes
//   output    : destination buffer (must be >= encodedMaxSize(input_len))
//   output_max: capacity of output buffer
//
// Returns: number of bytes written (including the 0x00 delimiter),
//          or 0 on error (output too small).
// ---------------------------------------------------------------------------
inline size_t encode(const uint8_t* input, size_t input_len,
                     uint8_t* output, size_t output_max) {
    if (output_max < encodedMaxSize(input_len)) return 0;

    size_t write_pos    = 0;
    size_t code_pos     = write_pos; // position of the current overhead byte
    uint8_t code        = 1;
    output[write_pos++] = 0xFF;      // placeholder

    for (size_t i = 0; i < input_len; ++i) {
        if (input[i] != 0x00) {
            output[write_pos++] = input[i];
            ++code;
            if (code == 0xFF) {
                output[code_pos] = code;
                code_pos = write_pos;
                output[write_pos++] = 0xFF;
                code = 1;
            }
        } else {
            output[code_pos] = code;
            code_pos = write_pos;
            output[write_pos++] = 0xFF;
            code = 1;
        }
    }
    output[code_pos]    = code;
    output[write_pos++] = 0x00; // frame delimiter
    return write_pos;
}

// ---------------------------------------------------------------------------
// decode() — Decode a COBS frame.
//
// Parameters:
//   input     : COBS-encoded bytes (not including the trailing 0x00 delimiter)
//   input_len : number of encoded bytes
//   output    : destination buffer (must be >= input_len)
//   output_max: capacity of output buffer
//
// Returns: number of decoded bytes, or 0 on framing error.
// ---------------------------------------------------------------------------
inline size_t decode(const uint8_t* input, size_t input_len,
                     uint8_t* output, size_t output_max) {
    if (input_len == 0) return 0;

    size_t read_pos  = 0;
    size_t write_pos = 0;

    while (read_pos < input_len) {
        uint8_t code = input[read_pos++];
        if (code == 0x00) return 0; // framing error — 0x00 inside encoded data

        for (uint8_t i = 1; i < code; ++i) {
            if (read_pos >= input_len) return 0; // truncated
            if (write_pos >= output_max) return 0; // output overflow
            output[write_pos++] = input[read_pos++];
        }
        if (code < 0xFF && read_pos < input_len) {
            if (write_pos >= output_max) return 0;
            output[write_pos++] = 0x00;
        }
    }
    return write_pos;
}

// ---------------------------------------------------------------------------
// Convenience: Fixed-size frame types for DELTA-V packet sizes.
// ---------------------------------------------------------------------------
constexpr size_t TELEM_FRAME_MAX  = encodedMaxSize(22);  // Header(10)+Telem(12)
constexpr size_t EVENT_FRAME_MAX  = encodedMaxSize(48);  // Header(10)+Event(36)+CRC(2)
constexpr size_t CMD_FRAME_MAX    = encodedMaxSize(22);  // Header(10)+Cmd(12)

using TelemFrame = std::array<uint8_t, TELEM_FRAME_MAX>;
using EventFrame = std::array<uint8_t, EVENT_FRAME_MAX>;
using CmdFrame   = std::array<uint8_t, CMD_FRAME_MAX>;

struct CobsFrame {
    std::array<uint8_t, CMD_FRAME_MAX> data{};
    size_t encoded_len{0};

    static auto encode(const uint8_t* payload, size_t payload_len) -> CobsFrame {
        CobsFrame frame{};
        frame.encoded_len = cobs::encode(payload, payload_len, frame.data.data(), frame.data.size());
        return frame;
    }

    static auto decode(const CobsFrame& frame, uint8_t* output, size_t output_max, size_t& decoded_len) -> bool {
        if (frame.encoded_len == 0 || frame.encoded_len > frame.data.size()) {
            decoded_len = 0;
            return false;
        }

        if (frame.data.at(frame.encoded_len - 1) != 0x00) {
            decoded_len = 0;
            return false;
        }

        decoded_len = cobs::decode(frame.data.data(), frame.encoded_len - 1, output, output_max);
        return decoded_len > 0 || (frame.encoded_len == 2 && frame.data.at(0) == 0x01);
    }
};

} // namespace cobs
} // namespace deltav
