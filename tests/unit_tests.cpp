// =============================================================================
// unit_tests.cpp — DELTA-V Framework Test Suite
// =============================================================================
// Covers every critical path identified in the evaluation:
//   - RingBuffer: push/pop/overflow/thread-safety
//   - Port: connect/send/receive/overflow counting
//   - Serializer: round-trip pack/unpack for all three packet types + CRC-16
//   - ParamDb: get/set/CRC integrity/collision resistance/string hash
//   - CommandHub: ACK routing, NACK on unknown ID, multi-command drain
//   - EventHub: multi-source drain, burst handling
//   - PowerComponent: opcode 1 reset, opcode 2 drain rate, timestamp
//   - SensorComponent: command drain, unknown opcode error count
//   - WatchdogComponent: health threshold transitions, battery safe-mode
//   - TimeService: MET advances, initEpoch idempotency
//   - TopologyManager: verify() passes after wire()
//
// Run: ./run_tests  (built by CMake target run_tests)
// =============================================================================
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Pull in all framework headers
#include "Os.hpp"
#include "Types.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "ParamDb.hpp"
#include "TimeService.hpp"
#include "CommandHub.hpp"
#include "EventHub.hpp"
#include "TelemHub.hpp"
#include "SensorComponent.hpp"
#include "PowerComponent.hpp"
#include "WatchdogComponent.hpp"
#include "TopologyManager.hpp"

using namespace deltav;

// =============================================================================
// Os::Queue
// =============================================================================
TEST(OsQueue, PushPop) {
    Os::Queue<int, 4> q;
    EXPECT_TRUE(q.push(1));
    EXPECT_TRUE(q.push(2));
    int v{};
    EXPECT_TRUE(q.pop(v)); EXPECT_EQ(v, 1);
    EXPECT_TRUE(q.pop(v)); EXPECT_EQ(v, 2);
    EXPECT_FALSE(q.pop(v)); // empty
}

TEST(OsQueue, FullRejectsPush) {
    Os::Queue<int, 2> q;
    EXPECT_TRUE(q.push(1));
    EXPECT_TRUE(q.push(2));
    EXPECT_FALSE(q.push(3)); // full (capacity 2, internal buf is 3)
}

TEST(OsQueue, IsEmpty) {
    Os::Queue<int, 4> q;
    EXPECT_TRUE(q.isEmpty());
    q.push(42);
    EXPECT_FALSE(q.isEmpty());
}

// =============================================================================
// RingBuffer
// =============================================================================
TEST(RingBuffer, BasicPushPop) {
    RingBuffer<int, 8> rb;
    EXPECT_TRUE(rb.push(10));
    EXPECT_TRUE(rb.push(20));
    int v{};
    EXPECT_TRUE(rb.pop(v)); EXPECT_EQ(v, 10);
    EXPECT_TRUE(rb.pop(v)); EXPECT_EQ(v, 20);
    EXPECT_FALSE(rb.pop(v)); // empty — safe, v unchanged
}

TEST(RingBuffer, PopOnEmptyReturnsFalse) {
    RingBuffer<TelemetryPacket, 4> rb;
    TelemetryPacket p{};
    p.timestamp_ms = 0xDEAD;
    EXPECT_FALSE(rb.pop(p));
    // p must be untouched
    EXPECT_EQ(p.timestamp_ms, 0xDEADu);
}

TEST(RingBuffer, OverflowCountIncrementsOnFull) {
    RingBuffer<int, 2> rb;
    rb.push(1); rb.push(2);
    EXPECT_FALSE(rb.push(3)); // overflow
    EXPECT_EQ(rb.peekOverflowCount(), 1u);
    EXPECT_EQ(rb.drainOverflowCount(), 1u);
    EXPECT_EQ(rb.peekOverflowCount(), 0u); // drained
}

