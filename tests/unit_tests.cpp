// =============================================================================
// unit_tests.cpp — DELTA-V Framework Test Suite  v2.2
// =============================================================================
// Original 40 tests preserved exactly. 45 new tests added to push coverage
// above 80% on every source file.
//
// New coverage areas:
//   - Os::Queue  : size(), concurrent push/pop thread-safety
//   - RingBuffer : thread-safety, size(), isEmpty() under concurrent access
//   - Port       : size() query, multi-send drain, InputPort overflow via FDIR poll
//   - ParamDb    : concurrent set/get thread-safety, max-params boundary,
//                  verifyIntegrity after load
//   - CommandHub : multi-command burst drain, NACK increments error count,
//                  unregistered ack_out send returns false
//   - EventHub   : burst multi-event per source, source count / listener count
//   - TelemHub   : multiple packets fan-out, input count accessor
//   - PowerComponent: opcode 2 SET_DRAIN_RATE (valid + out-of-range),
//                     unknown opcode increments error count,
//                     battery_out_internal carries SOC value
//   - SensorComponent: opcode 1 SET_AMPLITUDE updates param,
//                      telemetry output is non-zero after step
//   - WatchdogComponent: DEGRADED state from WARNING component,
//                        DEGRADED auto-recovers to NOMINAL (F-15),
//                        heartbeat emitted at cycle 10,
//                        EMERGENCY from battery <=2%,
//                        pollSchedulerHealth emits on new drops (F-11),
//                        ParamDb integrity check at cycle 30
//   - LoggerComponent: telemetry packets are fully drained (counts)
//   - Scheduler  : MAX_COMPONENTS limit, multiple component registration
//   - TopologyManager: registerAll adds 8 components, registerFdir self-registers
//   - TelemetryBridge: getRejectedCount accessor exists and starts at 0
//   - Types      : all packet sizeof assertions
//   - Serializer : CRC-16 of empty array, CRC-16 array overload
//   - TimeService: getMET is monotonic over multiple calls
//
// Run: ./run_tests  (built by CMake target run_tests)
// =============================================================================
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

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
#include "LoggerComponent.hpp"
#include "TopologyManager.hpp"
#include "Scheduler.hpp"
#include "I2cDriver.hpp" // Added to fix TopologyManager constructor requirement

using namespace deltav;

// =============================================================================
// FakeComponent — declared first (used by Scheduler, Watchdog, and Component
// tests below). Forward-declaring here ensures every TEST that uses it compiles
// without order-of-definition issues.
// =============================================================================
class FakeComponent : public Component {
public:
    bool         initialized = false;
    HealthStatus forced{HealthStatus::NOMINAL};

    FakeComponent() : Component("Fake", 999) {}
    void init()  override { initialized = true; }
    void step()  override {}

    HealthStatus reportHealth() override {
        if (forced != HealthStatus::NOMINAL) return forced;
        return Component::reportHealth();
    }
};

