#include "Cobs.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TelemetryBridge.hpp"
#include "TimeService.hpp"
#include "Types.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

using namespace deltav;

namespace {

using Clock = std::chrono::steady_clock;
using Seconds = std::chrono::duration<double>;

struct UplinkMetrics {
    size_t frames{0};
    size_t accepted{0};
    size_t consumed{0};
    double duration_s{0.0};
    double throughput_cmd_s{0.0};
    double latency_p50_us{0.0};
    double latency_p95_us{0.0};
};

struct ThroughputMetrics {
    size_t iterations{0};
    size_t bytes_per_iteration{0};
    double duration_s{0.0};
    double throughput_mb_s{0.0};
    uint64_t accumulator{0};
};

class ScopedStdioSilence {
public:
    ScopedStdioSilence() {
        null_fd_ = ::open("/dev/null", O_WRONLY);
        if (null_fd_ < 0) {
            return;
        }

        saved_stdout_ = ::dup(STDOUT_FILENO);
        saved_stderr_ = ::dup(STDERR_FILENO);
        if (saved_stdout_ < 0 || saved_stderr_ < 0) {
            closeSaved();
            return;
        }

        if (::dup2(null_fd_, STDOUT_FILENO) < 0 || ::dup2(null_fd_, STDERR_FILENO) < 0) {
            closeSaved();
            return;
        }

        active_ = true;
    }

    ~ScopedStdioSilence() {
        if (!active_) {
            closeSaved();
            return;
        }

        (void)::fflush(stdout);
        (void)::fflush(stderr);
        (void)::dup2(saved_stdout_, STDOUT_FILENO);
        (void)::dup2(saved_stderr_, STDERR_FILENO);
        closeSaved();
    }

    ScopedStdioSilence(const ScopedStdioSilence&) = delete;
    auto operator=(const ScopedStdioSilence&) -> ScopedStdioSilence& = delete;

private:
    int saved_stdout_{-1};
    int saved_stderr_{-1};
    int null_fd_{-1};
    bool active_{false};

    auto closeSaved() -> void {
        if (saved_stdout_ >= 0) {
            (void)::close(saved_stdout_);
            saved_stdout_ = -1;
        }
        if (saved_stderr_ >= 0) {
            (void)::close(saved_stderr_);
            saved_stderr_ = -1;
        }
        if (null_fd_ >= 0) {
            (void)::close(null_fd_);
            null_fd_ = -1;
        }
        active_ = false;
    }
};

auto percentile_us(std::vector<double> samples, double percentile) -> double {
    if (samples.empty()) {
        return 0.0;
    }
    std::sort(samples.begin(), samples.end());
    const double clamped = std::clamp(percentile, 0.0, 1.0);
    const size_t idx = static_cast<size_t>(clamped * static_cast<double>(samples.size() - 1u));
    return samples.at(idx);
}

auto build_uplink_frame(
    uint16_t seq,
    const CommandPacket& cmd) -> std::array<uint8_t, sizeof(CcsdsHeader) + sizeof(CommandPacket)> {
    std::array<uint8_t, sizeof(CcsdsHeader) + sizeof(CommandPacket)> frame{};

    CcsdsHeader hdr{};
    hdr.sync_word = CCSDS_SYNC_WORD;
    hdr.apid = Apid::COMMAND;
    hdr.seq_count = seq;
    hdr.payload_len = static_cast<uint16_t>(sizeof(CommandPacket));

    std::memcpy(frame.data(), &hdr, sizeof(hdr));
    std::memcpy(frame.data() + sizeof(hdr), &cmd, sizeof(cmd));
    return frame;
}

class ScopedReplayPath {
public:
    ScopedReplayPath() {
        const auto ticks = Clock::now().time_since_epoch().count();
        std::array<char, 96> path{};
        (void)std::snprintf(path.data(), path.size(), "benchmark_replay_seq_%lld.db",
            static_cast<long long>(ticks));
        path_ = path.data();
        (void)std::remove(path_.c_str());
        (void)setenv(TelemetryBridge::REPLAY_SEQ_FILE_ENV, path_.c_str(), 1);
    }

    ~ScopedReplayPath() {
        (void)unsetenv(TelemetryBridge::REPLAY_SEQ_FILE_ENV);
        (void)std::remove(path_.c_str());
    }