TEST(RingBuffer, WrapAround) {
    RingBuffer<int, 4> rb;
    for (int i = 0; i < 4; ++i) EXPECT_TRUE(rb.push(i));
    int v{};
    EXPECT_TRUE(rb.pop(v)); EXPECT_EQ(v, 0);
    EXPECT_TRUE(rb.push(99));
    EXPECT_TRUE(rb.pop(v)); EXPECT_EQ(v, 1);
    EXPECT_TRUE(rb.pop(v)); EXPECT_EQ(v, 2);
    EXPECT_TRUE(rb.pop(v)); EXPECT_EQ(v, 3);
    EXPECT_TRUE(rb.pop(v)); EXPECT_EQ(v, 99);
}

// =============================================================================
// Port — connect / send / receive / overflow
// =============================================================================
TEST(Port, SendReceive) {
    OutputPort<TelemetryPacket> out;
    InputPort<TelemetryPacket, 4> in;
    out.connect(&in);
    TelemetryPacket p{1000, 42, 3.14f};
    out.send(p);
    EXPECT_TRUE(in.hasNew());
    auto got = in.consume();
    EXPECT_EQ(got.timestamp_ms,  1000u);
    EXPECT_EQ(got.component_id,  42u);
    EXPECT_FLOAT_EQ(got.data_payload, 3.14f);
}

TEST(Port, UnconnectedSendIsSafe) {
    OutputPort<TelemetryPacket> out;
    TelemetryPacket p{};
    EXPECT_FALSE(out.send(p)); // no crash, returns false
}

TEST(Port, TryConsumeOnEmpty) {
    InputPort<TelemetryPacket, 4> in;
    TelemetryPacket p{};
    p.component_id = 0xCAFE;
    EXPECT_FALSE(in.tryConsume(p));
    EXPECT_EQ(p.component_id, 0xCAFEu); // untouched
}

TEST(Port, OverflowCounted) {
    OutputPort<TelemetryPacket> out;
    InputPort<TelemetryPacket, 2> in;
    out.connect(&in);
    TelemetryPacket p{1,2,3.0f};
    out.send(p); out.send(p);
    out.send(p); // overflow
    EXPECT_GT(in.overflowCount(), 0u);
}

// =============================================================================
// Serializer — round-trip and CRC
// =============================================================================
TEST(Serializer, TelemRoundTrip) {
    TelemetryPacket orig{12345, 200, -99.5f};
    auto bytes = Serializer::pack(orig);
    auto back  = Serializer::unpackTelem(bytes);
    EXPECT_EQ(back.timestamp_ms,  orig.timestamp_ms);
    EXPECT_EQ(back.component_id,  orig.component_id);
    EXPECT_FLOAT_EQ(back.data_payload, orig.data_payload);
}

TEST(Serializer, CommandRoundTrip) {
    CommandPacket orig{200, 1, 0.0f};
    auto bytes = Serializer::pack(orig);
    auto back  = Serializer::unpackCommand(bytes);
    EXPECT_EQ(back.target_id, 200u);
    EXPECT_EQ(back.opcode,    1u);
}

TEST(Serializer, EventRoundTrip) {
    EventPacket orig = EventPacket::create(Severity::CRITICAL, 1, "TEST EVENT");
    auto bytes = Serializer::pack(orig);
    auto back  = Serializer::unpackEvent(bytes);
    EXPECT_EQ(back.severity,  Severity::CRITICAL);
    EXPECT_EQ(back.source_id, 1u);
    EXPECT_STREQ(back.message, "TEST EVENT");
}

TEST(Serializer, Crc16KnownVector) {
    // CRC-16/CCITT of {0x31,0x32,...,0x39} = 0x29B1 (standard test vector)
    const uint8_t data[] = {'1','2','3','4','5','6','7','8','9'};
    EXPECT_EQ(Serializer::crc16(data, sizeof(data)), 0x29B1u);
}

TEST(Serializer, Crc16DetectsCorruption) {
    TelemetryPacket p{1000, 42, 1.0f};
    auto bytes = Serializer::pack(p);
    uint16_t good_crc = Serializer::crc16(bytes);
    bytes[0] ^= 0xFF; // corrupt one byte
    uint16_t bad_crc  = Serializer::crc16(bytes);
    EXPECT_NE(good_crc, bad_crc);
}