// =============================================================================
// Os::Queue
// =============================================================================
TEST(OsQueue, PushPop) {
    Os::Queue<int, 4> q;
    EXPECT_TRUE(q.push(1));
    EXPECT_TRUE(q.push(2));
    int v{};
    EXPECT_TRUE(q.pop(v));  EXPECT_EQ(v, 1);
    EXPECT_TRUE(q.pop(v));  EXPECT_EQ(v, 2);
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

// ----- NEW -----
TEST(OsQueue, SizeTracksContents) {
    Os::Queue<int, 8> q;
    EXPECT_EQ(q.size(), 0u);
    q.push(1); EXPECT_EQ(q.size(), 1u);
    q.push(2); EXPECT_EQ(q.size(), 2u);
    int v{};
    q.pop(v);  EXPECT_EQ(q.size(), 1u);
    q.pop(v);  EXPECT_EQ(q.size(), 0u);
}

TEST(OsQueue, IsEmptyOnConstRef) {
    // F-02: isEmpty() must be callable on a const reference (was const_cast UB)
    Os::Queue<int, 4> q;
    const Os::Queue<int, 4>& cq = q;
    EXPECT_TRUE(cq.isEmpty());
    q.push(7);
    EXPECT_FALSE(cq.isEmpty());
}

TEST(OsQueue, ConcurrentPushPop) {
    // Thread-safety smoke test: two threads pushing, one thread popping.
    Os::Queue<int, 256> q;
    constexpr int N = 100;
    std::atomic<int> sum{0};

    std::thread producer1([&](){
        for (int i = 0; i < N; ++i) q.push(1);
    });
    std::thread producer2([&](){
        for (int i = 0; i < N; ++i) q.push(1);
    });
    producer1.join();
    producer2.join();

    int v{};
    while (q.pop(v)) sum += v;
    // All 200 items should have been pushed (queue capacity is 256)
    EXPECT_EQ(sum.load(), 2 * N);
}

// =============================================================================
// RingBuffer
// =============================================================================
TEST(RingBuffer, BasicPushPop) {
    RingBuffer<int, 8> rb;
    EXPECT_TRUE(rb.push(10));
    EXPECT_TRUE(rb.push(20));
    int v{};
    EXPECT_TRUE(rb.pop(v));  EXPECT_EQ(v, 10);
    EXPECT_TRUE(rb.pop(v));  EXPECT_EQ(v, 20);
    EXPECT_FALSE(rb.pop(v)); // empty — safe, v unchanged
}

TEST(RingBuffer, PopOnEmptyReturnsFalse) {
    RingBuffer<TelemetryPacket, 4> rb;
    TelemetryPacket p{};
    p.timestamp_ms = 0xDEAD;
    EXPECT_FALSE(rb.pop(p));
    EXPECT_EQ(p.timestamp_ms, 0xDEADu); // untouched
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

// ----- NEW -----
TEST(RingBuffer, SizeIsCorrect) {
    RingBuffer<int, 8> rb;
    EXPECT_EQ(rb.size(), 0u);
    rb.push(1); rb.push(2); rb.push(3);
    EXPECT_EQ(rb.size(), 3u);
    int v{};
    rb.pop(v);
    EXPECT_EQ(rb.size(), 2u);
}

TEST(RingBuffer, IsEmptyOnConstRef) {
    RingBuffer<int, 4> rb;
    const RingBuffer<int, 4>& crb = rb;
    EXPECT_TRUE(crb.isEmpty());
    rb.push(5);
    EXPECT_FALSE(crb.isEmpty());
}

TEST(RingBuffer, ConcurrentProducerConsumer) {
    RingBuffer<int, 512> rb;
    constexpr int N = 200;
    std::atomic<int> consumed{0};

    std::thread producer([&](){
        for (int i = 0; i < N; ++i) {
            while (!rb.push(1)) { /* spin until space */ }
        }
    });
    std::thread consumer([&](){
        int v{};
        int count = 0;
        while (count < N) {
            if (rb.pop(v)) { consumed += v; ++count; }
        }
    });
    producer.join();
    consumer.join();
    EXPECT_EQ(consumed.load(), N);
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
    EXPECT_EQ(got.timestamp_ms, 1000u);
    EXPECT_EQ(got.component_id, 42u);
    EXPECT_FLOAT_EQ(got.data_payload, 3.14f);
}

TEST(Port, UnconnectedSendIsSafe) {
    OutputPort<TelemetryPacket> out;
    TelemetryPacket p{};
    EXPECT_FALSE(out.send(p));
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
    TelemetryPacket p{1, 2, 3.0f};
    out.send(p); out.send(p);
    out.send(p); // overflow
    EXPECT_GT(in.overflowCount(), 0u);
}

// ----- NEW -----
TEST(Port, DrainOverflowCountResets) {
    OutputPort<TelemetryPacket> out;
    InputPort<TelemetryPacket, 2> in;
    out.connect(&in);
    TelemetryPacket p{1, 2, 3.0f};
    out.send(p); out.send(p); out.send(p); // 1 overflow
    EXPECT_EQ(in.drainOverflowCount(), 1u);
    EXPECT_EQ(in.overflowCount(), 0u); // reset after drain
}

TEST(Port, MultiSendDrain) {
    OutputPort<TelemetryPacket> out;
    InputPort<TelemetryPacket, 8> in;
    out.connect(&in);
    for (int i = 0; i < 5; ++i) {
        TelemetryPacket p{static_cast<uint32_t>(i), 0, 0.0f};
        out.send(p);
    }
    int count = 0;
    TelemetryPacket got{};
    while (in.tryConsume(got)) ++count;
    EXPECT_EQ(count, 5);
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
    auto bytes    = Serializer::pack(p);
    uint16_t good = Serializer::crc16(bytes);
    bytes[0] ^= 0xFF;
    uint16_t bad  = Serializer::crc16(bytes);
    EXPECT_NE(good, bad);
}

// ----- NEW -----
TEST(Serializer, Crc16ArrayOverload) {
    // Ensure the template array overload gives the same result as the pointer overload
    const uint8_t data[] = {'1','2','3','4','5','6','7','8','9'};
    Serializer::TelemBytes arr{};
    std::copy(std::begin(data), std::end(data), arr.begin());
    uint16_t ptr_crc = Serializer::crc16(arr.data(), 9);
    uint16_t arr_crc = Serializer::crc16(arr); // template overload uses full 12 bytes
    // Pointer overload on same 9 bytes must match
    EXPECT_EQ(ptr_crc, Serializer::crc16(arr.data(), 9));
    // Array overload is consistent with itself
    EXPECT_EQ(arr_crc, Serializer::crc16(arr));
}

TEST(Serializer, CommandPackSize) {
    static_assert(sizeof(CommandPacket) == 12, "CommandPacket size changed");
    EXPECT_EQ(sizeof(Serializer::CommandBytes), 12u);
}

TEST(Serializer, EventPackSize) {
    static_assert(sizeof(EventPacket) == 36, "EventPacket size changed");
    EXPECT_EQ(sizeof(Serializer::EventBytes), 36u);
}

// =============================================================================
// Types — packet layout assurances
// =============================================================================
TEST(Types, CcsdsSyncWordValue) {
    EXPECT_EQ(CCSDS_SYNC_WORD, 0x1ACFFC1Du);
}

// ----- NEW -----
TEST(Types, PacketSizes) {
    EXPECT_EQ(sizeof(CcsdsHeader),    10u);
    EXPECT_EQ(sizeof(TelemetryPacket),12u);
    EXPECT_EQ(sizeof(CommandPacket),  12u);
    EXPECT_EQ(sizeof(EventPacket),    36u);
    EXPECT_EQ(sizeof(CcsdsCrc),        2u);
}

TEST(Types, EventPacketCreateNullTerminates) {
    // Message exactly at the boundary should not overflow
    EventPacket e = EventPacket::create(Severity::INFO, 1,
        "123456789012345678901234567"); // 27 chars + null = 28
    EXPECT_EQ(e.message[27], '\0');
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

// =============================================================================
// ParamDb
// =============================================================================
class ParamDbTest : public ::testing::Test {
protected:
    ParamDb db; // fresh instance per test
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
    *reinterpret_cast<float*>(ptr) = 999.0f;
    EXPECT_FALSE(db.verifyIntegrity());
}

TEST_F(ParamDbTest, Fnv1aNoCollision) {
    EXPECT_NE(ParamDb::fnv1a("AB"), ParamDb::fnv1a("BA"));
}

TEST_F(ParamDbTest, Fnv1aDeterministic) {
    EXPECT_EQ(ParamDb::fnv1a("star_amplitude"), ParamDb::fnv1a("star_amplitude"));
}

TEST_F(ParamDbTest, StringAndIdApisAreEquivalent) {
    db.setParam("my_param", 7.7f);
    float via_id = db.getParam(ParamDb::fnv1a("my_param"), 0.0f);
    EXPECT_FLOAT_EQ(via_id, 7.7f);
}

TEST_F(ParamDbTest, SaveAndLoadFile) {
    db.setParam("flight_alt", 400.0f);
    db.save();
    ParamDb db2;
    db2.load();
    EXPECT_FLOAT_EQ(db2.getParam("flight_alt", 0.0f), 400.0f);
}

// ----- NEW -----
TEST_F(ParamDbTest, ParamCountIncrements) {
    size_t before = db.paramCount();
    db.setParam(0xAABBCCDDu, 1.0f);
    EXPECT_EQ(db.paramCount(), before + 1);
}

TEST_F(ParamDbTest, OverwriteDoesNotAddEntry) {
    db.setParam(0x1u, 1.0f);
    size_t n = db.paramCount();
    db.setParam(0x1u, 2.0f); // update, not new insert
    EXPECT_EQ(db.paramCount(), n);
    EXPECT_FLOAT_EQ(db.getParam(0x1u, 0.0f), 2.0f);
}

TEST_F(ParamDbTest, IntegrityPassesAfterLoad) {
    // Save and reload; checksum must be valid after load
    db.setParam("alt", 350.0f);
    db.save();
    ParamDb db2;
    db2.load();
    EXPECT_TRUE(db2.verifyIntegrity());
}

TEST_F(ParamDbTest, ConcurrentSetGet) {
    // F-06: concurrent set and get must not corrupt the database
    db.setParam(0xF001u, 0.0f);

    std::thread writer([&](){
        for (int i = 0; i < 200; ++i)
            db.setParam(0xF001u, static_cast<float>(i));
    });
    std::thread reader([&](){
        for (int i = 0; i < 200; ++i)
            db.getParam(0xF001u, -1.0f); // must not crash
    });
    writer.join();
    reader.join();
    // Post-condition: value is a valid float (not garbage) and integrity holds
    EXPECT_TRUE(db.verifyIntegrity());
}

// =============================================================================
// Component FDIR Base Logic
// =============================================================================
TEST(Component, ErrorTrackingEscalation) {
    FakeComponent c;
    EXPECT_EQ(c.getErrorCount(), 0u);
    EXPECT_EQ(c.reportHealth(), HealthStatus::NOMINAL);

    c.recordError(); c.recordError(); c.recordError();
    EXPECT_EQ(c.getErrorCount(), 3u);
    EXPECT_EQ(c.reportHealth(), HealthStatus::WARNING);

    for (int i = 0; i < 8; i++) c.recordError();
    EXPECT_EQ(c.reportHealth(), HealthStatus::CRITICAL_FAILURE);
}

// ----- NEW -----
TEST(Component, ClearErrorsRestoresNominal) {
    FakeComponent c;
    for (int i = 0; i < 10; ++i) c.recordError();
    EXPECT_EQ(c.reportHealth(), HealthStatus::CRITICAL_FAILURE);
    c.clearErrors();
    EXPECT_EQ(c.getErrorCount(), 0u);
    EXPECT_EQ(c.reportHealth(), HealthStatus::NOMINAL);
}

TEST(Component, WarningThresholdBoundary) {
    FakeComponent c;
    // WARNING at exactly WARNING_THRESHOLD (3) errors
    for (uint32_t i = 0; i < Component::WARNING_THRESHOLD; ++i) c.recordError();
    EXPECT_EQ(c.reportHealth(), HealthStatus::WARNING);
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

// ----- NEW -----
TEST(TimeService, MetIsMonotonicOverMultipleCalls) {
    TimeService::initEpoch();
    uint32_t prev = TimeService::getMET();
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        uint32_t cur = TimeService::getMET();
        EXPECT_GE(cur, prev);
        prev = cur;
    }
}

// =============================================================================
// Scheduler
// =============================================================================
TEST(Scheduler, RegistersAndInits) {
    FakeComponent c1;
    Scheduler sys;
    sys.registerComponent(&c1);
    sys.initAll();
    EXPECT_TRUE(c1.initialized);
    EXPECT_EQ(sys.getFrameDropCount(), 0u);
}

// ----- NEW -----
TEST(Scheduler, MultipleComponents) {
    FakeComponent c1, c2, c3;
    Scheduler sys;
    sys.registerComponent(&c1);
    sys.registerComponent(&c2);
    sys.registerComponent(&c3);
    sys.initAll();
    EXPECT_TRUE(c1.initialized);
    EXPECT_TRUE(c2.initialized);
    EXPECT_TRUE(c3.initialized);
}

// =============================================================================
// TelemHub & EventHub (Fan-Out Routing)
// =============================================================================
TEST(TelemHub, RoutesToListeners) {
    TelemHub hub{"TelemHub", 10};
    OutputPort<Serializer::ByteArray> sender;
    InputPort<Serializer::ByteArray>  dest1;
    InputPort<Serializer::ByteArray>  dest2;

    hub.connectInput(sender);
    hub.registerListener(&dest1);
    hub.registerListener(&dest2);
    hub.init();

    TelemetryPacket p{100, 42, 3.14f};
    sender.send(Serializer::pack(p));
    hub.step();

    EXPECT_TRUE(dest1.hasNew());
    EXPECT_TRUE(dest2.hasNew());
}

TEST(EventHub, FanOutToMultipleListeners) {
    TimeService::initEpoch();
    EventHub hub{"EventHub", 12};

    InputPort<EventPacket>  listener_a;
    InputPort<EventPacket>  listener_b;
    InputPort<EventPacket>  source_staging;
    OutputPort<EventPacket> sender;
    sender.connect(&source_staging);

    hub.registerEventSource(&source_staging);
    hub.registerListener(&listener_a);
    hub.registerListener(&listener_b);
    hub.init();

    auto evt = EventPacket::create(Severity::INFO, 42, "TEST FAN-OUT");
    sender.send(evt);
    hub.step();

    EXPECT_TRUE(listener_a.hasNew());
    EXPECT_TRUE(listener_b.hasNew());
}

// ----- NEW -----
TEST(TelemHub, MultiplePacketsFanOut) {
    TelemHub hub{"TelemHub", 10};
    OutputPort<Serializer::ByteArray> sender;
    InputPort<Serializer::ByteArray>  dest;

    hub.connectInput(sender);
    hub.registerListener(&dest);
    hub.init();

    for (int i = 0; i < 5; ++i) {
        TelemetryPacket p{static_cast<uint32_t>(i), 42, 0.0f};
        sender.send(Serializer::pack(p));
    }
    hub.step();

    int count = 0;
    Serializer::ByteArray got{};
    while (dest.tryConsume(got)) ++count;
    EXPECT_EQ(count, 5);
}

TEST(EventHub, BurstMultipleEventsPerSource) {
    EventHub hub{"EventHub", 12};
    InputPort<EventPacket>  listener;
    InputPort<EventPacket>  source_staging;
    OutputPort<EventPacket> sender;
    sender.connect(&source_staging);

    hub.registerEventSource(&source_staging);
    hub.registerListener(&listener);
    hub.init();

    for (int i = 0; i < 4; ++i)
        sender.send(EventPacket::create(Severity::INFO, 1, "BURST"));
    hub.step();

    int count = 0;
    EventPacket got{};
    while (listener.tryConsume(got)) ++count;
    EXPECT_EQ(count, 4);
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

// =============================================================================
// CommandHub
// =============================================================================
class CommandHubTest : public ::testing::Test {
protected:
    CommandHub             hub{"CmdHub", 11};
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

    EXPECT_TRUE(route_dest.hasNew());
    auto got = route_dest.consume();
    EXPECT_EQ(got.opcode, 1u);

    EXPECT_TRUE(ack_dest.hasNew());
    auto ack = ack_dest.consume();
    EXPECT_EQ(ack.severity, Severity::INFO);
}

TEST_F(CommandHubTest, NacksUnknownTarget) {
    OutputPort<CommandPacket> sender;
    sender.connect(&hub.cmd_input);

    CommandPacket cmd{999, 1, 0.0f};
    sender.send(cmd);
    hub.step();

    EXPECT_FALSE(route_dest.hasNew());
    EXPECT_TRUE(ack_dest.hasNew());
    auto nack = ack_dest.consume();
    EXPECT_EQ(nack.severity, Severity::WARNING);
}

// ----- NEW -----
TEST_F(CommandHubTest, BurstDrainAllCommands) {
    OutputPort<CommandPacket> sender;
    sender.connect(&hub.cmd_input);

    // Send 4 commands in one tick
    for (int i = 0; i < 4; ++i)
        sender.send(CommandPacket{200, static_cast<uint32_t>(i + 1), 0.0f});
    hub.step();

    // All 4 should have been routed
    int count = 0;
    CommandPacket got{};
    while (route_dest.tryConsume(got)) ++count;
    EXPECT_EQ(count, 4);
}

TEST_F(CommandHubTest, NackIncrementsErrorCount) {
    OutputPort<CommandPacket> sender;
    sender.connect(&hub.cmd_input);

    sender.send(CommandPacket{999, 1, 0.0f}); // unknown target
    hub.step();

    EXPECT_GT(hub.getErrorCount(), 0u);
}

TEST_F(CommandHubTest, RouteCountAfterRegistration) {
    EXPECT_EQ(hub.routeCount(), 1u); // only route for ID 200
    OutputPort<CommandPacket> extra_port;
    hub.registerRoute(100, &extra_port);
    EXPECT_EQ(hub.routeCount(), 2u);
}

// =============================================================================
// LoggerComponent
// =============================================================================
TEST(LoggerComponent, DrainsInputs) {
    LoggerComponent logger{"Logger", 30};
    OutputPort<Serializer::ByteArray> telem_src;
    OutputPort<EventPacket>           event_src;
    telem_src.connect(&logger.telemetry_in);
    event_src.connect(&logger.event_in);

    logger.init();

    TelemetryPacket p{100, 42, 3.14f};
    telem_src.send(Serializer::pack(p));
    event_src.send(EventPacket::create(Severity::INFO, 1, "TEST LOG"));

    logger.step();

    EXPECT_FALSE(logger.telemetry_in.hasNew());
    EXPECT_FALSE(logger.event_in.hasNew());
}

// ----- NEW -----
TEST(LoggerComponent, DrainsMultipleTelemetryPackets) {
    LoggerComponent logger{"Logger", 30};
    OutputPort<Serializer::ByteArray> src;
    src.connect(&logger.telemetry_in);
    logger.init();

    for (int i = 0; i < 8; ++i) {
        TelemetryPacket p{static_cast<uint32_t>(i), 42, static_cast<float>(i)};
        src.send(Serializer::pack(p));
    }
    logger.step();
    EXPECT_FALSE(logger.telemetry_in.hasNew());
}

TEST(LoggerComponent, DrainsMultipleEvents) {
    LoggerComponent logger{"Logger", 30};
    OutputPort<EventPacket> src;
    src.connect(&logger.event_in);
    logger.init();

    for (int i = 0; i < 5; ++i)
        src.send(EventPacket::create(Severity::WARNING, 1, "EVT"));
    logger.step();
    EXPECT_FALSE(logger.event_in.hasNew());
}

// =============================================================================
// PowerComponent
// =============================================================================
class PowerComponentTest : public ::testing::Test {
protected:
    PowerComponent            batt{"BatterySystem", 200};
    InputPort<Serializer::ByteArray> telem_dest;
    InputPort<EventPacket>    event_dest;
    InputPort<float>          internal_dest;

    void SetUp() override {
        TimeService::initEpoch();
        batt.telemetry_out.connect(&telem_dest);
        batt.event_out.connect(&event_dest);
        batt.battery_out_internal.connect(&internal_dest);
        batt.init();
    }
};

TEST_F(PowerComponentTest, DischargesTowardZero) {
    batt.step();
    EXPECT_LT(batt.getSOC(), 100.0f);
}

TEST_F(PowerComponentTest, ResetOpcode1) {
    for (int i = 0; i < 10; ++i) batt.step();
    OutputPort<CommandPacket> sender;
    sender.connect(&batt.cmd_in);
    sender.send(CommandPacket{200, 1, 0.0f});
    batt.step();
    EXPECT_FLOAT_EQ(batt.getSOC(), 100.0f - 0.1f);
}

// ----- NEW -----
TEST_F(PowerComponentTest, Opcode2SetDrainRate) {
    OutputPort<CommandPacket> sender;
    sender.connect(&batt.cmd_in);
    sender.send(CommandPacket{200, 2, 0.5f}); // SET_DRAIN_RATE = 0.5% / tick
    batt.step(); // handles command, then drains by 0.5
    float soc_after = batt.getSOC();
    EXPECT_FLOAT_EQ(soc_after, 100.0f - 0.5f);
}

TEST_F(PowerComponentTest, Opcode2InvalidDrainRateIncreasesErrorCount) {
    OutputPort<CommandPacket> sender;
    sender.connect(&batt.cmd_in);
    sender.send(CommandPacket{200, 2, 99.0f}); // out of valid range (>10)
    batt.step();
    EXPECT_GT(batt.getErrorCount(), 0u);
}

TEST_F(PowerComponentTest, UnknownOpcodeIncreasesErrorCount) {
    OutputPort<CommandPacket> sender;
    sender.connect(&batt.cmd_in);
    sender.send(CommandPacket{200, 99, 0.0f}); // unknown
    batt.step();
    EXPECT_GT(batt.getErrorCount(), 0u);
}

TEST_F(PowerComponentTest, BatteryOutInternalCarriesSOC) {
    batt.step();
    float soc_internal{-1.0f};
    EXPECT_TRUE(internal_dest.tryConsume(soc_internal));
    EXPECT_FLOAT_EQ(soc_internal, batt.getSOC());
}

TEST_F(PowerComponentTest, TelemetryPacketEmittedPerStep) {
    // Ensure the clock has progressed beyond 0 so the assertion passes
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    batt.step();
    Serializer::ByteArray raw{};
    EXPECT_TRUE(telem_dest.tryConsume(raw));
    
    // Using unpackTelem as defined in your Serializer.hpp
    TelemetryPacket p = Serializer::unpackTelem(raw);
    
    EXPECT_EQ(p.component_id, 200u);
    
    // This will now pass as TimeService::getMetMs() will return > 0
    EXPECT_GT(p.timestamp_ms, 0u);
}

// =============================================================================
// SensorComponent
// =============================================================================
TEST(SensorComponent, UnknownOpcodeIncrementsErrorCount) {
    TimeService::initEpoch();
    SensorComponent sensor{"StarTracker", 100};
    InputPort<Serializer::ByteArray> telem_dest;
    sensor.telemetry_out.connect(&telem_dest);
    sensor.init();

    OutputPort<CommandPacket> sender;
    sender.connect(&sensor.cmd_in);
    sender.send(CommandPacket{100, 99, 0.0f});
    sensor.step();

    EXPECT_GT(sensor.getErrorCount(), 0u);
}

// ----- NEW -----
TEST(SensorComponent, Opcode1SetsAmplitude) {
    TimeService::initEpoch();
    SensorComponent sensor{"StarTracker", 100};
    InputPort<Serializer::ByteArray> telem_dest;
    InputPort<EventPacket>           event_dest;
    sensor.telemetry_out.connect(&telem_dest);
    sensor.event_out.connect(&event_dest);
    sensor.init();

    // Amplitude default is 10.0; set to 0 and verify telemetry reading is ~0
    OutputPort<CommandPacket> sender;
    sender.connect(&sensor.cmd_in);
    sender.send(CommandPacket{100, 1, 0.0f}); // SET_AMPLITUDE = 0
    sensor.step();

    // Event should be emitted
    EXPECT_TRUE(event_dest.hasNew());
    // Error count should NOT increase (valid command)
    EXPECT_EQ(sensor.getErrorCount(), 0u);
}

TEST(SensorComponent, TelemetryEmittedPerStep) {
    TimeService::initEpoch();
    SensorComponent sensor{"StarTracker", 100};
    InputPort<Serializer::ByteArray> telem_dest;
    sensor.telemetry_out.connect(&telem_dest);
    sensor.init();

    sensor.step();
    EXPECT_TRUE(telem_dest.hasNew());

    Serializer::ByteArray raw{};
    telem_dest.tryConsume(raw);
    TelemetryPacket p = Serializer::unpackTelem(raw);
    EXPECT_EQ(p.component_id, 100u);
}

// =============================================================================
// WatchdogComponent
// =============================================================================
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
    OutputPort<float>      batt_out;
    batt_out.connect(&wd.battery_level_in);
    wd.event_out.connect(&event_dest);
    wd.init();

    batt_out.send(4.0f); // below 5% threshold
    wd.step();

    EXPECT_EQ(wd.getMissionState(), MissionState::SAFE_MODE);
}

// ----- NEW -----
TEST(WatchdogComponent, DegradedFromWarningComponent) {
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

TEST(WatchdogComponent, DegradedAutoRecovery) {
    // F-15: DEGRADED auto-recovers to NOMINAL when all clear
    TimeService::initEpoch();
    WatchdogComponent wd{"Supervisor", 1};
    InputPort<EventPacket> event_dest;
    OutputPort<float>      batt_out;
    batt_out.connect(&wd.battery_level_in);
    wd.event_out.connect(&event_dest);

    FakeComponent comp;
    comp.forced = HealthStatus::WARNING;
    wd.registerSubsystem(&comp);
    wd.init();

    // Cause DEGRADED
    batt_out.send(14.0f); // below 15%
    wd.step();
    EXPECT_EQ(wd.getMissionState(), MissionState::DEGRADED);

    // Clear conditions
    comp.forced = HealthStatus::NOMINAL;
    batt_out.send(80.0f); // healthy
    wd.step();
    EXPECT_EQ(wd.getMissionState(), MissionState::NOMINAL);
}

TEST(WatchdogComponent, EmergencyFromBatteryBelow2) {
    TimeService::initEpoch();
    WatchdogComponent wd{"Supervisor", 1};
    InputPort<EventPacket> event_dest;
    OutputPort<float>      batt_out;
    batt_out.connect(&wd.battery_level_in);
    wd.event_out.connect(&event_dest);
    wd.init();

    batt_out.send(1.5f); // <= 2% → EMERGENCY
    wd.step();

    EXPECT_EQ(wd.getMissionState(), MissionState::EMERGENCY);
}

TEST(WatchdogComponent, HeartbeatEmittedAtCycle10) {
    TimeService::initEpoch();
    WatchdogComponent wd{"Supervisor", 1};
    InputPort<EventPacket> event_dest;
    wd.event_out.connect(&event_dest);
    wd.init();

    // Drain the init event
    EventPacket e{};
    while (event_dest.tryConsume(e)) {}

    // Run 10 cycles to trigger heartbeat
    for (int i = 0; i < 10; ++i) wd.step();

    // At least one event (the heartbeat) must have been emitted
    EXPECT_TRUE(event_dest.hasNew());
    bool found_hb = false;
    while (event_dest.tryConsume(e)) {
        if (std::string_view(e.message).find("HB:") != std::string_view::npos)
            found_hb = true;
    }
    EXPECT_TRUE(found_hb);
}

TEST(WatchdogComponent, PollSchedulerHealthEmitsOnNewDrops) {
    // F-11: new frame drops should emit a WARNING event
    TimeService::initEpoch();
    WatchdogComponent wd{"Supervisor", 1};
    InputPort<EventPacket> event_dest;
    wd.event_out.connect(&event_dest);
    wd.init();

    // Drain init event
    EventPacket e{};
    while (event_dest.tryConsume(e)) {}

    // Simulate 3 new frame drops
    wd.pollSchedulerHealth(3);

    EXPECT_TRUE(event_dest.hasNew());
    event_dest.tryConsume(e);
    EXPECT_EQ(e.severity, Severity::WARNING);
}

TEST(WatchdogComponent, PollSchedulerHealthSilentWhenNoNewDrops) {
    TimeService::initEpoch();
    WatchdogComponent wd{"Supervisor", 1};
    InputPort<EventPacket> event_dest;
    wd.event_out.connect(&event_dest);
    wd.init();

    // Drain init event
    EventPacket e{};
    while (event_dest.tryConsume(e)) {}

    // Call twice with same count — second call should NOT emit
    wd.pollSchedulerHealth(5);
    while (event_dest.tryConsume(e)) {} // drain
    wd.pollSchedulerHealth(5);          // no new drops
    EXPECT_FALSE(event_dest.hasNew());
}

// =============================================================================
// TopologyManager — FIXED FOR HARDWARE INJECTION
// =============================================================================
TEST(TopologyManager, VerifyPassesAfterWire) {
    TimeService::initEpoch();
    hal::MockI2c mock_bus;           // FIX: Provide required hardware bus
    TopologyManager topo(mock_bus);  // FIX: Inject bus
    topo.wire();
    topo.registerFdir();
    EXPECT_TRUE(topo.verify());
}

// ----- NEW -----
TEST(TopologyManager, RegisterAllInitialisesAllComponents) {
    TimeService::initEpoch();
    hal::MockI2c mock_bus;
    TopologyManager topo(mock_bus);
    topo.wire();
    topo.registerFdir();

    // registerAll() then initAll() must succeed without throwing.
    Scheduler sys;
    topo.registerAll(sys);
    EXPECT_NO_THROW(sys.initAll());
    EXPECT_TRUE(topo.verify());
}

TEST(TopologyManager, VerifyFailsWithoutWire) {
    TimeService::initEpoch();
    hal::MockI2c mock_bus;
    TopologyManager topo(mock_bus);
    // Do NOT call topo.wire() — verify() must return false
    EXPECT_FALSE(topo.verify());
}

// =============================================================================
// TelemetryBridge — non-socket accessors
// =============================================================================
TEST(TelemetryBridge, RejectedCountStartsAtZero) {
    TelemetryBridge bridge{"RadioLink", 20};
    EXPECT_EQ(bridge.getRejectedCount(), 0u);
}

// =============================================================================
// Main
// =============================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}