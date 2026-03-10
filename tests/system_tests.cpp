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

TEST(SystemIntegration, ReplayProtectionRejectsDuplicateSequenceAtIngress) {
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

    const uint32_t rejected_before = topo.radio.getRejectedCount();

    const auto first = buildUplinkFrame(30u, CommandPacket{200u, 2u, 1.0f});
    ASSERT_TRUE(topo.radio.ingestUplinkFrameForTest(first.data(), first.size()));

    // Same sequence must be rejected by anti-replay before command dispatch.
    const auto duplicate = buildUplinkFrame(30u, CommandPacket{200u, 2u, 5.0f});
    EXPECT_FALSE(topo.radio.ingestUplinkFrameForTest(duplicate.data(), duplicate.size()));
    EXPECT_EQ(topo.radio.getRejectedCount(), rejected_before + 1u);

    topo.cmd_hub.step();
    topo.battery.step();

    EXPECT_LT(topo.battery.getSOC(), 99.5f);

    size_t ack_count = 0u;
    EventPacket evt{};
    while (ack_sink.tryConsume(evt)) {
        ++ack_count;
    }
    EXPECT_EQ(ack_count, 1u);
}

TEST(SystemIntegration, InvalidHeadersAreRejectedBeforeCommandDispatch) {
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

    const uint32_t rejected_before = topo.radio.getRejectedCount();

    auto bad_sync = buildUplinkFrame(40u, CommandPacket{200u, 2u, 1.0f});
    auto bad_apid = buildUplinkFrame(41u, CommandPacket{200u, 2u, 1.0f});
    auto bad_len = buildUplinkFrame(42u, CommandPacket{200u, 2u, 1.0f});

    CcsdsHeader hdr{};

    std::memcpy(&hdr, bad_sync.data(), sizeof(hdr));
    hdr.sync_word = 0xDEADBEEFu;
    std::memcpy(bad_sync.data(), &hdr, sizeof(hdr));

    std::memcpy(&hdr, bad_apid.data(), sizeof(hdr));
    hdr.apid = 0xFFFFu;
    std::memcpy(bad_apid.data(), &hdr, sizeof(hdr));

    std::memcpy(&hdr, bad_len.data(), sizeof(hdr));
    hdr.payload_len = static_cast<uint16_t>(sizeof(CommandPacket) - 1u);
    std::memcpy(bad_len.data(), &hdr, sizeof(hdr));

    EXPECT_FALSE(topo.radio.ingestUplinkFrameForTest(bad_sync.data(), bad_sync.size()));
    EXPECT_FALSE(topo.radio.ingestUplinkFrameForTest(bad_apid.data(), bad_apid.size()));
    EXPECT_FALSE(topo.radio.ingestUplinkFrameForTest(bad_len.data(), bad_len.size()));
    EXPECT_EQ(topo.radio.getRejectedCount(), rejected_before + 3u);

    // A valid frame should still pass and dispatch after prior malformed rejects.
    const auto valid = buildUplinkFrame(43u, CommandPacket{200u, 2u, 1.0f});
    ASSERT_TRUE(topo.radio.ingestUplinkFrameForTest(valid.data(), valid.size()));

    topo.cmd_hub.step();
    topo.battery.step();
    EXPECT_LT(topo.battery.getSOC(), 99.5f);

    EventPacket ack{};
    ASSERT_TRUE(ack_sink.tryConsume(ack));
    EXPECT_EQ(ack.severity, Severity::INFO);
    EXPECT_NE(std::string(ack.message.data()).find("ACK"), std::string::npos);
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

TEST(SystemIntegration, RoutesCivilianOpsTargetsFromRadioThroughCommandHub) {
    TimeService::initEpoch();
    ScopedReplayPath replay_path{};
    hal::MockI2c i2c{};
    TopologyManager topo(i2c);

    topo.wire();
    topo.registerFdir();
    topo.watchdog.init();

    ASSERT_TRUE(topo.radio.ingestUplinkFrameForTest(
        buildUplinkFrame(10u, CommandPacket{46u, 1u, 200.0f}).data(),
        sizeof(CcsdsHeader) + sizeof(CommandPacket)));
    ASSERT_TRUE(topo.radio.ingestUplinkFrameForTest(
        buildUplinkFrame(11u, CommandPacket{47u, 1u, 200.0f}).data(),
        sizeof(CcsdsHeader) + sizeof(CommandPacket)));
    ASSERT_TRUE(topo.radio.ingestUplinkFrameForTest(
        buildUplinkFrame(12u, CommandPacket{48u, 1u, 2.0f}).data(),
        sizeof(CcsdsHeader) + sizeof(CommandPacket)));
    ASSERT_TRUE(topo.radio.ingestUplinkFrameForTest(
        buildUplinkFrame(13u, CommandPacket{49u, 1u, 200.0f}).data(),
        sizeof(CcsdsHeader) + sizeof(CommandPacket)));

    topo.cmd_hub.step();

    EXPECT_GT(topo.ats_rts.cmd_in.size(), 0u);
    EXPECT_GT(topo.limit_checker.cmd_in.size(), 0u);
    EXPECT_GT(topo.cfdp_manager.cmd_in.size(), 0u);
    EXPECT_GT(topo.mode_manager.cmd_in.size(), 0u);
}

TEST(SystemIntegration, ModeManagerCommandOutFeedsBatteryThroughCommandHub) {
    TimeService::initEpoch();
    ScopedReplayPath replay_path{};
    hal::MockI2c i2c{};
    TopologyManager topo(i2c);

    topo.wire();
    topo.registerFdir();
    topo.watchdog.init();
    topo.battery.init();
    topo.mode_manager.init();

    const auto send = [&](uint16_t seq, uint32_t opcode, float arg) {
        const auto frame = buildUplinkFrame(seq, CommandPacket{49u, opcode, arg});
        ASSERT_TRUE(topo.radio.ingestUplinkFrameForTest(frame.data(), frame.size()));
    };

    send(20u, 1u, 200.0f); // MODE_STAGE_TARGET
    send(21u, 2u, 2.0f);   // MODE_STAGE_ENABLE_OPCODE -> SET_DRAIN_RATE
    send(22u, 3u, 5.0f);   // MODE_STAGE_ENABLE_ARG
    send(23u, 4u, 2.0f);   // MODE_STAGE_DISABLE_OPCODE
    send(24u, 5u, 0.0f);   // MODE_STAGE_DISABLE_ARG
    send(25u, 6u, 2.0f);   // MODE_STAGE_MASK -> SUN
    send(26u, 7u, 0.0f);   // MODE_COMMIT_RULE
    send(27u, 9u, 2.0f);   // MODE_SET_MODE -> SUN
    send(28u, 10u, 0.0f);  // MODE_APPLY

    topo.cmd_hub.step();      // route incoming mode commands to mode_manager
    topo.mode_manager.step(); // emits command_out -> cmd_hub.cmd_input
    topo.cmd_hub.step();      // route mode-generated command to battery
    topo.battery.step();

    EXPECT_LT(topo.battery.getSOC(), 96.0f);
}

auto main(int argc, char** argv) -> int {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