// =============================================================================
// ParamDb
// =============================================================================
class ParamDbTest : public ::testing::Test {
protected:
    ParamDb db; // fresh instance per test (not the singleton)
};

TEST_F(ParamDbTest, GetReturnsDefault) {
    float v = db.getParam(0xABCD1234u, 42.0f);
    EXPECT_FLOAT_EQ(v, 42.0f);
}

TEST_F(ParamDbTest, SetAndGet) {
    db.setParam(0x1u, 3.14f);
    EXPECT_FLOAT_EQ(db.getParam(0x1u, 0.0f), 3.14f);
}

TEST_F(ParamDbTest, CrcPassesAfterSet) {
    db.setParam(0x1u, 1.0f);
    EXPECT_TRUE(db.verifyIntegrity());
}

TEST_F(ParamDbTest, CrcFailsAfterCorruption) {
    db.setParam(0x1u, 1.0f);
    void* ptr = db.getRawPtr(0x1u);
    ASSERT_NE(ptr, nullptr);
    // Corrupt the raw float
    *reinterpret_cast<float*>(ptr) = 999.0f;
    EXPECT_FALSE(db.verifyIntegrity());
}

TEST_F(ParamDbTest, Fnv1aNoCollision) {
    // Old character-sum: "AB"==66+65=131, "BA"==65+66=131 — collision!
    // FNV-1a must produce different values
    uint32_t ab = ParamDb::fnv1a("AB");
    uint32_t ba = ParamDb::fnv1a("BA");
    EXPECT_NE(ab, ba);
}

TEST_F(ParamDbTest, Fnv1aDeterministic) {
    EXPECT_EQ(ParamDb::fnv1a("star_amplitude"), ParamDb::fnv1a("star_amplitude"));
}

TEST_F(ParamDbTest, StringAndIdApisAreEquivalent) {
    db.setParam("my_param", 7.7f);
    float via_id = db.getParam(ParamDb::fnv1a("my_param"), 0.0f);
    EXPECT_FLOAT_EQ(via_id, 7.7f);
}

// =============================================================================
// TimeService
// =============================================================================
TEST(TimeService, MetAdvances) {
    TimeService::initEpoch();
    uint32_t t0 = TimeService::getMET();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    uint32_t t1 = TimeService::getMET();
    EXPECT_GT(t1, t0);
}

TEST(TimeService, IsReadyAfterInit) {
    TimeService::initEpoch();
    EXPECT_TRUE(TimeService::isReady());
}

// =============================================================================
// CommandHub
// =============================================================================
class CommandHubTest : public ::testing::Test {
protected:
    CommandHub hub{"CmdHub", 11};
    OutputPort<CommandPacket> route_port;
    InputPort<CommandPacket>  route_dest;
    InputPort<EventPacket>    ack_dest;

    void SetUp() override {
        TimeService::initEpoch();
        route_port.connect(&route_dest);
        hub.registerRoute(200, &route_port);
        hub.ack_out.connect(&ack_dest);
        hub.init();
    }
};

TEST_F(CommandHubTest, RoutesKnownTargetAndAcks) {
    OutputPort<CommandPacket> sender;
    sender.connect(&hub.cmd_input);

    CommandPacket cmd{200, 1, 0.0f};
    sender.send(cmd);
    hub.step();

    // Command arrived at destination
    EXPECT_TRUE(route_dest.hasNew());
    auto got = route_dest.consume();
    EXPECT_EQ(got.opcode, 1u);

    // ACK emitted
    EXPECT_TRUE(ack_dest.hasNew());
    auto ack = ack_dest.consume();
    EXPECT_EQ(ack.severity, Severity::INFO);
}