    ScopedReplayPath(const ScopedReplayPath&) = delete;
    auto operator=(const ScopedReplayPath&) -> ScopedReplayPath& = delete;

private:
    std::string path_{};
};

auto run_uplink_benchmark() -> UplinkMetrics {
    constexpr size_t FRAME_COUNT = 20000u;

    TimeService::initEpoch();
    ScopedReplayPath replay_path{};

    TelemetryBridge bridge{"RadioLink", 20, 19301, 19302};
    InputPort<CommandPacket, 32768> sink{};
    bridge.command_out.connect(&sink);

    std::vector<std::array<uint8_t, sizeof(CcsdsHeader) + sizeof(CommandPacket)>> frames;
    frames.reserve(FRAME_COUNT);
    for (size_t i = 0; i < FRAME_COUNT; ++i) {
        frames.push_back(build_uplink_frame(
            static_cast<uint16_t>((i % 60000u) + 1u),
            CommandPacket{200u, 1u, 0.0f}));
    }

    std::vector<double> latencies_us;
    latencies_us.reserve(FRAME_COUNT);

    const auto bench_start = Clock::now();
    size_t accepted = 0u;
    {
        // Keep benchmark output deterministic and concise while stressing uplink path.
        ScopedStdioSilence silenced_stdio{};
        for (const auto& frame : frames) {
            const auto t0 = Clock::now();
            if (bridge.ingestUplinkFrameForTest(frame.data(), frame.size())) {
                ++accepted;
            }
            const auto t1 = Clock::now();
            latencies_us.push_back(
                std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
    }
    const double duration_s = Seconds(Clock::now() - bench_start).count();

    size_t consumed = 0u;
    CommandPacket cmd{};
    while (sink.tryConsume(cmd)) {
        ++consumed;
    }

    UplinkMetrics out{};
    out.frames = FRAME_COUNT;
    out.accepted = accepted;
    out.consumed = consumed;
    out.duration_s = duration_s;
    out.throughput_cmd_s = duration_s > 0.0 ? static_cast<double>(accepted) / duration_s : 0.0;
    out.latency_p50_us = percentile_us(latencies_us, 0.50);
    out.latency_p95_us = percentile_us(latencies_us, 0.95);
    return out;
}

auto run_crc16_benchmark() -> ThroughputMetrics {
    constexpr size_t PAYLOAD_SIZE = 256u;
    constexpr size_t ITERATIONS = 300000u;

    std::array<uint8_t, PAYLOAD_SIZE> payload{};
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>(i & 0xFFu);
    }

    uint64_t accumulator = 0u;
    const auto start = Clock::now();
    for (size_t i = 0; i < ITERATIONS; ++i) {
        payload[0] = static_cast<uint8_t>(i & 0xFFu);
        accumulator ^= Serializer::crc16(payload.data(), payload.size());
    }
    const double duration_s = Seconds(Clock::now() - start).count();

    const double total_mb = (static_cast<double>(PAYLOAD_SIZE) * static_cast<double>(ITERATIONS))
                            / 1'000'000.0;

    ThroughputMetrics out{};
    out.iterations = ITERATIONS;
    out.bytes_per_iteration = PAYLOAD_SIZE;
    out.duration_s = duration_s;
    out.throughput_mb_s = duration_s > 0.0 ? total_mb / duration_s : 0.0;
    out.accumulator = accumulator;
    return out;
}

auto run_cobs_roundtrip_benchmark() -> ThroughputMetrics {
    constexpr size_t PAYLOAD_SIZE = 22u;
    constexpr size_t ITERATIONS = 300000u;

    std::array<uint8_t, PAYLOAD_SIZE> payload{};
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>((i * 17u) & 0xFFu);
    }
    payload[5] = 0u;
    payload[9] = 0u;

    std::array<uint8_t, cobs::encodedMaxSize(PAYLOAD_SIZE)> encoded{};
    std::array<uint8_t, PAYLOAD_SIZE> decoded{};

    uint64_t accumulator = 0u;
    const auto start = Clock::now();
    for (size_t i = 0; i < ITERATIONS; ++i) {
        payload[0] = static_cast<uint8_t>(i & 0xFFu);
        const size_t encoded_len = cobs::encode(
            payload.data(), payload.size(), encoded.data(), encoded.size());
        if (encoded_len == 0u) {
            continue;
        }
        const size_t decoded_len = cobs::decode(
            encoded.data(), encoded_len - 1u, decoded.data(), decoded.size());
        if (decoded_len == payload.size()) {
            accumulator += decoded[0];
        }
    }
    const double duration_s = Seconds(Clock::now() - start).count();

    const double total_mb = (static_cast<double>(PAYLOAD_SIZE) * static_cast<double>(ITERATIONS))
                            / 1'000'000.0;

    ThroughputMetrics out{};
    out.iterations = ITERATIONS;
    out.bytes_per_iteration = PAYLOAD_SIZE;
    out.duration_s = duration_s;
    out.throughput_mb_s = duration_s > 0.0 ? total_mb / duration_s : 0.0;
    out.accumulator = accumulator;
    return out;
}

auto render_json(
    const UplinkMetrics& uplink,
    const ThroughputMetrics& crc16,
    const ThroughputMetrics& cobs_rt) -> std::string {
    std::array<char, 4096> buf{};
    const int written = std::snprintf(
        buf.data(), buf.size(),
        "{\n"
        "  \"uplink\": {\n"
        "    \"frames\": %zu,\n"
        "    \"accepted\": %zu,\n"
        "    \"consumed\": %zu,\n"
        "    \"duration_s\": %.6f,\n"
        "    \"throughput_cmd_per_s\": %.3f,\n"
        "    \"latency_p50_us\": %.3f,\n"
        "    \"latency_p95_us\": %.3f\n"
        "  },\n"
        "  \"crc16\": {\n"
        "    \"iterations\": %zu,\n"
        "    \"bytes_per_iteration\": %zu,\n"
        "    \"duration_s\": %.6f,\n"
        "    \"throughput_mb_per_s\": %.3f,\n"
        "    \"accumulator\": %llu\n"
        "  },\n"
        "  \"cobs_roundtrip\": {\n"
        "    \"iterations\": %zu,\n"
        "    \"bytes_per_iteration\": %zu,\n"
        "    \"duration_s\": %.6f,\n"
        "    \"throughput_mb_per_s\": %.3f,\n"
        "    \"accumulator\": %llu\n"
        "  }\n"
        "}\n",
        uplink.frames,
        uplink.accepted,
        uplink.consumed,
        uplink.duration_s,
        uplink.throughput_cmd_s,
        uplink.latency_p50_us,
        uplink.latency_p95_us,
        crc16.iterations,
        crc16.bytes_per_iteration,
        crc16.duration_s,
        crc16.throughput_mb_s,
        static_cast<unsigned long long>(crc16.accumulator),
        cobs_rt.iterations,
        cobs_rt.bytes_per_iteration,
        cobs_rt.duration_s,
        cobs_rt.throughput_mb_s,
        static_cast<unsigned long long>(cobs_rt.accumulator));

    if (written <= 0) {
        return "{}\n";
    }
    return std::string(buf.data(), static_cast<size_t>(written));
}

} // namespace

auto main(int argc, char** argv) -> int {
    std::string json_out_path{};
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--json") == 0 && (i + 1) < argc) {
            json_out_path = argv[++i];
        }
    }

    const UplinkMetrics uplink = run_uplink_benchmark();
    const ThroughputMetrics crc16 = run_crc16_benchmark();
    const ThroughputMetrics cobs_rt = run_cobs_roundtrip_benchmark();

    const std::string payload = render_json(uplink, crc16, cobs_rt);
    std::printf("%s", payload.c_str());

    if (!json_out_path.empty()) {
        std::ofstream out(json_out_path, std::ios::trunc);
        if (!out.is_open()) {
            std::fprintf(stderr, "[benchmark] ERROR: unable to open output: %s\n", json_out_path.c_str());
            return 2;
        }
        out << payload;
    }

    if (uplink.accepted != uplink.frames || uplink.consumed != uplink.accepted) {
        std::fprintf(stderr, "[benchmark] ERROR: uplink acceptance/consumption mismatch.\n");
        return 3;
    }

    return 0;
}
