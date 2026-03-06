#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "TelemetryBridge.hpp"
#include "TimeService.hpp"
#include "TopologyManager.hpp"
#include "I2cDriver.hpp"

using namespace deltav;

namespace {

auto buildUplinkFrame(
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
        const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
        std::array<char, 96> path{};
        (void)std::snprintf(path.data(), path.size(), "system_test_replay_%lld.db",
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

} // namespace

TEST(SystemIntegration, CommandPathNominalDispatchesAndUpdatesBattery) {
    TimeService::initEpoch();
    ScopedReplayPath replay_path{};
    hal::MockI2c i2c{};
    TopologyManager topo(i2c);

    topo.wire();
    topo.registerFdir();
    topo.watchdog.init();
    topo.battery.init();

    InputPort<EventPacket, 64> ack_sink{};
    topo.cmd_hub.ack_out.connect(&ack_sink);

    const auto frame = buildUplinkFrame(1u, CommandPacket{200u, 2u, 1.0f});
    ASSERT_TRUE(topo.radio.ingestUplinkFrameForTest(frame.data(), frame.size()));

    topo.cmd_hub.step();
    topo.battery.step();

    EXPECT_LT(topo.battery.getSOC(), 99.5f);

    EventPacket ack{};
    ASSERT_TRUE(ack_sink.tryConsume(ack));
    EXPECT_EQ(ack.severity, Severity::INFO);
    EXPECT_NE(std::string(ack.message.data()).find("ACK"), std::string::npos);
}

TEST(SystemIntegration, SafeModeBlocksOperationalCommand) {
    TimeService::initEpoch();
    ScopedReplayPath replay_path{};
    hal::MockI2c i2c{};
    TopologyManager topo(i2c);

    topo.wire();
    topo.registerFdir();
    topo.watchdog.init();
    topo.battery.init();

    InputPort<EventPacket, 64> ack_sink{};
    topo.cmd_hub.ack_out.connect(&ack_sink);

    topo.watchdog.injectBatteryLevel(4.0f); // force SAFE_MODE threshold

    const auto frame = buildUplinkFrame(2u, CommandPacket{200u, 2u, 4.0f});
    ASSERT_TRUE(topo.radio.ingestUplinkFrameForTest(frame.data(), frame.size()));

    topo.cmd_hub.step();
    topo.battery.step();

    // Default drain remains 0.1f; blocked opcode must not set 4.0f drain rate.
    EXPECT_GT(topo.battery.getSOC(), 99.7f);

    EventPacket nack{};
    ASSERT_TRUE(ack_sink.tryConsume(nack));
    EXPECT_EQ(nack.severity, Severity::WARNING);
    EXPECT_NE(std::string(nack.message.data()).find("FSM_BLOCKED"), std::string::npos);
}

TEST(SystemIntegration, TelemAndEventHubFanOutToRadioAndRecorder) {
    TimeService::initEpoch();
    ScopedReplayPath replay_path{};
    hal::MockI2c i2c{};
    TopologyManager topo(i2c);

    topo.wire();
    topo.registerFdir();
    topo.star_tracker.init();
    topo.battery.init();
    topo.imu_unit.init();

    topo.star_tracker.step();
    topo.battery.step();
    topo.imu_unit.step();
    topo.telem_hub.step();

    EXPECT_GT(topo.radio.telem_in.size(), 0u);
    EXPECT_GT(topo.recorder.telemetry_in.size(), 0u);

    ASSERT_TRUE(topo.battery.cmd_in.receive(CommandPacket{200u, 99u, 0.0f}));
    topo.battery.step();
    topo.event_hub.step();

    EXPECT_GT(topo.radio.event_in.size(), 0u);
    EXPECT_GT(topo.recorder.event_in.size(), 0u);
}

auto main(int argc, char** argv) -> int {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