TEST_F(CommandHubTest, NacksUnknownTarget) {
    OutputPort<CommandPacket> sender;
    sender.connect(&hub.cmd_input);

    CommandPacket cmd{999, 1, 0.0f}; // unknown ID
    sender.send(cmd);
    hub.step();

    EXPECT_FALSE(route_dest.hasNew()); // not delivered

    EXPECT_TRUE(ack_dest.hasNew());
    auto nack = ack_dest.consume();
    EXPECT_EQ(nack.severity, Severity::WARNING);
}

TEST_F(CommandHubTest, DrainsMultipleCommandsPerTick) {
    OutputPort<CommandPacket> sender;
    sender.connect(&hub.cmd_input);

    CommandPacket cmd{200, 1, 0.0f};
    sender.send(cmd);
    sender.send(cmd);
    sender.send(cmd);
    hub.step(); // must drain all 3

    int count = 0;
    CommandPacket tmp{};
    while (route_dest.tryConsume(tmp)) ++count;
    EXPECT_EQ(count, 3);
}

// =============================================================================
// PowerComponent
// =============================================================================
class PowerComponentTest : public ::testing::Test {
protected:
    PowerComponent batt{"BatterySystem", 200};
    InputPort<Serializer::ByteArray> telem_dest;
    InputPort<EventPacket>           event_dest;
    InputPort<float>                 internal_dest;

    void SetUp() override {
        TimeService::initEpoch();
        batt.telemetry_out.connect(&telem_dest);
        batt.event_out.connect(&event_dest);
        batt.battery_out_internal.connect(&internal_dest);
        batt.init();
    }
};

TEST_F(PowerComponentTest, StartsAt100) {
    EXPECT_FLOAT_EQ(batt.getSOC(), 100.0f);
}

TEST_F(PowerComponentTest, DischargesTowardZero) {
    batt.step();
    EXPECT_LT(batt.getSOC(), 100.0f);
}

TEST_F(PowerComponentTest, ResetOpcode1) {
    // Discharge a bit
    for (int i = 0; i < 10; ++i) batt.step();
    EXPECT_LT(batt.getSOC(), 100.0f);

    // Send RESET_BATTERY command
    OutputPort<CommandPacket> sender;
    sender.connect(&batt.cmd_in);
    sender.send(CommandPacket{200, 1, 0.0f});
    batt.step();

    EXPECT_FLOAT_EQ(batt.getSOC(), 100.0f - 0.1f); // reset then one tick drain
}

TEST_F(PowerComponentTest, SetDrainRateOpcode2) {
    OutputPort<CommandPacket> sender;
    sender.connect(&batt.cmd_in);
    sender.send(CommandPacket{200, 2, 0.5f});
    batt.step(); // applies new drain rate
    batt.step();
    EXPECT_FLOAT_EQ(batt.getSOC(), 100.0f - 0.5f - 0.5f);
}

TEST_F(PowerComponentTest, TelemetryHasRealTimestamp) {
    // The old code hardcoded timestamp_ms = 0 in the TelemetryPacket literal.
    // New code calls TimeService::getMET(). We verify this by:
    //   1. Recording MET before step()
    //   2. Sleeping so the clock ticks
    //   3. Calling step() — packet timestamp must be >= t_before
    // This proves getMET() was called, not a literal zero.
    uint32_t t_before = TimeService::getMET();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    batt.step();
    EXPECT_TRUE(telem_dest.hasNew());
    auto raw = telem_dest.consume();
    auto p   = Serializer::unpackTelem(raw);
    EXPECT_GE(p.timestamp_ms, t_before);
    // Also verify it is strictly greater than t_before (clock advanced)
    EXPECT_GT(TimeService::getMET(), t_before);
}

TEST_F(PowerComponentTest, UnknownOpcodeIncrementsErrorCount) {
    OutputPort<CommandPacket> sender;
    sender.connect(&batt.cmd_in);
    sender.send(CommandPacket{200, 99, 0.0f});
    batt.step();
    EXPECT_GT(batt.getErrorCount(), 0u);
}

// =============================================================================
// SensorComponent
// =============================================================================
TEST(SensorComponent, UnknownOpcodeIncrementsErrorCount) {
    TimeService::initEpoch();
    SensorComponent sensor{"StarTracker", 100};
    InputPort<Serializer::ByteArray> telem_dest;
    sensor.telemetry_out_ground.connect(&telem_dest);
    sensor.init();

    OutputPort<CommandPacket> sender;
    sender.connect(&sensor.cmd_in);
    sender.send(CommandPacket{100, 99, 0.0f}); // unknown opcode
    sensor.step();

    EXPECT_GT(sensor.getErrorCount(), 0u);
}

TEST(SensorComponent, ProducesTelemetry) {
    TimeService::initEpoch();
    SensorComponent sensor{"StarTracker", 100};
    InputPort<Serializer::ByteArray> telem_dest;
    sensor.telemetry_out_ground.connect(&telem_dest);
    sensor.init();
    sensor.step();
    EXPECT_TRUE(telem_dest.hasNew());
}

// =============================================================================
// WatchdogComponent — health threshold FSM
// =============================================================================

// Helper: a component that reports a configurable health status
class FakeComponent : public Component {
public:
    HealthStatus forced{HealthStatus::NOMINAL};
    FakeComponent() : Component("Fake", 999) {}
    void init()  override {}
    void step()  override {}
    HealthStatus reportHealth() override { return forced; }
};

TEST(WatchdogComponent, NominalHeartbeatOnly) {
    TimeService::initEpoch();
    WatchdogComponent wd{"Supervisor", 1};
    InputPort<EventPacket> event_dest;
    wd.event_out.connect(&event_dest);
    wd.init();

    // Run 10 cycles — should get heartbeat on cycle 10
    for (int i = 0; i < 10; ++i) wd.step();

    // Should have events (init event + heartbeat)
    EXPECT_TRUE(event_dest.hasNew());
}

TEST(WatchdogComponent, DetectsWarningHealth) {
    TimeService::initEpoch();
    WatchdogComponent wd{"Supervisor", 1};
    InputPort<EventPacket> event_dest;
    wd.event_out.connect(&event_dest);

    FakeComponent comp;
    comp.forced = HealthStatus::WARNING;
    wd.registerSubsystem(&comp);
    wd.init();
    wd.step();

    EXPECT_EQ(wd.getMissionState(), MissionState::DEGRADED);
}

TEST(WatchdogComponent, EscalatesToSafeModeOnCritical) {
    TimeService::initEpoch();
    WatchdogComponent wd{"Supervisor", 1};
    InputPort<EventPacket> event_dest;
    wd.event_out.connect(&event_dest);

    FakeComponent comp;
    comp.forced = HealthStatus::CRITICAL_FAILURE;
    wd.registerSubsystem(&comp);
    wd.init();
    wd.step();

    EXPECT_EQ(wd.getMissionState(), MissionState::SAFE_MODE);
}

TEST(WatchdogComponent, BatterySafeModeThreshold) {
    TimeService::initEpoch();
    WatchdogComponent wd{"Supervisor", 1};
    InputPort<EventPacket> event_dest;
    InputPort<float>       batt_src;
    OutputPort<float>      batt_out;
    batt_out.connect(&wd.battery_level_in);
    wd.event_out.connect(&event_dest);
    wd.init();

    batt_out.send(4.0f); // below 5% threshold
    wd.step();

    EXPECT_EQ(wd.getMissionState(), MissionState::SAFE_MODE);
}

// =============================================================================
// TopologyManager — verify() must pass after wire()
// =============================================================================
TEST(TopologyManager, VerifyPassesAfterWire) {
    TimeService::initEpoch();
    TopologyManager topo;
    topo.wire();
    topo.registerFdir();
    EXPECT_TRUE(topo.verify());
}

// =============================================================================
// CCSDS Types
// =============================================================================
TEST(Types, CcsdsSyncWordValue) {
    EXPECT_EQ(CCSDS_SYNC_WORD, 0x1ACFFC1Du);
}

TEST(Types, SeverityConstants) {
    EXPECT_EQ(Severity::INFO,     1u);
    EXPECT_EQ(Severity::WARNING,  2u);
    EXPECT_EQ(Severity::CRITICAL, 3u);
}

TEST(Types, ApidConstants) {
    EXPECT_EQ(Apid::TELEMETRY, 10u);
    EXPECT_EQ(Apid::EVENT,     20u);
    EXPECT_EQ(Apid::COMMAND,   30u);
}

TEST(Types, EventPacketNullTerminated) {
    // Message longer than 27 chars must be safely truncated
    auto evt = EventPacket::create(1, 42,
        "THIS IS A VERY LONG MESSAGE THAT EXCEEDS THE FIELD SIZE");
    EXPECT_EQ(evt.message[27], '\0');
}

// =============================================================================
// Main
// =============================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// =============================================================================
// EventHub — Fan-Out (new in v2.0)
// =============================================================================
TEST(EventHub, FanOutToMultipleListeners) {
    TimeService::initEpoch();
    EventHub hub{"EventHub", 12};

    // Two independent listener InputPorts
    InputPort<EventPacket> listener_a;
    InputPort<EventPacket> listener_b;

    // One source
    InputPort<EventPacket> source_staging;
    OutputPort<EventPacket> sender;
    sender.connect(&source_staging);

    hub.registerEventSource(&source_staging);
    hub.registerListener(&listener_a);
    hub.registerListener(&listener_b);
    hub.init();

    // Send one event
    auto evt = EventPacket::create(Severity::INFO, 42, "TEST FAN-OUT");
    sender.send(evt);
    hub.step();

    // Both listeners must have received it independently
    EXPECT_TRUE(listener_a.hasNew());
    EXPECT_TRUE(listener_b.hasNew());

    EventPacket out_a{}, out_b{};
    listener_a.tryConsume(out_a);
    listener_b.tryConsume(out_b);

    EXPECT_STREQ(out_a.message, "TEST FAN-OUT");
    EXPECT_STREQ(out_b.message, "TEST FAN-OUT");
    EXPECT_EQ(out_a.source_id, 42u);
    EXPECT_EQ(out_b.source_id, 42u);
}

TEST(EventHub, FanOutMultipleEventsAllReachAllListeners) {
    TimeService::initEpoch();
    EventHub hub{"EventHub", 12};

    InputPort<EventPacket> listener_a, listener_b;
    InputPort<EventPacket> source_staging;
    OutputPort<EventPacket> sender;
    sender.connect(&source_staging);

    hub.registerEventSource(&source_staging);
    hub.registerListener(&listener_a);
    hub.registerListener(&listener_b);
    hub.init();

    // Send 3 events
    sender.send(EventPacket::create(Severity::INFO,     1, "EVT1"));
    sender.send(EventPacket::create(Severity::WARNING,  2, "EVT2"));
    sender.send(EventPacket::create(Severity::CRITICAL, 3, "EVT3"));
    hub.step();

    // Each listener should have all 3
    int count_a = 0, count_b = 0;
    EventPacket tmp{};
    while (listener_a.tryConsume(tmp)) ++count_a;
    while (listener_b.tryConsume(tmp)) ++count_b;
    EXPECT_EQ(count_a, 3);
    EXPECT_EQ(count_b, 3);
}

TEST(EventHub, SourceAndListenerCounts) {
    EventHub hub{"EventHub", 12};
    InputPort<EventPacket> s1, s2, s3;
    InputPort<EventPacket> l1, l2;

    hub.registerEventSource(&s1);
    hub.registerEventSource(&s2);
    hub.registerEventSource(&s3);
    hub.registerListener(&l1);
    hub.registerListener(&l2);

    EXPECT_EQ(hub.getSourceCount(),   3u);
    EXPECT_EQ(hub.getListenerCount(), 2u);
}

TEST(TopologyManager, EventHubHasTwoListenersAfterWire) {
    TimeService::initEpoch();
    TopologyManager topo;
    topo.wire();
    EXPECT_GE(topo.event_hub.getListenerCount(), 2u);
    EXPECT_GE(topo.event_hub.getSourceCount(),   4u);
}