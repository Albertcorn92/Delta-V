// =============================================================================
// unit_tests.cpp — DELTA-V Framework Test Suite  v4.0
// =============================================================================
// This suite preserves legacy regression tests and extends coverage for
// v3.0/v4.0 safety, security, and ESP32-readiness additions:
//
//   HeapGuard     : arm/isArmed, violation aborts (death test)
//   TmrStore      : read/write, SEU detection, majority-vote self-healing,
//                   double-upset fallback, TmrRegistry scrubAll
//   Cobs          : encode/decode roundtrip, delimiter guarantee, noise recovery
//   MissionFsm    : every state × every OpClass combination
//   RateGroupExecutive: registration counts, initAll, tier counts
//   CommandHub v3 : FSM blocks OPERATIONAL in SAFE_MODE (DV-SEC-02)
//   WatchdogComponent v3: injectBatteryLevel fault injection, TMR scrub call
//
// All tests reference the requirement IDs defined in Requirements.hpp.
//
// Build:  cmake --build build --target run_tests
// Run:    cd build && ctest  (or ./run_tests --gtest_color=yes)
// =============================================================================
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <random>
#include <string>
#include <thread>
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
#include "TelemetryBridge.hpp"
#include "SensorComponent.hpp"
#include "PowerComponent.hpp"
#include "PayloadMonitorComponent.hpp"
#include "WatchdogComponent.hpp"
#include "LoggerComponent.hpp"
#include "TopologyManager.hpp"
#include "Scheduler.hpp"
#include "I2cDriver.hpp"
#include "HeapGuard.hpp"
#include "ImuComponent.hpp"
#include "TmrStore.hpp"
#include "Cobs.hpp"
#include "MissionFsm.hpp"
#include "RateGroupExecutive.hpp"
#include "CommandSequencerComponent.hpp"
#include "FileTransferComponent.hpp"
#include "MemoryDwellComponent.hpp"
#include "TimeSyncComponent.hpp"
#include "PlaybackComponent.hpp"
#include "OtaComponent.hpp"
#include "AtsRtsSequencerComponent.hpp"
#include "LimitCheckerComponent.hpp"
#include "CfdpComponent.hpp"
#include "ModeManagerComponent.hpp"

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

auto buildRawKissWireFrame(
    const std::array<uint8_t, sizeof(CcsdsHeader) + sizeof(CommandPacket)>& frame)
    -> std::vector<uint8_t> {
    std::vector<uint8_t> wire{};
    wire.reserve(2u + (frame.size() * 2u) + 1u);
    wire.push_back(TelemetryBridge::KISS_FEND);
    wire.push_back(TelemetryBridge::KISS_CMD_DATA);
    for (uint8_t byte : frame) {
        if (byte == TelemetryBridge::KISS_FEND) {
            wire.push_back(TelemetryBridge::KISS_FESC);
            wire.push_back(TelemetryBridge::KISS_TFEND);
        } else if (byte == TelemetryBridge::KISS_FESC) {
            wire.push_back(TelemetryBridge::KISS_FESC);
            wire.push_back(TelemetryBridge::KISS_TFESC);
        } else {
            wire.push_back(byte);
        }
    }
    wire.push_back(TelemetryBridge::KISS_FEND);
    return wire;
}

auto makeUniqueTestLogPath(const char* stem, const char* ext) -> std::string {
    static std::atomic<uint32_t> counter{0};
    const uint32_t id = counter.fetch_add(1, std::memory_order_relaxed);
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    std::array<char, 128> path{};
    (void)std::snprintf(path.data(), path.size(), "%s_%lld_%u%s",
        stem, static_cast<long long>(tick), id, ext);
    return std::string{path.data()};
}

class ScopedEnvVar {
public:
    ScopedEnvVar(const char* key, const char* value)
        : key_(key) {
        const char* old = std::getenv(key_);
        if (old != nullptr) {
            had_old_ = true;
            old_value_ = old;
        }

        if (value != nullptr) {
            (void)setenv(key_, value, 1);
        } else {
            (void)unsetenv(key_);
        }
    }

    ~ScopedEnvVar() {
        if (had_old_) {
            (void)setenv(key_, old_value_.c_str(), 1);
        } else {
            (void)unsetenv(key_);
        }
    }

    ScopedEnvVar(const ScopedEnvVar&) = delete;
    auto operator=(const ScopedEnvVar&) -> ScopedEnvVar& = delete;

private:
    const char* key_{nullptr};
    bool had_old_{false};
    std::string old_value_{};
};

class ScopedReplaySeqFile {
public:
    ScopedReplaySeqFile()
        : path_(makeUniqueTestLogPath("test_replay_seq", ".db")),
          env_(TelemetryBridge::REPLAY_SEQ_FILE_ENV, path_.c_str()) {
        (void)std::remove(path_.c_str());
    }

    ~ScopedReplaySeqFile() {
        (void)std::remove(path_.c_str());
    }

    ScopedReplaySeqFile(const ScopedReplaySeqFile&) = delete;
    auto operator=(const ScopedReplaySeqFile&) -> ScopedReplaySeqFile& = delete;

private:
    std::string path_{};
    ScopedEnvVar env_;
};

} // namespace

// =============================================================================
// Helpers
// =============================================================================
class FakeComponent : public Component {
public:
    bool         initialized{false};
    HealthStatus forced{HealthStatus::NOMINAL};
    FakeComponent() : Component("Fake", 999) {}
    void init()  override { initialized = true; }
    void step()  override {}
    HealthStatus reportHealth() override {
        return (forced != HealthStatus::NOMINAL) ? forced : Component::reportHealth();
    }
};

class FakeActiveCritical : public Component {
public:
    FakeActiveCritical() : Component("FakeActive", 1001) {}

    void init() override {}
    void step() override {}
    bool isActive() const override { return true; }
    void startThread() override { ++start_count; }
    void joinThread() override { ++join_count; }
    HealthStatus reportHealth() override { return HealthStatus::CRITICAL_FAILURE; }

    uint32_t start_count{0};
    uint32_t join_count{0};
};

class FakeActiveThreshold : public Component {
public:
    FakeActiveThreshold() : Component("FakeActiveThreshold", 1002) {
        for (uint32_t i = 0; i < Component::CRITICAL_THRESHOLD; ++i) {
            recordError();
        }
    }

    void init() override {}
    void step() override {}
    bool isActive() const override { return true; }
    void startThread() override { ++start_count; }
    void joinThread() override { ++join_count; }

    uint32_t start_count{0};
    uint32_t join_count{0};
};

class TestI2cBus final : public hal::I2cBus {
public:
    bool write_ok{true};
    bool read_ok{true};
    uint8_t read_hi{0x01};
    uint8_t read_lo{0x02};

    bool read(uint8_t, uint8_t, uint8_t* data, size_t len) override {
        if (!read_ok) {
            return false;
        }
        if (len >= 2) {
            data[0] = read_hi;
            data[1] = read_lo;
        }
        return true;
    }

    bool write(uint8_t, uint8_t, const uint8_t*, size_t) override {
        return write_ok;
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
    EXPECT_FALSE(q.pop(v));
}

TEST(OsQueue, FullRejectsPush) {
    Os::Queue<int, 2> q;
    EXPECT_TRUE(q.push(1));
    EXPECT_TRUE(q.push(2));
    EXPECT_FALSE(q.push(3));
}

TEST(OsQueue, SizeAndIsEmpty) {
    Os::Queue<int, 8> q;
    EXPECT_TRUE(q.isEmpty());
    EXPECT_EQ(q.size(), 0u);
    q.push(42);
    EXPECT_FALSE(q.isEmpty());
    EXPECT_EQ(q.size(), 1u);
}

TEST(OsQueue, ThreadSafePushPop) {
    Os::Queue<int, 128> q;
    std::atomic<int> sum{0};
    std::thread producer([&](){
        for (int i = 0; i < 100; ++i) q.push(i);
    });
    std::thread consumer([&](){
        int v{}, count{0};
        while (count < 100) {
            if (q.pop(v)) { sum += v; ++count; }
        }
    });
    producer.join();
    consumer.join();
    EXPECT_EQ(sum.load(), 4950); // sum(0..99)
}

// =============================================================================
// RingBuffer / Port
// =============================================================================
TEST(RingBuffer, PushPopBasic) {
    InputPort<int, 4> port;
    OutputPort<int> out;
    out.connect(&port);
    out.send(7);
    int v{};
    EXPECT_TRUE(port.tryConsume(v));
    EXPECT_EQ(v, 7);
}

TEST(RingBuffer, OverflowCounted) {
    InputPort<int, 2> port;
    OutputPort<int> out;
    out.connect(&port);
    out.send(1); out.send(2); out.send(3); // 3rd overflows capacity 2
    EXPECT_GT(port.overflowCount(), 0u);
}

TEST(RingBuffer, HasNew) {
    InputPort<int, 4> port;
    OutputPort<int> out;
    out.connect(&port);
    EXPECT_FALSE(port.hasNew());
    out.send(1);
    EXPECT_TRUE(port.hasNew());
}

TEST(RingBuffer, DrainOverflowCount) {
    InputPort<int, 2> port;
    OutputPort<int> out;
    out.connect(&port);
    out.send(1); out.send(2); out.send(3);
    uint64_t n = port.drainOverflowCount();
    EXPECT_GT(n, 0u);
    EXPECT_EQ(port.drainOverflowCount(), 0u); // second call should be 0
}

TEST(RingBuffer, SizeQuery) {
    InputPort<int, 8> port;
    OutputPort<int> out;
    out.connect(&port);
    out.send(1); out.send(2);
    EXPECT_EQ(port.size(), 2u);
}

TEST(RingBuffer, SendFailsWhenDisconnected) {
    OutputPort<int> out;
    EXPECT_FALSE(out.send(123));
}

// =============================================================================
// Types & Serializer
// =============================================================================
TEST(Types, PacketSizes) {
    EXPECT_EQ(sizeof(CcsdsHeader),    10u);
    EXPECT_EQ(sizeof(TelemetryPacket),12u);
    EXPECT_EQ(sizeof(CommandPacket),  12u);
    EXPECT_EQ(sizeof(EventPacket),    36u);
}

TEST(Serializer, PackUnpackTelemetry) {
    TelemetryPacket p{12345, 42, 3.14f};
    auto bytes = Serializer::pack(p);
    auto out   = Serializer::unpackTelem(bytes);
    EXPECT_EQ(out.timestamp_ms, 12345u);
    EXPECT_EQ(out.component_id, 42u);
    EXPECT_FLOAT_EQ(out.data_payload, 3.14f);
}

TEST(Serializer, CRC16NonZeroForNonEmpty) {
    uint8_t data[] = {1, 2, 3, 4};
    EXPECT_NE(Serializer::crc16(data, 4), 0u);
}

TEST(Serializer, CRC16ArrayOverload) {
    TelemetryPacket p{0, 1, 2.0f};
    auto bytes = Serializer::pack(p);
    uint16_t crc1 = Serializer::crc16(bytes.data(), bytes.size());
    uint16_t crc2 = Serializer::crc16(bytes);
    EXPECT_EQ(crc1, crc2);
}

TEST(Serializer, PackUnpackCommand) {
    CommandPacket p{100, 2, 9.8f};
    auto bytes = Serializer::pack(p);
    auto out   = Serializer::unpackCommand(bytes);
    EXPECT_EQ(out.target_id, 100u);
    EXPECT_EQ(out.opcode,    2u);
    EXPECT_FLOAT_EQ(out.argument, 9.8f);
}

TEST(Serializer, PackUnpackEvent) {
    auto e = EventPacket::create(Severity::CRITICAL, 7, "Test message");
    auto bytes = Serializer::pack(e);
    auto out   = Serializer::unpackEvent(bytes);
    EXPECT_EQ(out.severity,  Severity::CRITICAL);
    EXPECT_EQ(out.source_id, 7u);
    EXPECT_STREQ(out.message.data(), "Test message");
}

TEST(Serializer, CRC16KnownVector) {
    // DV-COMM-01: CRC-16/CCITT reference vector ("123456789")
    constexpr std::array<uint8_t, 9> payload{
        {'1', '2', '3', '4', '5', '6', '7', '8', '9'}
    };
    EXPECT_EQ(Serializer::crc16(payload), 0x29B1u);
}

// =============================================================================
// TimeService
// =============================================================================
TEST(TimeService, METIsMonotonic) {
    TimeService::initEpoch();
    uint32_t t1 = TimeService::getMET();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    uint32_t t2 = TimeService::getMET();
    EXPECT_GE(t2, t1);
}

// =============================================================================
// ParamDb
// =============================================================================
TEST(ParamDb, SetAndGet) {
    ParamDb::getInstance().setParam(ParamDb::fnv1a("test_key"), 42.0f);
    EXPECT_FLOAT_EQ(ParamDb::getInstance().getParam(ParamDb::fnv1a("test_key"), 0.0f), 42.0f);
}

TEST(ParamDb, DefaultValue) {
    EXPECT_FLOAT_EQ(
        ParamDb::getInstance().getParam(ParamDb::fnv1a("nonexistent_key"), 99.0f),
        99.0f);
}

TEST(ParamDb, IntegrityAfterWrite) {
    ParamDb::getInstance().setParam(ParamDb::fnv1a("check_key"), 1.0f);
    EXPECT_TRUE(ParamDb::getInstance().verifyIntegrity());
}

TEST(ParamDb, ConcurrentReadWrite) {
    ParamDb& db = ParamDb::getInstance();
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&, i](){
            db.setParam(ParamDb::fnv1a("concurrent"), static_cast<float>(i));
            (void)db.getParam(ParamDb::fnv1a("concurrent"), 0.0f);
        });
    }
    for (auto& t : threads) t.join();
    EXPECT_TRUE(db.verifyIntegrity());
}

TEST(ParamDb, CapacityBoundedAtCompileTimeLimit) {
    ParamDb db;
    for (size_t i = 0; i < MAX_PARAMS + 32u; ++i) {
        db.setParam(static_cast<uint32_t>(i + 1u), static_cast<float>(i));
    }
    EXPECT_EQ(db.paramCount(), MAX_PARAMS);
    EXPECT_TRUE(db.verifyIntegrity());

    // Attempting to insert beyond the fixed bound must not grow the table.
    EXPECT_FLOAT_EQ(db.getParam(0xFFFF0001u, -123.0f), -123.0f);
    EXPECT_EQ(db.paramCount(), MAX_PARAMS);
}

TEST(ParamDb, ConcurrentVerifyIntegrityStaysStableDuringWrites) {
    ParamDb db;
    for (size_t i = 0; i < MAX_PARAMS; ++i) {
        db.setParam(static_cast<uint32_t>(i + 1u), static_cast<float>(i));
    }

    std::atomic<bool> mismatch{false};
    std::atomic<uint32_t> active_writers{2u};

    auto writer = [&]() {
        for (int i = 0; i < 20000; ++i) {
            db.setParam(1u, static_cast<float>(i));
            if ((i % 64) == 0) {
                std::this_thread::yield();
            }
        }
        active_writers.fetch_sub(1u, std::memory_order_acq_rel);
    };

    std::thread writer_a(writer);
    std::thread writer_b(writer);
    std::thread verifier([&]() {
        while (active_writers.load(std::memory_order_acquire) > 0u) {
            if (!db.verifyIntegrity()) {
                mismatch.store(true, std::memory_order_release);
                return;
            }
        }
    });

    writer_a.join();
    writer_b.join();
    verifier.join();

    EXPECT_FALSE(mismatch.load(std::memory_order_acquire));
    EXPECT_TRUE(db.verifyIntegrity());
}

// =============================================================================
// EventHub
// =============================================================================
TEST(EventHub, PropagatesEvent) {
    EventHub hub{"EventHub", 12};
    InputPort<EventPacket> src_port;
    InputPort<EventPacket> listener;
    hub.registerEventSource(&src_port);
    hub.registerListener(&listener);
    hub.init();
    OutputPort<EventPacket> sender;
    sender.connect(&src_port);
    sender.send(EventPacket::create(Severity::INFO, 1, "Hello"));
    hub.step();
    EXPECT_TRUE(listener.hasNew());
}

TEST(EventHub, SourceAndListenerCounts) {
    EventHub hub{"EventHub", 12};
    InputPort<EventPacket> s1, s2;
    InputPort<EventPacket> l1, l2, l3;
    hub.registerEventSource(&s1);
    hub.registerEventSource(&s2);
    hub.registerListener(&l1);
    hub.registerListener(&l2);
    hub.registerListener(&l3);
    EXPECT_EQ(hub.getSourceCount(),   2u);
    EXPECT_EQ(hub.getListenerCount(), 3u);
}

TEST(EventHub, BurstMultipleEventsAllListeners) {
    EventHub hub{"EventHub", 12};
    InputPort<EventPacket> src;
    InputPort<EventPacket> l1, l2;
    hub.registerEventSource(&src);
    hub.registerListener(&l1);
    hub.registerListener(&l2);
    hub.init();
    OutputPort<EventPacket> sender;
    sender.connect(&src);
    for (int i = 0; i < 5; ++i)
        sender.send(EventPacket::create(Severity::WARNING, 1, "burst"));
    hub.step();
    EXPECT_TRUE(l1.hasNew());
    EXPECT_TRUE(l2.hasNew());
}

TEST(EventHub, BackpressuredListenerIncrementsErrorCount) {
    EventHub hub{"EventHub", 12};
    InputPort<EventPacket> src;
    InputPort<EventPacket> constrained_listener;
    InputPort<EventPacket> healthy_listener;
    hub.registerEventSource(&src);
    hub.registerListener(&constrained_listener);
    hub.registerListener(&healthy_listener);
    hub.init();

    OutputPort<EventPacket> sender;
    sender.connect(&src);
    OutputPort<EventPacket> filler;
    filler.connect(&constrained_listener);
    for (size_t i = 0; i < DEFAULT_QUEUE_DEPTH; ++i) {
        ASSERT_TRUE(filler.send(EventPacket::create(Severity::WARNING, 1, "prefill")));
    }
    ASSERT_TRUE(sender.send(EventPacket::create(Severity::WARNING, 1, "evt-1")));
    ASSERT_TRUE(sender.send(EventPacket::create(Severity::WARNING, 1, "evt-2")));

    hub.step();

    EXPECT_GT(hub.getErrorCount(), 0u);
    EXPECT_EQ(constrained_listener.size(), DEFAULT_QUEUE_DEPTH);

    size_t healthy_count = 0u;
    EventPacket evt{};
    while (healthy_listener.tryConsume(evt)) {
        ++healthy_count;
    }
    EXPECT_EQ(healthy_count, 2u);
}

// =============================================================================
// TelemHub
// =============================================================================
TEST(TelemHub, FanOutToAllListeners) {
    TelemHub hub{"TelemHub", 10};
    InputPort<Serializer::ByteArray> l1, l2;
    hub.registerListener(&l1);
    hub.registerListener(&l2);
    hub.init();
    OutputPort<Serializer::ByteArray> src;
    hub.connectInput(src);
    TelemetryPacket p{100, 10, 1.5f};
    src.send(Serializer::pack(p));
    hub.step();
    EXPECT_TRUE(l1.hasNew());
    EXPECT_TRUE(l2.hasNew());
}

TEST(TelemHub, MultiplePacketsFanOut) {
    TelemHub hub{"TelemHub", 10};
    InputPort<Serializer::ByteArray> l1;
    hub.registerListener(&l1);
    hub.init();
    OutputPort<Serializer::ByteArray> src;
    hub.connectInput(src);
    for (int i = 0; i < 4; ++i) {
        TelemetryPacket p{static_cast<uint32_t>(i), 10, static_cast<float>(i)};
        src.send(Serializer::pack(p));
    }
    hub.step();
    int count = 0;
    Serializer::ByteArray buf{};
    while (l1.tryConsume(buf)) ++count;
    EXPECT_EQ(count, 4);
}

TEST(TelemHub, BackpressuredListenerIncrementsErrorCount) {
    TelemHub hub{"TelemHub", 10};
    InputPort<Serializer::ByteArray> constrained_listener;
    InputPort<Serializer::ByteArray> healthy_listener;
    hub.registerListener(&constrained_listener);
    hub.registerListener(&healthy_listener);
    hub.init();

    OutputPort<Serializer::ByteArray> src;
    hub.connectInput(src);

    const auto prefill = Serializer::pack(TelemetryPacket{1u, 10u, 1.0f});
    OutputPort<Serializer::ByteArray> filler;
    filler.connect(&constrained_listener);
    for (size_t i = 0; i < DEFAULT_QUEUE_DEPTH; ++i) {
        ASSERT_TRUE(filler.send(prefill));
    }

    ASSERT_TRUE(src.send(Serializer::pack(TelemetryPacket{2u, 10u, 2.0f})));
    ASSERT_TRUE(src.send(Serializer::pack(TelemetryPacket{3u, 10u, 3.0f})));

    hub.step();

    EXPECT_GT(hub.getErrorCount(), 0u);
    EXPECT_EQ(constrained_listener.size(), DEFAULT_QUEUE_DEPTH);

    size_t healthy_count = 0u;
    Serializer::ByteArray buf{};
    while (healthy_listener.tryConsume(buf)) {
        ++healthy_count;
    }
    EXPECT_EQ(healthy_count, 2u);
}

// =============================================================================
// CommandHub v3.0 — FSM gating
// =============================================================================
class CommandHubFixture : public ::testing::Test {
protected:
    WatchdogComponent wd{"Supervisor", 1};
    CommandHub        hub{"CmdHub", 11};
    InputPort<EventPacket> event_dest;

    void SetUp() override {
        TimeService::initEpoch();
        wd.event_out.connect(&event_dest);
        hub.ack_out.connect(&event_dest);
        hub.setMissionStateSource(&wd);
        wd.init();
    }
};

TEST_F(CommandHubFixture, DispatchesInNominal) {
    InputPort<CommandPacket> target;
    OutputPort<CommandPacket> route;
    route.connect(&target);
    hub.registerRoute(200, &route);
    hub.init();
    OutputPort<CommandPacket> sender;
    sender.connect(&hub.cmd_input);
    sender.send(CommandPacket{200, 1, 0.0f});
    hub.step();
    EXPECT_TRUE(target.hasNew());
}

TEST_F(CommandHubFixture, NackWhenRouteQueueIsFull) {
    InputPort<CommandPacket, 1> target;
    OutputPort<CommandPacket> route;
    route.connect(&target);
    hub.registerRoute(200, &route);
    hub.init();

    // Saturate the route queue so hub delivery fails.
    ASSERT_TRUE(route.send(CommandPacket{200, 1, 0.0f}));

    EventPacket drain{};
    while (event_dest.tryConsume(drain)) {}

    const uint32_t before_errors = hub.getErrorCount();
    OutputPort<CommandPacket> sender;
    sender.connect(&hub.cmd_input);
    sender.send(CommandPacket{200, 1, 0.0f});
    hub.step();

    EXPECT_GT(hub.getErrorCount(), before_errors);

    bool saw_route_busy_nack = false;
    EventPacket evt{};
    while (event_dest.tryConsume(evt)) {
        if (std::string_view(evt.message.data()).find("ROUTE BUSY") != std::string_view::npos) {
            saw_route_busy_nack = true;
        }
    }
    EXPECT_TRUE(saw_route_busy_nack);
}

TEST_F(CommandHubFixture, BlocksOperationalInSafeMode) {
    // DV-SEC-02: OPERATIONAL commands blocked in SAFE_MODE
    InputPort<CommandPacket> target;
    OutputPort<CommandPacket> route;
    route.connect(&target);
    hub.registerRoute(200, &route);
    hub.init();

    // Force SAFE_MODE
    OutputPort<float> batt_src;
    batt_src.connect(&wd.battery_level_in);
    batt_src.send(4.0f); // below 5% threshold
    wd.step();
    ASSERT_EQ(wd.getMissionState(), MissionState::SAFE_MODE);

    // OPERATIONAL opcode (2 = SET_DRAIN_RATE)
    OutputPort<CommandPacket> sender;
    sender.connect(&hub.cmd_input);
    sender.send(CommandPacket{200, 2, 1.0f});
    hub.step();
    EXPECT_FALSE(target.hasNew()); // must be blocked
}

TEST_F(CommandHubFixture, AllowsHousekeepingInSafeMode) {
    InputPort<CommandPacket> target;
    OutputPort<CommandPacket> route;
    route.connect(&target);
    hub.registerRoute(200, &route);
    hub.registerCommandPolicy(200, 1, OpClass::HOUSEKEEPING);
    hub.init();

    OutputPort<float> batt_src;
    batt_src.connect(&wd.battery_level_in);
    batt_src.send(4.0f);
    wd.step();
    ASSERT_EQ(wd.getMissionState(), MissionState::SAFE_MODE);

    // HOUSEKEEPING opcode (1 = RESET_BATTERY)
    OutputPort<CommandPacket> sender;
    sender.connect(&hub.cmd_input);
    sender.send(CommandPacket{200, 1, 0.0f});
    hub.step();
    EXPECT_TRUE(target.hasNew()); // must pass through
}

TEST_F(CommandHubFixture, AllowsConfiguredHousekeepingPolicyInSafeMode) {
    InputPort<CommandPacket> target;
    OutputPort<CommandPacket> route;
    route.connect(&target);
    hub.registerRoute(46, &route);
    hub.registerCommandPolicy(46, 2, OpClass::HOUSEKEEPING);
    hub.init();

    OutputPort<float> batt_src;
    batt_src.connect(&wd.battery_level_in);
    batt_src.send(4.0f);
    wd.step();
    ASSERT_EQ(wd.getMissionState(), MissionState::SAFE_MODE);

    // This target/opcode is explicitly configured as HOUSEKEEPING.
    OutputPort<CommandPacket> sender;
    sender.connect(&hub.cmd_input);
    sender.send(CommandPacket{46, 2, 0.0f});
    hub.step();
    EXPECT_TRUE(target.hasNew());
}

TEST_F(CommandHubFixture, DoesNotBypassNonRegisteredTargetsInSafeMode) {
    InputPort<CommandPacket> target;
    OutputPort<CommandPacket> route;
    route.connect(&target);
    hub.registerRoute(50, &route);
    hub.init();

    OutputPort<float> batt_src;
    batt_src.connect(&wd.battery_level_in);
    batt_src.send(4.0f);
    wd.step();
    ASSERT_EQ(wd.getMissionState(), MissionState::SAFE_MODE);

    OutputPort<CommandPacket> sender;
    sender.connect(&hub.cmd_input);
    sender.send(CommandPacket{50, 2, 0.0f});
    hub.step();
    EXPECT_FALSE(target.hasNew());
}

TEST_F(CommandHubFixture, NackOnUnknownTarget) {
    hub.init();
    OutputPort<CommandPacket> sender;
    sender.connect(&hub.cmd_input);
    sender.send(CommandPacket{999, 1, 0.0f}); // no route registered for 999
    hub.step();
    EXPECT_GT(hub.getErrorCount(), 0u);
}

TEST_F(CommandHubFixture, RouteCountAfterRegistration) {
    EXPECT_EQ(hub.routeCount(), 0u);
    OutputPort<CommandPacket> p1, p2;
    InputPort<CommandPacket>  t1, t2;
    p1.connect(&t1); p2.connect(&t2);
    hub.registerRoute(100, &p1);
    hub.registerRoute(200, &p2);
    EXPECT_EQ(hub.routeCount(), 2u);
}

// =============================================================================
// LoggerComponent
// =============================================================================
TEST(LoggerComponent, DrainsAll) {
    LoggerComponent logger{"Logger", 30};
    OutputPort<Serializer::ByteArray> ts;
    OutputPort<EventPacket>           es;
    ts.connect(&logger.telemetry_in);
    es.connect(&logger.event_in);
    logger.init();
    for (int i = 0; i < 8; ++i) {
        TelemetryPacket p{(uint32_t)i, 42, (float)i};
        ts.send(Serializer::pack(p));
    }
    for (int i = 0; i < 5; ++i)
        es.send(EventPacket::create(Severity::INFO, 1, "log"));
    logger.step();
    EXPECT_FALSE(logger.telemetry_in.hasNew());
    EXPECT_FALSE(logger.event_in.hasNew());
}

TEST(LoggerComponent, EventFlushesImmediately) {
    const std::string telem_log = makeUniqueTestLogPath("test_telem_flush", ".csv");
    const std::string event_log = makeUniqueTestLogPath("test_event_flush", ".log");
    (void)std::remove(telem_log.c_str());
    (void)std::remove(event_log.c_str());

    LoggerComponent logger{"Logger", 30, telem_log.c_str(), event_log.c_str()};
    OutputPort<EventPacket> es;
    es.connect(&logger.event_in);
    logger.init();
    es.send(EventPacket::create(Severity::INFO, 1, "flush_check"));
    logger.step();

    std::ifstream event_file(event_log);
    ASSERT_TRUE(event_file.is_open());
    std::string content;
    std::getline(event_file, content);
    std::getline(event_file, content);
    EXPECT_NE(content.find("flush_check"), std::string::npos);

    (void)std::remove(telem_log.c_str());
    (void)std::remove(event_log.c_str());
}

TEST(LoggerComponent, TelemetryFlushesAtInterval) {
    const std::string telem_log = makeUniqueTestLogPath("test_telem_interval", ".csv");
    const std::string event_log = makeUniqueTestLogPath("test_event_interval", ".log");
    (void)std::remove(telem_log.c_str());
    (void)std::remove(event_log.c_str());

    LoggerComponent logger{"Logger", 30, telem_log.c_str(), event_log.c_str()};
    OutputPort<Serializer::ByteArray> ts;
    ts.connect(&logger.telemetry_in);
    logger.init();
    for (uint32_t i = 0; i < LOG_FLUSH_INTERVAL / 2; ++i) {
        TelemetryPacket p{i, 42, static_cast<float>(i)};
        ts.send(Serializer::pack(p));
    }
    logger.step();
    for (uint32_t i = LOG_FLUSH_INTERVAL / 2; i < LOG_FLUSH_INTERVAL; ++i) {
        TelemetryPacket p{i, 42, static_cast<float>(i)};
        ts.send(Serializer::pack(p));
    }
    logger.step();

    std::ifstream telem_file(telem_log);
    ASSERT_TRUE(telem_file.is_open());
    size_t lines = 0;
    std::string line;
    while (std::getline(telem_file, line)) {
        ++lines;
    }
    EXPECT_GE(lines, static_cast<size_t>(LOG_FLUSH_INTERVAL + 1)); // header + data rows

    (void)std::remove(telem_log.c_str());
    (void)std::remove(event_log.c_str());
}

TEST(LoggerComponent, InitFailureAndDroppedDataIncrementErrors) {
    const std::string telem_log = makeUniqueTestLogPath("missing_dir/test_telem_fail", ".csv");
    const std::string event_log = makeUniqueTestLogPath("missing_dir/test_event_fail", ".log");

    LoggerComponent logger{"Logger", 30, telem_log.c_str(), event_log.c_str()};
    OutputPort<Serializer::ByteArray> ts;
    OutputPort<EventPacket> es;
    ts.connect(&logger.telemetry_in);
    es.connect(&logger.event_in);

    logger.init();
    EXPECT_GE(logger.getErrorCount(), 2u);

    const uint32_t before = logger.getErrorCount();
    ASSERT_TRUE(ts.send(Serializer::pack(TelemetryPacket{1u, 42u, 3.14f})));
    ASSERT_TRUE(es.send(EventPacket::create(Severity::WARNING, 1u, "drop_check")));
    logger.step();

    EXPECT_GT(logger.getErrorCount(), before);
    EXPECT_FALSE(logger.telemetry_in.hasNew());
    EXPECT_FALSE(logger.event_in.hasNew());
}

// =============================================================================
// PowerComponent
// =============================================================================
class PowerTest : public ::testing::Test {
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

TEST_F(PowerTest, DischargesTowardZero) {
    batt.step();
    EXPECT_LT(batt.getSOC(), 100.0f);
}

TEST_F(PowerTest, ResetOpcode1) {
    for (int i = 0; i < 10; ++i) batt.step();
    OutputPort<CommandPacket> sender;
    sender.connect(&batt.cmd_in);
    sender.send(CommandPacket{200, 1, 0.0f});
    batt.step();
    EXPECT_FLOAT_EQ(batt.getSOC(), 100.0f - 0.1f);
}

TEST_F(PowerTest, SetDrainRateOpcode2) {
    OutputPort<CommandPacket> sender;
    sender.connect(&batt.cmd_in);
    sender.send(CommandPacket{200, 2, 0.5f});
    batt.step();
    EXPECT_FLOAT_EQ(batt.getSOC(), 100.0f - 0.5f);
}

TEST_F(PowerTest, InvalidDrainRateIncreasesErrors) {
    OutputPort<CommandPacket> sender;
    sender.connect(&batt.cmd_in);
    sender.send(CommandPacket{200, 2, 99.0f}); // out of range
    batt.step();
    EXPECT_GT(batt.getErrorCount(), 0u);
}

TEST_F(PowerTest, BatteryOutInternalCarriesSOC) {
    batt.step();
    float soc{-1.0f};
    EXPECT_TRUE(internal_dest.tryConsume(soc));
    EXPECT_FLOAT_EQ(soc, batt.getSOC());
}

TEST_F(PowerTest, TelemetryEmittedPerStep) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    batt.step();
    Serializer::ByteArray raw{};
    EXPECT_TRUE(telem_dest.tryConsume(raw));
    auto p = Serializer::unpackTelem(raw);
    EXPECT_EQ(p.component_id, 200u);
    EXPECT_GT(p.timestamp_ms, 0u);
}

// =============================================================================
// SensorComponent
// =============================================================================
TEST(SensorComponent, TelemetryPerStep) {
    TimeService::initEpoch();
    SensorComponent s{"StarTracker", 100};
    InputPort<Serializer::ByteArray> td;
    s.telemetry_out.connect(&td);
    s.init();
    s.step();
    EXPECT_TRUE(td.hasNew());
}

TEST(SensorComponent, SetAmplitudeOpcode1) {
    TimeService::initEpoch();
    SensorComponent s{"StarTracker", 100};
    InputPort<Serializer::ByteArray> td;
    InputPort<EventPacket>           ed;
    s.telemetry_out.connect(&td);
    s.event_out.connect(&ed);
    s.init();
    OutputPort<CommandPacket> sender;
    sender.connect(&s.cmd_in);
    sender.send(CommandPacket{100, 1, 0.0f});
    s.step();
    EXPECT_EQ(s.getErrorCount(), 0u);
}

TEST(SensorComponent, UnknownOpcodeIncreasesErrors) {
    TimeService::initEpoch();
    SensorComponent s{"StarTracker", 100};
    InputPort<Serializer::ByteArray> td;
    s.telemetry_out.connect(&td);
    s.init();
    OutputPort<CommandPacket> sender;
    sender.connect(&s.cmd_in);
    sender.send(CommandPacket{100, 99, 0.0f});
    s.step();
    EXPECT_GT(s.getErrorCount(), 0u);
}

// =============================================================================
// PayloadMonitorComponent
// =============================================================================
TEST(PayloadMonitorComponent, CaptureRejectedWhenDisabled) {
    TimeService::initEpoch();
    PayloadMonitorComponent payload{"PayloadMonitor", 400};
    InputPort<EventPacket> events{};
    payload.event_out.connect(&events);
    payload.init();

    OutputPort<CommandPacket> sender{};
    sender.connect(&payload.cmd_in);
    sender.send(CommandPacket{400u, PayloadMonitorComponent::OP_CAPTURE_SAMPLE, 5.0f});
    payload.step();

    EXPECT_GT(payload.getErrorCount(), 0u);
    EXPECT_EQ(payload.captureCountForTest(), 0u);

    EventPacket evt{};
    bool saw_reject = false;
    while (events.tryConsume(evt)) {
        if (std::string_view(evt.message.data()).find("Capture rejected") != std::string_view::npos) {
            saw_reject = true;
        }
    }
    EXPECT_TRUE(saw_reject);
}

TEST(PayloadMonitorComponent, EnableGainAndCaptureSample) {
    TimeService::initEpoch();
    PayloadMonitorComponent payload{"PayloadMonitor", 400};
    InputPort<EventPacket> events{};
    InputPort<Serializer::ByteArray> telemetry{};
    payload.event_out.connect(&events);
    payload.telemetry_out.connect(&telemetry);
    payload.init();

    OutputPort<CommandPacket> sender{};
    sender.connect(&payload.cmd_in);
    sender.send(CommandPacket{400u, PayloadMonitorComponent::OP_SET_ENABLE, 1.0f});
    sender.send(CommandPacket{400u, PayloadMonitorComponent::OP_SET_GAIN, 2.0f});
    sender.send(CommandPacket{400u, PayloadMonitorComponent::OP_CAPTURE_SAMPLE, 4.0f});
    payload.step();

    EXPECT_TRUE(payload.isEnabledForTest());
    EXPECT_FLOAT_EQ(payload.gainForTest(), 2.0f);
    EXPECT_FLOAT_EQ(payload.lastSampleForTest(), 8.0f);
    EXPECT_EQ(payload.captureCountForTest(), 1u);

    Serializer::ByteArray raw{};
    ASSERT_TRUE(telemetry.tryConsume(raw));
    const auto telem = Serializer::unpackTelem(raw);
    EXPECT_EQ(telem.component_id, 400u);
    EXPECT_FLOAT_EQ(telem.data_payload, 8.0f);

    EventPacket evt{};
    bool saw_enable = false;
    bool saw_gain = false;
    bool saw_capture = false;
    while (events.tryConsume(evt)) {
        const std::string_view msg{evt.message.data()};
        saw_enable = saw_enable || (msg.find("Enabled") != std::string_view::npos);
        saw_gain = saw_gain || (msg.find("Gain updated") != std::string_view::npos);
        saw_capture = saw_capture || (msg.find("Sample captured") != std::string_view::npos);
    }
    EXPECT_TRUE(saw_enable);
    EXPECT_TRUE(saw_gain);
    EXPECT_TRUE(saw_capture);
}

TEST(PayloadMonitorComponent, InvalidGainIncreasesErrors) {
    TimeService::initEpoch();
    PayloadMonitorComponent payload{"PayloadMonitor", 400};
    payload.init();

    OutputPort<CommandPacket> sender{};
    sender.connect(&payload.cmd_in);
    sender.send(CommandPacket{400u, PayloadMonitorComponent::OP_SET_GAIN, 99.0f});
    payload.step();

    EXPECT_GT(payload.getErrorCount(), 0u);
}

TEST(PayloadMonitorComponent, OutputBackpressureIncrementsErrorCount) {
    TimeService::initEpoch();
    PayloadMonitorComponent payload{"PayloadMonitor", 400};
    InputPort<EventPacket, 1> events{};
    InputPort<Serializer::ByteArray, 1> telemetry{};
    OutputPort<EventPacket> event_prefill{};
    OutputPort<Serializer::ByteArray> telem_prefill{};
    payload.event_out.connect(&events);
    payload.telemetry_out.connect(&telemetry);
    event_prefill.connect(&events);
    telem_prefill.connect(&telemetry);
    payload.init();

    ASSERT_TRUE(event_prefill.send(EventPacket::create(Severity::INFO, 1u, "prefill")));
    ASSERT_TRUE(telem_prefill.send(Serializer::pack(TelemetryPacket{1u, 1u, 1.0f})));
    ASSERT_TRUE(payload.cmd_in.receive(
        CommandPacket{400u, PayloadMonitorComponent::OP_SET_ENABLE, 1.0f}));

    const uint32_t before = payload.getErrorCount();
    payload.step();

    EXPECT_GE(payload.getErrorCount(), before + 2u);
}

// =============================================================================
// WatchdogComponent v3.0
// =============================================================================
class WdFixture : public ::testing::Test {
protected:
    WatchdogComponent wd{"Supervisor", 1};
    InputPort<EventPacket> event_dest;
    OutputPort<float>      batt_out;
    void SetUp() override {
        TimeService::initEpoch();
        wd.event_out.connect(&event_dest);
        batt_out.connect(&wd.battery_level_in);
        wd.init();
        EventPacket e{};
        while (event_dest.tryConsume(e)) {} // drain init event
    }
};

TEST_F(WdFixture, SafeModeOnCriticalComponent) {
    FakeComponent c; c.forced = HealthStatus::CRITICAL_FAILURE;
    wd.registerSubsystem(&c);
    wd.step();
    EXPECT_EQ(wd.getMissionState(), MissionState::SAFE_MODE);
}

TEST_F(WdFixture, SafeModeOnLowBattery) {
    batt_out.send(4.0f);
    wd.step();
    EXPECT_EQ(wd.getMissionState(), MissionState::SAFE_MODE);
}

TEST_F(WdFixture, DegradedFromWarning) {
    FakeComponent c; c.forced = HealthStatus::WARNING;
    wd.registerSubsystem(&c);
    wd.step();
    EXPECT_EQ(wd.getMissionState(), MissionState::DEGRADED);
}

TEST_F(WdFixture, DegradedAutoRecovery) {
    FakeComponent c; c.forced = HealthStatus::WARNING;
    wd.registerSubsystem(&c);
    batt_out.send(14.0f);
    wd.step();
    EXPECT_EQ(wd.getMissionState(), MissionState::DEGRADED);
    c.forced = HealthStatus::NOMINAL;
    batt_out.send(80.0f);
    wd.step();
    EXPECT_EQ(wd.getMissionState(), MissionState::NOMINAL);
}

TEST_F(WdFixture, EmergencyOnVeryLowBattery) {
    batt_out.send(1.5f);
    wd.step();
    EXPECT_EQ(wd.getMissionState(), MissionState::EMERGENCY);
}

TEST_F(WdFixture, HeartbeatAtCycle10) {
    for (int i = 0; i < 10; ++i) wd.step();
    bool found_hb = false;
    EventPacket e{};
    while (event_dest.tryConsume(e))
        if (std::string_view(e.message.data()).find("HB:") != std::string_view::npos)
            found_hb = true;
    EXPECT_TRUE(found_hb);
}

TEST_F(WdFixture, SchedulerHealthEmitsOnNewDrops) {
    wd.pollSchedulerHealth(3);
    EventPacket e{};
    EXPECT_TRUE(event_dest.hasNew());
    event_dest.tryConsume(e);
    EXPECT_EQ(e.severity, Severity::WARNING);
}

TEST_F(WdFixture, SchedulerHealthSilentWhenNoNewDrops) {
    wd.pollSchedulerHealth(5);
    EventPacket e{};
    while (event_dest.tryConsume(e)) {}
    wd.pollSchedulerHealth(5); // same count — no new drops
    EXPECT_FALSE(event_dest.hasNew());
}

TEST_F(WdFixture, InjectBatteryLevel_v3) {
    // DV-FDIR-01: injectBatteryLevel is a v3 test hook for HIL fault injection
    wd.injectBatteryLevel(4.0f);
    EXPECT_EQ(wd.getMissionState(), MissionState::SAFE_MODE);
}

TEST_F(WdFixture, RestartsCriticalActiveComponent) {
    FakeActiveCritical active_fault;
    wd.registerSubsystem(&active_fault);
    wd.step();
    EXPECT_GT(active_fault.join_count, 0u);
    EXPECT_GT(active_fault.start_count, 0u);
}

TEST_F(WdFixture, EscalatesEmergencyAfterRestartBudgetExhausted) {
    FakeActiveCritical active_fault;
    wd.registerSubsystem(&active_fault);
    for (uint32_t i = 0; i <= MAX_RESTARTS_PER_COMPONENT; ++i) {
        wd.step();
    }
    EXPECT_EQ(wd.getMissionState(), MissionState::EMERGENCY);
    EXPECT_EQ(active_fault.start_count, MAX_RESTARTS_PER_COMPONENT);
    EXPECT_EQ(active_fault.join_count, MAX_RESTARTS_PER_COMPONENT);
}

TEST_F(WdFixture, RestartClearsErrorCounter) {
    FakeActiveThreshold active_fault;
    wd.registerSubsystem(&active_fault);
    ASSERT_GE(active_fault.getErrorCount(), Component::CRITICAL_THRESHOLD);
    wd.step();
    EXPECT_EQ(active_fault.getErrorCount(), 0u);
    EXPECT_GT(active_fault.start_count, 0u);
    EXPECT_GT(active_fault.join_count, 0u);
}

TEST_F(WdFixture, PollPortOverflowEmitsWarning) {
    InputPort<CommandPacket, 2> cmd_port;
    OutputPort<CommandPacket> cmd_src;
    cmd_src.connect(&cmd_port);
    cmd_src.send(CommandPacket{200, 1, 0.0f});
    cmd_src.send(CommandPacket{200, 1, 0.0f});
    cmd_src.send(CommandPacket{200, 1, 0.0f}); // overflow

    const auto errors_before = wd.getErrorCount();
    wd.pollPortOverflow(cmd_port, "cmd_port");

    bool found_overflow_event = false;
    EventPacket evt{};
    while (event_dest.tryConsume(evt)) {
        if (std::string_view(evt.message.data()).find("OVF:cmd_port") != std::string_view::npos) {
            found_overflow_event = true;
        }
    }
    EXPECT_TRUE(found_overflow_event);
    EXPECT_GT(wd.getErrorCount(), errors_before);
}

TEST_F(WdFixture, PeriodicTmrScrubRepairsSingleFault) {
    TmrStore<float> store{42.0F};
    store.registerWithFdir("wd_periodic_scrub_store");
    store.injectFaultForTesting(-1.0F);
    ASSERT_FALSE(store.isSane());

    for (uint32_t i = 0; i < PARAM_CHECK_INTERVAL_CYCLES; ++i) {
        wd.step();
    }

    EXPECT_TRUE(store.isSane());
    EXPECT_FLOAT_EQ(store.read(), 42.0F);
}

TEST(WatchdogThresholds, UsesConfiguredBatteryThresholds) {
    WatchdogComponent wd{"Supervisor", 1};
    InputPort<EventPacket> events;
    wd.event_out.connect(&events);
    wd.init();

    EventPacket evt{};
    while (events.tryConsume(evt)) {}

    TmrStore<float> emergency_pct{10.0F};
    TmrStore<float> safe_mode_pct{20.0F};
    TmrStore<float> degraded_pct{30.0F};
    wd.setBatteryThresholdSources(&emergency_pct, &safe_mode_pct, &degraded_pct);
    ASSERT_TRUE(wd.hasBatteryThresholdSources());

    wd.injectBatteryLevel(25.0F);
    EXPECT_EQ(wd.getMissionState(), MissionState::DEGRADED);

    wd.injectBatteryLevel(15.0F);
    EXPECT_EQ(wd.getMissionState(), MissionState::SAFE_MODE);

    wd.injectBatteryLevel(9.0F);
    EXPECT_EQ(wd.getMissionState(), MissionState::EMERGENCY);
}

TEST(WatchdogThresholds, InvalidThresholdConfigFallsBackToDefaults) {
    WatchdogComponent wd{"Supervisor", 1};
    InputPort<EventPacket> events;
    wd.event_out.connect(&events);
    wd.init();

    EventPacket evt{};
    while (events.tryConsume(evt)) {}

    TmrStore<float> emergency_pct{10.0F};
    TmrStore<float> safe_mode_pct{5.0F};
    TmrStore<float> degraded_pct{4.0F}; // invalid ordering
    wd.setBatteryThresholdSources(&emergency_pct, &safe_mode_pct, &degraded_pct);

    const auto errors_before = wd.getErrorCount();
    wd.injectBatteryLevel(4.5F); // with defaults, this enters SAFE_MODE
    EXPECT_EQ(wd.getMissionState(), MissionState::SAFE_MODE);
    EXPECT_GT(wd.getErrorCount(), errors_before);

    bool saw_invalid_threshold_event = false;
    while (events.tryConsume(evt)) {
        if (std::string_view(evt.message.data()).find("Invalid battery") != std::string_view::npos) {
            saw_invalid_threshold_event = true;
        }
    }
    EXPECT_TRUE(saw_invalid_threshold_event);
}

TEST(WatchdogThresholds, RecoveryUsesHysteresisBand) {
    WatchdogComponent wd{"Supervisor", 1};
    InputPort<EventPacket> events;
    wd.event_out.connect(&events);
    wd.init();

    EventPacket evt{};
    while (events.tryConsume(evt)) {}

    TmrStore<float> emergency_pct{10.0F};
    TmrStore<float> safe_mode_pct{20.0F};
    TmrStore<float> degraded_pct{30.0F};
    wd.setBatteryThresholdSources(&emergency_pct, &safe_mode_pct, &degraded_pct);

    wd.injectBatteryLevel(29.0F);
    EXPECT_EQ(wd.getMissionState(), MissionState::DEGRADED);

    // 30.5% is above degraded threshold, but below 30% + 2% recovery hysteresis.
    wd.injectBatteryLevel(30.5F);
    wd.step();
    EXPECT_EQ(wd.getMissionState(), MissionState::DEGRADED);

    wd.injectBatteryLevel(32.5F);
    wd.step();
    EXPECT_EQ(wd.getMissionState(), MissionState::NOMINAL);
}

TEST(WatchdogComponent, InitBackpressureIncrementsErrorCount) {
    TimeService::initEpoch();
    WatchdogComponent wd{"Supervisor", 1};
    InputPort<EventPacket, 1> events{};
    OutputPort<EventPacket> prefill{};
    wd.event_out.connect(&events);
    prefill.connect(&events);

    ASSERT_TRUE(prefill.send(EventPacket::create(Severity::INFO, 99u, "prefill")));
    const uint32_t before = wd.getErrorCount();
    wd.init();

    EXPECT_GT(wd.getErrorCount(), before);
}

// =============================================================================
// TopologyManager
// =============================================================================
TEST(TopologyManager, VerifyPassesAfterWire) {
    TimeService::initEpoch();
    hal::MockI2c bus;
    TopologyManager topo(bus);
    topo.wire();
    topo.registerFdir();
    EXPECT_TRUE(topo.verify());
}

TEST(TopologyManager, VerifyFailsWithoutWire) {
    TimeService::initEpoch();
    hal::MockI2c bus;
    TopologyManager topo(bus);
    EXPECT_FALSE(topo.verify());
}

TEST(TopologyManager, RegisterAllPopulatesRGE) {
    TimeService::initEpoch();
    hal::MockI2c bus;
    TopologyManager topo(bus);
    topo.wire();
    topo.registerFdir();
    RateGroupExecutive rge;
    topo.registerAll(rge);
    EXPECT_GT(rge.totalCount(), 0u);
}

TEST(TopologyManager, WireSetsWatchdogThresholdSources) {
    TimeService::initEpoch();
    hal::MockI2c bus;
    TopologyManager topo(bus);
    topo.wire();
    EXPECT_TRUE(topo.watchdog.hasBatteryThresholdSources());
}

TEST(TopologyManager, VerifyFailsOnBrokenEventSourceConnection) {
    TimeService::initEpoch();
    hal::MockI2c bus;
    TopologyManager topo(bus);
    topo.wire();

    InputPort<EventPacket> wrong_sink{};
    topo.watchdog.event_out.connect(&wrong_sink);

    EXPECT_FALSE(topo.verify());
}

TEST(TopologyManager, VerifyFailsOnBrokenTelemetryConnection) {
    TimeService::initEpoch();
    hal::MockI2c bus;
    TopologyManager topo(bus);
    topo.wire();

    InputPort<Serializer::ByteArray> wrong_sink{};
    topo.cmd_sequencer.telemetry_out.connect(&wrong_sink);

    EXPECT_FALSE(topo.verify());
}

// =============================================================================
// TelemetryBridge accessor (no socket needed)
// =============================================================================
TEST(TelemetryBridge, RejectedCountStartsAtZero) {
    ScopedReplaySeqFile replay_path_env{};
    TelemetryBridge bridge{"RadioLink", 20};
    EXPECT_EQ(bridge.getRejectedCount(), 0u);
}

TEST(ActiveComponent, ThreadLifecycleStartStop) {
    ScopedReplaySeqFile replay_path_env{};
    TelemetryBridge bridge{"RadioLink", 20};
    EXPECT_FALSE(bridge.isThreadRunning());
    bridge.startThread();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_TRUE(bridge.isThreadRunning());
    bridge.joinThread();
    EXPECT_FALSE(bridge.isThreadRunning());
}

TEST(TelemetryBridge, RejectsNonCanonicalFrameLength) {
    ScopedReplaySeqFile replay_path_env{};
    constexpr uint16_t DL_PORT = 19011;
    constexpr uint16_t UL_PORT = 19012;

    TelemetryBridge bridge{"RadioLink", 20, DL_PORT, UL_PORT};
    InputPort<CommandPacket, 64> dest{};
    bridge.command_out.connect(&dest);

    auto frame = buildUplinkFrame(10, CommandPacket{200, 1, 0.0f});
    std::array<uint8_t, sizeof(CcsdsHeader) + sizeof(CommandPacket) + 4> oversized{};
    std::memcpy(oversized.data(), frame.data(), frame.size());
    oversized.back() = 0xAA;

    EXPECT_FALSE(bridge.ingestUplinkFrameForTest(oversized.data(), oversized.size()));

    EXPECT_FALSE(dest.hasNew());
    EXPECT_GT(bridge.getRejectedCount(), 0u);
}

TEST(TelemetryBridge, RejectsReplaySequence) {
    ScopedReplaySeqFile replay_path_env{};
    constexpr uint16_t DL_PORT = 19013;
    constexpr uint16_t UL_PORT = 19014;

    TelemetryBridge bridge{"RadioLink", 20, DL_PORT, UL_PORT};
    InputPort<CommandPacket, 64> dest{};
    bridge.command_out.connect(&dest);

    auto frame1 = buildUplinkFrame(50, CommandPacket{200, 1, 0.0f});
    auto frame2 = buildUplinkFrame(50, CommandPacket{200, 1, 0.0f}); // replay

    EXPECT_TRUE(bridge.ingestUplinkFrameForTest(frame1.data(), frame1.size()));
    EXPECT_FALSE(bridge.ingestUplinkFrameForTest(frame2.data(), frame2.size()));

    int accepted = 0;
    CommandPacket cmd{};
    while (dest.tryConsume(cmd)) {
        ++accepted;
    }
    EXPECT_EQ(accepted, 1);
    EXPECT_EQ(bridge.getRejectedCount(), 1u);
}

TEST(TelemetryBridge, DispatchFailureDoesNotAdvanceReplaySequence) {
    ScopedReplaySeqFile replay_path_env{};
    constexpr uint16_t DL_PORT = 19041;
    constexpr uint16_t UL_PORT = 19042;

    TelemetryBridge bridge{"RadioLink", 20, DL_PORT, UL_PORT};
    auto frame = buildUplinkFrame(321, CommandPacket{200, 1, 0.0f});

    // No command sink connected yet -> dispatch failure.
    EXPECT_FALSE(bridge.ingestUplinkFrameForTest(frame.data(), frame.size()));
    EXPECT_EQ(bridge.getRejectedCount(), 1u);
    EXPECT_GT(bridge.getErrorCount(), 0u);

    InputPort<CommandPacket, 64> dest{};
    bridge.command_out.connect(&dest);

    // Same sequence should still be accepted now (not treated as replay).
    EXPECT_TRUE(bridge.ingestUplinkFrameForTest(frame.data(), frame.size()));
    EXPECT_TRUE(dest.hasNew());
    EXPECT_EQ(bridge.getRejectedCount(), 1u);
}

TEST(TelemetryBridge, EnforcesMaxCommandsPerTick) {
    ScopedReplaySeqFile replay_path_env{};
    constexpr uint16_t DL_PORT = 19015;
    constexpr uint16_t UL_PORT = 19016;

    TelemetryBridge bridge{"RadioLink", 20, DL_PORT, UL_PORT};
    InputPort<CommandPacket, 64> dest{};
    bridge.command_out.connect(&dest);

    constexpr size_t EXTRA_CMDS = 5;
    const size_t total = TelemetryBridge::MAX_CMDS_PER_TICK + EXTRA_CMDS;
    std::vector<std::vector<uint8_t>> frames;
    frames.reserve(total);
    for (size_t i = 0; i < total; ++i) {
        auto frame = buildUplinkFrame(static_cast<uint16_t>(200 + i), CommandPacket{200, 1, 0.0f});
        frames.emplace_back(frame.begin(), frame.end());
    }

    const size_t processed = bridge.ingestUplinkBatchForTest(frames);
    EXPECT_EQ(processed, TelemetryBridge::MAX_CMDS_PER_TICK);

    size_t consumed_first = 0;
    CommandPacket cmd{};
    while (dest.tryConsume(cmd)) {
        ++consumed_first;
    }
    EXPECT_EQ(consumed_first, TelemetryBridge::MAX_CMDS_PER_TICK);

    std::vector<std::vector<uint8_t>> tail(
        std::next(frames.begin(), static_cast<std::ptrdiff_t>(processed)),
        frames.end());
    const size_t processed_tail = bridge.ingestUplinkBatchForTest(tail);
    EXPECT_EQ(processed_tail, EXTRA_CMDS);

    size_t consumed_second = 0;
    while (dest.tryConsume(cmd)) {
        ++consumed_second;
    }
    EXPECT_EQ(consumed_second, EXTRA_CMDS);
}

TEST(TelemetryBridge, RejectsInvalidHeaderFields) {
    ScopedReplaySeqFile replay_path_env{};
    constexpr uint16_t DL_PORT = 19017;
    constexpr uint16_t UL_PORT = 19018;

    TelemetryBridge bridge{"RadioLink", 20, DL_PORT, UL_PORT};
    InputPort<CommandPacket, 64> dest{};
    bridge.command_out.connect(&dest);

    auto bad_sync = buildUplinkFrame(301, CommandPacket{200, 1, 0.0f});
    auto bad_apid = buildUplinkFrame(302, CommandPacket{200, 1, 0.0f});
    auto bad_len = buildUplinkFrame(303, CommandPacket{200, 1, 0.0f});

    CcsdsHeader hdr{};

    std::memcpy(&hdr, bad_sync.data(), sizeof(hdr));
    hdr.sync_word = 0xDEADBEEFu;
    std::memcpy(bad_sync.data(), &hdr, sizeof(hdr));

    std::memcpy(&hdr, bad_apid.data(), sizeof(hdr));
    hdr.apid = Apid::EVENT;
    std::memcpy(bad_apid.data(), &hdr, sizeof(hdr));

    std::memcpy(&hdr, bad_len.data(), sizeof(hdr));
    hdr.payload_len = static_cast<uint16_t>(sizeof(CommandPacket) - 1u);
    std::memcpy(bad_len.data(), &hdr, sizeof(hdr));

    EXPECT_FALSE(bridge.ingestUplinkFrameForTest(bad_sync.data(), bad_sync.size()));
    EXPECT_FALSE(bridge.ingestUplinkFrameForTest(bad_apid.data(), bad_apid.size()));
    EXPECT_FALSE(bridge.ingestUplinkFrameForTest(bad_len.data(), bad_len.size()));

    EXPECT_FALSE(dest.hasNew());
    EXPECT_EQ(bridge.getRejectedCount(), 3u);
}

TEST(TelemetryBridge, RejectsTruncatedFrame) {
    ScopedReplaySeqFile replay_path_env{};
    constexpr uint16_t DL_PORT = 19019;
    constexpr uint16_t UL_PORT = 19020;

    TelemetryBridge bridge{"RadioLink", 20, DL_PORT, UL_PORT};
    std::array<uint8_t, 4> frame{};
    EXPECT_FALSE(bridge.ingestUplinkFrameForTest(frame.data(), frame.size()));
    EXPECT_EQ(bridge.getRejectedCount(), 1u);
}

TEST(TelemetryBridge, AcceptsSequenceWrapFromHighToLow) {
    ScopedReplaySeqFile replay_path_env{};
    constexpr uint16_t DL_PORT = 19021;
    constexpr uint16_t UL_PORT = 19022;

    TelemetryBridge bridge{"RadioLink", 20, DL_PORT, UL_PORT};
    InputPort<CommandPacket, 64> dest{};
    bridge.command_out.connect(&dest);

    constexpr uint16_t SEQ_HIGH = 0xFFFEu;
    constexpr uint16_t SEQ_LOW = 0x0002u;
    auto frame1 = buildUplinkFrame(SEQ_HIGH, CommandPacket{200, 1, 0.0f});
    auto frame2 = buildUplinkFrame(SEQ_LOW, CommandPacket{200, 1, 0.0f});

    EXPECT_TRUE(bridge.ingestUplinkFrameForTest(frame1.data(), frame1.size()));
    EXPECT_TRUE(bridge.ingestUplinkFrameForTest(frame2.data(), frame2.size()));

    size_t accepted = 0;
    CommandPacket cmd{};
    while (dest.tryConsume(cmd)) {
        ++accepted;
    }
    EXPECT_EQ(accepted, 2u);
    EXPECT_EQ(bridge.getRejectedCount(), 0u);
}

// =============================================================================
// HeapGuard  (DV-MEM-01)
// =============================================================================
TEST(HeapGuard, ArmAndIsArmed) {
    // Cannot arm in a unit test that runs the full suite — would prevent
    // allocations in later tests. Test the flag logic only.
    // The full arm() abort test is a dedicated death test binary.
    bool was_armed = HeapGuard::isArmed();
    // Only verify the API compiles and returns a consistent value.
    EXPECT_EQ(HeapGuard::isArmed(), was_armed);
}

TEST(HeapGuard, NewAfterArmTriggersFatal) {
    ASSERT_DEATH(
        {
            HeapGuard::arm();
            auto* p = new int(42);
            (void)p;
        },
        "HeapGuard");
}

// =============================================================================
// TmrStore  (DV-TMR-01, DV-TMR-02)
// =============================================================================
TEST(TmrStore, WriteAndRead) {
    // DV-TMR-01: majority vote returns written value when all copies agree
    TmrStore<float> s{0.0f};
    s.write(3.14f);
    EXPECT_FLOAT_EQ(s.read(), 3.14f);
}

TEST(TmrStore, IsSaneAfterWrite) {
    TmrStore<float> s{0.0f};
    s.write(1.0f);
    EXPECT_TRUE(s.isSane());
}

TEST(TmrStore, DefaultInitialValue) {
    TmrStore<float> s{42.0f};
    EXPECT_FLOAT_EQ(s.read(), 42.0f);
}

TEST(TmrStore, MajorityVoteHealsOneSEU) {
    // DV-TMR-01: single-event upset in one copy self-heals
    TmrStore<float> s{7.0f};
    s.injectFaultForTesting(99.0f); // corrupt copy[0]
    float voted = s.read();
    EXPECT_FLOAT_EQ(voted, 7.0f);  // vote returns correct value
    EXPECT_TRUE(s.isSane());
    // After read(), all three copies should agree again
    EXPECT_FLOAT_EQ(s.read(), 7.0f);
}

TEST(TmrStore, AllThreeDifferIsNotSane) {
    // DV-TMR-02: double upset detected via isSane()
    TmrStore<float> s{1.0f};
    s.injectDoubleUpset(2.0f, 3.0f);
    EXPECT_FALSE(s.isSane());
}

TEST(TmrStore, IntType) {
    TmrStore<uint32_t> s{100u};
    s.write(200u);
    EXPECT_EQ(s.read(), 200u);
}

TEST(TmrStore, TmrRegistryScrubAll) {
    TmrStore<float> a{1.0f}, b{2.0f};
    TmrRegistry& reg = TmrRegistry::getInstance();
    reg.registerStore(&a);
    reg.registerStore(&b);
    // Inject fault then scrub — should not throw
    a.injectFaultForTesting(99.0f);
    EXPECT_NO_THROW(reg.scrubAll());
    EXPECT_FLOAT_EQ(a.read(), 1.0f); // healed
}

TEST(TmrStore, RegistryIgnoresDuplicateRegistration) {
    TmrRegistry& reg = TmrRegistry::getInstance();
    const size_t before = reg.registeredCount();

    TmrStore<float> store{5.0f};
    store.registerWithFdir("dup_store");
    const size_t after_first = reg.registeredCount();
    store.registerWithFdir("dup_store");
    const size_t after_second = reg.registeredCount();

    EXPECT_EQ(after_first, before + 1u);
    EXPECT_EQ(after_second, after_first);
}

TEST(TmrStore, RegistryRejectsNullEntries) {
    TmrRegistry& reg = TmrRegistry::getInstance();
    const size_t before = reg.registeredCount();
    reg.registerParam(nullptr, nullptr, nullptr);
    EXPECT_EQ(reg.registeredCount(), before);
}

TEST(TmrStore, ScopedStoreAutoUnregisters) {
    TmrRegistry& reg = TmrRegistry::getInstance();
    const size_t before = reg.registeredCount();
    {
        TmrStore<float> scoped{7.0f};
        scoped.registerWithFdir("scoped_store");
        EXPECT_EQ(reg.registeredCount(), before + 1u);
    }
    EXPECT_EQ(reg.registeredCount(), before);
}

// =============================================================================
// COBS framing  (DV-COMM-02)
// =============================================================================
TEST(Cobs, RoundtripShortPayload) {
    // DV-COMM-02: encode then decode must recover the original data
    std::array<uint8_t, 8> payload{1, 2, 3, 4, 5, 0, 7, 8};
    auto frame = cobs::CobsFrame::encode(payload.data(), payload.size());
    std::array<uint8_t, 8> decoded{};
    size_t decoded_len = 0;
    bool ok = cobs::CobsFrame::decode(frame, decoded.data(), decoded.size(), decoded_len);
    EXPECT_TRUE(ok);
    EXPECT_EQ(decoded_len, payload.size());
    EXPECT_EQ(decoded, payload);
}

TEST(Cobs, NoZerosInEncodedData) {
    // DV-COMM-02: 0x00 must not appear in the encoded payload (only as delimiter)
    std::array<uint8_t, 12> payload{};
    for (uint8_t i = 0; i < 12; ++i) payload[i] = i; // includes 0x00 at index 0
    auto frame = cobs::CobsFrame::encode(payload.data(), payload.size());
    // Skip the trailing delimiter
    for (size_t i = 0; i + 1 < frame.encoded_len; ++i) {
        EXPECT_NE(frame.data[i], 0u)
            << "Zero byte found at encoded position " << i;
    }
}

TEST(Cobs, DelimiterIsLastByte) {
    std::array<uint8_t, 4> payload{10, 20, 30, 40};
    auto frame = cobs::CobsFrame::encode(payload.data(), payload.size());
    EXPECT_EQ(frame.data[frame.encoded_len - 1], 0x00u);
}

TEST(Cobs, DecodeFailsOnTruncated) {
    cobs::CobsFrame bad;
    bad.encoded_len = 1; // truncated — no valid COBS structure
    bad.data[0] = 0xFFu;
    std::array<uint8_t, 16> out{};
    size_t len = 0;
    bool ok = cobs::CobsFrame::decode(bad, out.data(), out.size(), len);
    EXPECT_FALSE(ok);
}

TEST(Cobs, RoundtripEmptyPayload) {
    constexpr uint8_t PAYLOAD = 0xAB;
    auto frame = cobs::CobsFrame::encode(&PAYLOAD, 0);
    ASSERT_EQ(frame.encoded_len, 2u);
    EXPECT_EQ(frame.data[0], 0x01u);
    EXPECT_EQ(frame.data[1], 0x00u);

    std::array<uint8_t, 1> decoded{};
    size_t decoded_len = 123;
    const bool ok = cobs::CobsFrame::decode(frame, decoded.data(), decoded.size(), decoded_len);
    EXPECT_TRUE(ok);
    EXPECT_EQ(decoded_len, 0u);
}

TEST(Cobs, DecodeFailsWhenDelimiterMissing) {
    cobs::CobsFrame bad;
    bad.encoded_len = 3;
    bad.data[0] = 0x02u;
    bad.data[1] = 0x11u;
    bad.data[2] = 0x22u; // must be 0x00 delimiter

    std::array<uint8_t, 8> out{};
    size_t decoded_len = 0;
    EXPECT_FALSE(cobs::CobsFrame::decode(bad, out.data(), out.size(), decoded_len));
    EXPECT_EQ(decoded_len, 0u);
}

TEST(Cobs, RoundtripRawPayloadAt254Boundary) {
    std::array<uint8_t, 254> payload{};
    for (size_t i = 0; i < payload.size(); ++i) {
        // Non-zero pattern forces the 0xFF code-path in COBS encode.
        payload[i] = static_cast<uint8_t>((i % 254u) + 1u);
    }

    std::array<uint8_t, cobs::encodedMaxSize(payload.size())> encoded{};
    const size_t encoded_len = cobs::encode(
        payload.data(), payload.size(), encoded.data(), encoded.size());
    ASSERT_GT(encoded_len, 0u);
    ASSERT_EQ(encoded[encoded_len - 1], 0x00u);

    std::array<uint8_t, payload.size()> decoded{};
    const size_t decoded_len = cobs::decode(
        encoded.data(), encoded_len - 1u, decoded.data(), decoded.size());
    EXPECT_EQ(decoded_len, payload.size());
    EXPECT_EQ(decoded, payload);
}

TEST(Cobs, RandomizedCorpusRoundtrip) {
    std::mt19937 rng(0xC0B5u);
    std::uniform_int_distribution<size_t> len_dist(0u, 22u);
    std::uniform_int_distribution<int> byte_dist(0, 255);

    for (size_t iter = 0; iter < 256u; ++iter) {
        const size_t len = len_dist(rng);
        std::array<uint8_t, 22> payload{};
        for (size_t i = 0; i < len; ++i) {
            payload[i] = static_cast<uint8_t>(byte_dist(rng));
        }

        auto frame = cobs::CobsFrame::encode(payload.data(), len);
        ASSERT_GT(frame.encoded_len, 0u);

        std::array<uint8_t, 22> decoded{};
        size_t decoded_len = 0;
        const bool ok = cobs::CobsFrame::decode(frame, decoded.data(), decoded.size(), decoded_len);
        ASSERT_TRUE(ok);
        ASSERT_EQ(decoded_len, len);
        for (size_t i = 0; i < len; ++i) {
            EXPECT_EQ(decoded[i], payload[i]);
        }
    }
}

// =============================================================================
// MissionFsm  (DV-SEC-02)
// =============================================================================
TEST(MissionFsm, NominalAllowsAll) {
    EXPECT_TRUE(MissionFsm::isAllowed(MissionState::NOMINAL, OpClass::HOUSEKEEPING));
    EXPECT_TRUE(MissionFsm::isAllowed(MissionState::NOMINAL, OpClass::OPERATIONAL));
    EXPECT_TRUE(MissionFsm::isAllowed(MissionState::NOMINAL, OpClass::RESTRICTED));
}

TEST(MissionFsm, SafeModeBlocksOperational) {
    EXPECT_TRUE( MissionFsm::isAllowed(MissionState::SAFE_MODE, OpClass::HOUSEKEEPING));
    EXPECT_FALSE(MissionFsm::isAllowed(MissionState::SAFE_MODE, OpClass::OPERATIONAL));
    EXPECT_FALSE(MissionFsm::isAllowed(MissionState::SAFE_MODE, OpClass::RESTRICTED));
}

TEST(MissionFsm, EmergencyBlocksAll) {
    EXPECT_FALSE(MissionFsm::isAllowed(MissionState::EMERGENCY, OpClass::HOUSEKEEPING));
    EXPECT_FALSE(MissionFsm::isAllowed(MissionState::EMERGENCY, OpClass::OPERATIONAL));
    EXPECT_FALSE(MissionFsm::isAllowed(MissionState::EMERGENCY, OpClass::RESTRICTED));
}

TEST(MissionFsm, DegradedAllowsHousekeepingAndOperational) {
    EXPECT_TRUE( MissionFsm::isAllowed(MissionState::DEGRADED, OpClass::HOUSEKEEPING));
    EXPECT_TRUE( MissionFsm::isAllowed(MissionState::DEGRADED, OpClass::OPERATIONAL));
    EXPECT_FALSE(MissionFsm::isAllowed(MissionState::DEGRADED, OpClass::RESTRICTED));
}

TEST(MissionFsm, BootAllowsOnlyHousekeeping) {
    EXPECT_TRUE( MissionFsm::isAllowed(MissionState::BOOT, OpClass::HOUSEKEEPING));
    EXPECT_FALSE(MissionFsm::isAllowed(MissionState::BOOT, OpClass::OPERATIONAL));
    EXPECT_FALSE(MissionFsm::isAllowed(MissionState::BOOT, OpClass::RESTRICTED));
}

TEST(TimeService, OverflowWarningThreshold) {
    EXPECT_FALSE(TimeService::isNearOverflow(TimeService::MET_WARN_THRESHOLD_MS));
    EXPECT_TRUE(TimeService::isNearOverflow(TimeService::MET_WARN_THRESHOLD_MS + 1));
}

TEST(TimeService, ReadyAfterInitEpoch) {
    TimeService::initEpoch();
    EXPECT_TRUE(TimeService::isReady());
}

// =============================================================================
// RateGroupExecutive  (DV-ARCH-03)
// =============================================================================
TEST(RateGroupExecutive, RegistrationCounts) {
    FakeComponent f1, f2, f3;
    RateGroupExecutive rge;
    rge.registerFast(&f1);
    rge.registerNorm(&f2);
    rge.registerSlow(&f3);
    EXPECT_EQ(rge.fastCount(),   1u);
    EXPECT_EQ(rge.normCount(),   1u);
    EXPECT_EQ(rge.slowCount(),   1u);
    EXPECT_EQ(rge.activeCount(), 0u);
    EXPECT_EQ(rge.totalCount(),  3u);
}

TEST(RateGroupExecutive, InitAllCallsComponentInit) {
    FakeComponent f;
    EXPECT_FALSE(f.initialized);
    RateGroupExecutive rge;
    rge.registerNorm(&f);
    rge.initAll();
    EXPECT_TRUE(f.initialized);
}

TEST(RateGroupExecutive, RegisterComponentRoutesPassiveToNorm) {
    FakeComponent f;
    RateGroupExecutive rge;
    rge.registerComponent(&f);
    EXPECT_EQ(rge.normCount(), 1u);
    EXPECT_EQ(rge.activeCount(), 0u);
}

TEST(RateGroupExecutive, RejectsDuplicateAcrossTiers) {
    FakeComponent f;
    RateGroupExecutive rge;
    rge.registerFast(&f);
    rge.registerNorm(&f);
    rge.registerSlow(&f);
    EXPECT_EQ(rge.fastCount(), 1u);
    EXPECT_EQ(rge.normCount(), 0u);
    EXPECT_EQ(rge.slowCount(), 0u);
    EXPECT_EQ(rge.totalCount(), 1u);
}

TEST(RateGroupExecutive, RejectsDuplicateActiveRegistration) {
    FakeActiveCritical active;
    RateGroupExecutive rge;
    rge.registerComponent(&active);
    rge.registerComponent(&active);
    EXPECT_EQ(rge.activeCount(), 1u);
    EXPECT_EQ(rge.totalCount(), 1u);
}

TEST(RateGroupExecutive, RejectsNullRegistration) {
    RateGroupExecutive rge;
    rge.registerFast(nullptr);
    rge.registerNorm(nullptr);
    rge.registerSlow(nullptr);
    rge.registerComponent(nullptr);
    EXPECT_EQ(rge.totalCount(), 0u);
}

TEST(RateGroupExecutive, FrameDropsStartAtZero) {
    RateGroupExecutive rge;
    EXPECT_EQ(rge.getFastDrops(), 0u);
    EXPECT_EQ(rge.getNormDrops(), 0u);
}

TEST(Component, ReportHealthTransitionsAtThresholds) {
    FakeComponent c;
    EXPECT_EQ(c.reportHealth(), HealthStatus::NOMINAL);

    for (uint32_t i = 0; i < Component::WARNING_THRESHOLD; ++i) {
        c.recordError();
    }
    EXPECT_EQ(c.reportHealth(), HealthStatus::WARNING);

    for (uint32_t i = Component::WARNING_THRESHOLD; i < Component::CRITICAL_THRESHOLD; ++i) {
        c.recordError();
    }
    EXPECT_EQ(c.reportHealth(), HealthStatus::CRITICAL_FAILURE);

    c.clearErrors();
    EXPECT_EQ(c.reportHealth(), HealthStatus::NOMINAL);
}

TEST(Architecture, SingletonServiceIdentity) {
    ParamDb* p1 = &ParamDb::getInstance();
    ParamDb* p2 = &ParamDb::getInstance();
    EXPECT_EQ(p1, p2);
}

// =============================================================================
// Scheduler (v2 legacy — kept for regression)
// =============================================================================
TEST(Scheduler, RegisterAndInit) {
    Scheduler sys;
    FakeComponent c;
    sys.registerComponent(&c);
    sys.initAll();
    EXPECT_TRUE(c.initialized);
}

TEST(Scheduler, MaxComponentsLimit) {
    Scheduler sys;
    std::vector<FakeComponent> comps(MAX_COMPONENTS);
    for (auto& c : comps) sys.registerComponent(&c);
    FakeComponent overflow;
    sys.registerComponent(&overflow); // should silently ignore
    sys.initAll();
    EXPECT_FALSE(overflow.initialized);
}

TEST(Scheduler, NullRegistrationIgnored) {
    Scheduler sys;
    sys.registerComponent(nullptr);
    sys.initAll();
    EXPECT_EQ(sys.getFrameDropCount(), 0u);
}

TEST(Scheduler, ZeroHzLoopReturnsWithoutStartingThreads) {
    Scheduler sys;
    FakeActiveCritical active;
    sys.registerComponent(&active);
    sys.executeLoop(0);
    EXPECT_EQ(active.start_count, 0u);
    EXPECT_EQ(active.join_count, 0u);
}

TEST(MemorySafety, StaticBoundsAreNonZero) {
    EXPECT_GT(DEFAULT_QUEUE_DEPTH, 0u);
    EXPECT_GT(MAX_COMPONENTS, 0u);
    EXPECT_GT(MAX_EVENT_SOURCES, 0u);
    EXPECT_GT(MAX_EVENT_LISTENERS, 0u);
    EXPECT_GT(MAX_TELEM_INPUTS, 0u);
    EXPECT_GT(MAX_TELEM_LISTENERS, 0u);
}

// =============================================================================
// HAL / IMU (ESP32-readiness via HAL contract tests)
// =============================================================================
TEST(HalMockI2c, ReadWriteContract) {
    TimeService::initEpoch();
    hal::MockI2c bus;
    uint8_t data[2]{0, 0};
    EXPECT_TRUE(bus.read(0x68, 0x3B, data, 2));
    EXPECT_TRUE(bus.write(0x68, 0x6B, data, 1));
}

TEST(ImuComponent, InitFailureEmitsCriticalEvent) {
    TimeService::initEpoch();
    TestI2cBus bus;
    bus.write_ok = false;
    ImuComponent imu{"IMU_01", 300, bus};
    InputPort<EventPacket> events;
    imu.event_out.connect(&events);
    imu.init();
    EXPECT_GT(imu.getErrorCount(), 0u);
    EventPacket evt{};
    ASSERT_TRUE(events.tryConsume(evt));
    EXPECT_EQ(evt.severity, Severity::CRITICAL);
}

TEST(ImuComponent, StepPublishesTelemetryOnReadSuccess) {
    TimeService::initEpoch();
    TestI2cBus bus;
    bus.write_ok = true;
    bus.read_ok = true;
    bus.read_hi = 0x01;
    bus.read_lo = 0x02;
    ImuComponent imu{"IMU_01", 300, bus};
    InputPort<Serializer::ByteArray> telem;
    imu.telemetry_out.connect(&telem);
    imu.init();
    imu.step();
    Serializer::ByteArray raw{};
    ASSERT_TRUE(telem.tryConsume(raw));
    const auto pkt = Serializer::unpackTelem(raw);
    EXPECT_EQ(pkt.component_id, 300u);
    EXPECT_FLOAT_EQ(pkt.data_payload, 258.0f);
}

TEST(ImuComponent, StepReadFailureIncrementsErrorCount) {
    TimeService::initEpoch();
    TestI2cBus bus;
    bus.read_ok = false;
    ImuComponent imu{"IMU_01", 300, bus};
    imu.init();
    const auto before = imu.getErrorCount();
    imu.step();
    EXPECT_GT(imu.getErrorCount(), before);
}

TEST(ImuComponent, UnknownCommandEmitsWarningEvent) {
    TimeService::initEpoch();
    TestI2cBus bus;
    ImuComponent imu{"IMU_01", 300, bus};
    InputPort<EventPacket> events;
    OutputPort<CommandPacket> cmd_sender;
    imu.event_out.connect(&events);
    cmd_sender.connect(&imu.cmd_in);
    imu.init();
    cmd_sender.send(CommandPacket{300, 99, 0.0f});
    imu.step();
    EventPacket evt{};
    ASSERT_TRUE(events.tryConsume(evt));
    EXPECT_EQ(evt.severity, Severity::WARNING);
}

// =============================================================================
// CommandHub hardening
// =============================================================================
TEST_F(CommandHubFixture, DuplicateRouteRegistrationIsRejected) {
    OutputPort<CommandPacket> route1;
    InputPort<CommandPacket> target1;
    route1.connect(&target1);
    hub.registerRoute(200, &route1);
    const uint32_t before = hub.getErrorCount();
    hub.registerRoute(200, &route1);
    EXPECT_EQ(hub.routeCount(), 1u);
    EXPECT_GT(hub.getErrorCount(), before);
}

TEST_F(CommandHubFixture, NullRouteRegistrationIsRejected) {
    const uint32_t before = hub.getErrorCount();
    hub.registerRoute(200, nullptr);
    EXPECT_EQ(hub.routeCount(), 0u);
    EXPECT_GT(hub.getErrorCount(), before);
}

TEST_F(CommandHubFixture, ConflictingDuplicatePolicyRegistrationPreservesOriginalPolicy) {
    InputPort<CommandPacket> target;
    OutputPort<CommandPacket> route;
    route.connect(&target);
    hub.registerRoute(46, &route);
    hub.registerCommandPolicy(46, 2, OpClass::HOUSEKEEPING);
    const uint32_t before = hub.getErrorCount();
    hub.registerCommandPolicy(46, 2, OpClass::OPERATIONAL);
    EXPECT_GT(hub.getErrorCount(), before);

    OutputPort<float> batt_src;
    batt_src.connect(&wd.battery_level_in);
    batt_src.send(4.0f);
    wd.step();
    ASSERT_EQ(wd.getMissionState(), MissionState::SAFE_MODE);

    OutputPort<CommandPacket> sender;
    sender.connect(&hub.cmd_input);
    sender.send(CommandPacket{46, 2, 0.0f});
    hub.step();
    EXPECT_TRUE(target.hasNew());
}

TEST_F(CommandHubFixture, RouteTableSaturationIsRejected) {
    std::array<InputPort<CommandPacket>, MAX_COMMAND_ROUTES + 1u> targets{};
    std::array<OutputPort<CommandPacket>, MAX_COMMAND_ROUTES + 1u> routes{};
    for (size_t i = 0; i < MAX_COMMAND_ROUTES; ++i) {
        routes.at(i).connect(&targets.at(i));
        hub.registerRoute(1000u + static_cast<uint32_t>(i), &routes.at(i));
    }
    const uint32_t before = hub.getErrorCount();
    routes.back().connect(&targets.back());
    hub.registerRoute(5000u, &routes.back());
    EXPECT_EQ(hub.routeCount(), MAX_COMMAND_ROUTES);
    EXPECT_GT(hub.getErrorCount(), before);
}

TEST(CommandHub, MissionStateSourceFlag) {
    CommandHub hub{"CmdHub", 11};
    EXPECT_FALSE(hub.hasMissionStateSource());
    WatchdogComponent wd{"Supervisor", 1};
    hub.setMissionStateSource(&wd);
    EXPECT_TRUE(hub.hasMissionStateSource());
}

TEST(TelemetryBridge, UsesConfiguredPorts) {
    ScopedReplaySeqFile replay_path_env{};
    TelemetryBridge bridge{"RadioLink", 20, 19101, 19102};
    EXPECT_EQ(bridge.getDownlinkPort(), 19101u);
    EXPECT_EQ(bridge.getUplinkPort(), 19102u);
}

TEST(TelemetryBridge, BindFailureDisablesUdpCommandIngress) {
    ScopedReplaySeqFile replay_path_env{};
    ScopedEnvVar enable_uplink_env(TelemetryBridge::ENABLE_UNAUTH_UPLINK_ENV, "1");
    TelemetryBridge bridge{"RadioLink", 20, 19103u, 19104u};
    const uint32_t errors_before = bridge.getErrorCount();

    bridge.setUdpIngressStateForTest(true, true);
    EXPECT_TRUE(bridge.isCommandIngressReady());
    bridge.simulateUdpBindFailureForTest();
    EXPECT_FALSE(bridge.isCommandIngressReady());
    EXPECT_GT(bridge.getErrorCount(), errors_before);
}

TEST(TelemetryBridge, PersistsReplaySequenceAcrossRestart) {
    const std::string replay_file = makeUniqueTestLogPath("test_replay_seq", ".db");
    (void)std::remove(replay_file.c_str());
    ScopedEnvVar replay_path_env(TelemetryBridge::REPLAY_SEQ_FILE_ENV, replay_file.c_str());

    constexpr uint16_t DL_PORT = 19105;
    constexpr uint16_t UL_PORT = 19106;

    {
        TelemetryBridge bridge{"RadioLink", 20, DL_PORT, UL_PORT};
        InputPort<CommandPacket, 64> dest{};
        bridge.command_out.connect(&dest);
        auto frame = buildUplinkFrame(77, CommandPacket{200, 1, 0.0f});
        EXPECT_TRUE(bridge.ingestUplinkFrameForTest(frame.data(), frame.size()));
    }

    {
        TelemetryBridge bridge{"RadioLink", 20, DL_PORT + 1, UL_PORT + 1};
        auto replayed_frame = buildUplinkFrame(77, CommandPacket{200, 1, 0.0f});
        EXPECT_FALSE(bridge.ingestUplinkFrameForTest(replayed_frame.data(), replayed_frame.size()));
        EXPECT_EQ(bridge.getRejectedCount(), 1u);
    }

    (void)std::remove(replay_file.c_str());
}

TEST(TelemetryBridge, RejectsOutOfRangeReplayStateFile) {
    const std::string replay_file = makeUniqueTestLogPath("test_replay_bad_seq", ".db");
    {
        std::ofstream out(replay_file, std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << "70000\n"; // > uint16 max
    }

    ScopedEnvVar replay_path_env(TelemetryBridge::REPLAY_SEQ_FILE_ENV, replay_file.c_str());
    TelemetryBridge bridge{"RadioLink", 20, 19111, 19112};
    InputPort<CommandPacket, 64> dest{};
    bridge.command_out.connect(&dest);
    EXPECT_GT(bridge.getErrorCount(), 0u);

    auto frame = buildUplinkFrame(1, CommandPacket{200, 1, 0.0f});
    EXPECT_TRUE(bridge.ingestUplinkFrameForTest(frame.data(), frame.size()));

    (void)std::remove(replay_file.c_str());
}

TEST(TelemetryBridge, PersistReplayStateFailureIncrementsErrors) {
    const std::string replay_file = makeUniqueTestLogPath("missing_dir/replay_seq", ".db");
    ScopedEnvVar replay_path_env(TelemetryBridge::REPLAY_SEQ_FILE_ENV, replay_file.c_str());
    TelemetryBridge bridge{"RadioLink", 20, 19113, 19114};
    InputPort<CommandPacket, 64> dest{};
    bridge.command_out.connect(&dest);

    auto frame = buildUplinkFrame(88, CommandPacket{200, 1, 0.0f});
    EXPECT_TRUE(bridge.ingestUplinkFrameForTest(frame.data(), frame.size()));
    EXPECT_GT(bridge.getErrorCount(), 0u);
}

TEST(TelemetryBridge, MalformedFrameCorpusRejected) {
    ScopedReplaySeqFile replay_path_env{};
    TelemetryBridge bridge{"RadioLink", 20, 19115, 19116};

    auto base = buildUplinkFrame(500, CommandPacket{200, 1, 0.0f});
    uint32_t rejected_before = bridge.getRejectedCount();

    for (size_t i = 0; i < 128u; ++i) {
        auto frame = base;
        const size_t mode = i % 4u;
        if (mode == 0u) {
            // Bad sync
            CcsdsHeader hdr{};
            std::memcpy(&hdr, frame.data(), sizeof(hdr));
            hdr.sync_word ^= 0xFFFFFFFFu;
            std::memcpy(frame.data(), &hdr, sizeof(hdr));
            EXPECT_FALSE(bridge.ingestUplinkFrameForTest(frame.data(), frame.size()));
        } else if (mode == 1u) {
            // Bad APID
            CcsdsHeader hdr{};
            std::memcpy(&hdr, frame.data(), sizeof(hdr));
            hdr.apid = Apid::EVENT;
            std::memcpy(frame.data(), &hdr, sizeof(hdr));
            EXPECT_FALSE(bridge.ingestUplinkFrameForTest(frame.data(), frame.size()));
        } else if (mode == 2u) {
            // Bad payload length
            CcsdsHeader hdr{};
            std::memcpy(&hdr, frame.data(), sizeof(hdr));
            hdr.payload_len = static_cast<uint16_t>(sizeof(CommandPacket) - 1u);
            std::memcpy(frame.data(), &hdr, sizeof(hdr));
            EXPECT_FALSE(bridge.ingestUplinkFrameForTest(frame.data(), frame.size()));
        } else {
            // Truncated frame
            EXPECT_FALSE(bridge.ingestUplinkFrameForTest(frame.data(), frame.size() - 1u));
        }
    }

    EXPECT_GE(bridge.getRejectedCount(), rejected_before + 128u);
}

TEST(TelemetryBridge, RandomizedMalformedFramesProperty) {
    ScopedReplaySeqFile replay_path_env{};
    TelemetryBridge bridge{"RadioLink", 20, 19117, 19118};
    InputPort<CommandPacket, 4096> dest{};
    bridge.command_out.connect(&dest);

    std::mt19937 rng(0xD37AF00Du);
    std::uniform_int_distribution<int> len_dist(
        0, static_cast<int>(TelemetryBridge::UPLINK_FRAME_SIZE + 8u));
    std::uniform_int_distribution<int> byte_dist(0, 255);

    size_t accepted = 0u;
    size_t rejected = 0u;
    uint16_t valid_seq = 600u;

    for (size_t i = 0; i < 2000u; ++i) {
        // Every 25th sample is intentionally canonical to prove acceptance path.
        if (i % 25u == 0u) {
            auto frame = buildUplinkFrame(valid_seq++, CommandPacket{200, 1, 0.0f});
            const bool ok = bridge.ingestUplinkFrameForTest(frame.data(), frame.size());
            EXPECT_TRUE(ok);
            if (ok) {
                ++accepted;
            } else {
                ++rejected;
            }
            continue;
        }

        const size_t len = static_cast<size_t>(len_dist(rng));
        std::vector<uint8_t> blob(len, 0u);
        for (size_t j = 0; j < blob.size(); ++j) {
            blob[j] = static_cast<uint8_t>(byte_dist(rng));
        }

        const bool ok = bridge.ingestUplinkFrameForTest(blob.data(), blob.size());
        if (ok) {
            ++accepted;
        } else {
            ++rejected;
        }
    }

    EXPECT_GT(rejected, 1000u);
    EXPECT_GE(accepted, 80u); // includes intentionally valid injections

    size_t consumed = 0u;
    CommandPacket cmd{};
    while (dest.tryConsume(cmd)) {
        ++consumed;
    }
    EXPECT_EQ(consumed, accepted);
}

TEST(TelemetryBridge, ReplayMonotonicityProperty) {
    ScopedReplaySeqFile replay_path_env{};
    TelemetryBridge bridge{"RadioLink", 20, 19119, 19120};
    InputPort<CommandPacket, 4096> dest{};
    bridge.command_out.connect(&dest);

    uint16_t seq = 1000u;
    size_t accepted = 0u;
    size_t rejected = 0u;

    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    for (size_t i = 0; i < 1024u; ++i) {
        const bool replay = (i > 0u) && (i % 19u == 0u);
        const uint16_t frame_seq = replay ? seq : static_cast<uint16_t>(seq + 1u);
        if (!replay) {
            seq = frame_seq;
        }

        auto frame = buildUplinkFrame(frame_seq, CommandPacket{200, 1, 0.0f});
        const bool ok = bridge.ingestUplinkFrameForTest(frame.data(), frame.size());
        if (replay) {
            EXPECT_FALSE(ok);
            ++rejected;
        } else {
            EXPECT_TRUE(ok);
            ++accepted;
        }
    }
    (void)testing::internal::GetCapturedStdout();
    (void)testing::internal::GetCapturedStderr();

    size_t consumed = 0u;
    CommandPacket cmd{};
    while (dest.tryConsume(cmd)) {
        ++consumed;
    }
    EXPECT_EQ(consumed, accepted);
    EXPECT_EQ(bridge.getRejectedCount(), rejected);
}

TEST(TelemetryBridge, SerialKissModeSelectedFromEnv) {
    ScopedReplaySeqFile replay_path_env{};
    ScopedEnvVar mode_env(TelemetryBridge::LINK_MODE_ENV, "serial_kiss");
    TelemetryBridge bridge{"RadioLink", 20, 19131, 19132};
    EXPECT_EQ(bridge.getLinkMode(), TelemetryBridge::LinkMode::SERIAL_KISS);
}

TEST(TelemetryBridge, DefaultPortsCanBeOverriddenFromEnv) {
    ScopedReplaySeqFile replay_path_env{};
    ScopedEnvVar downlink_env(TelemetryBridge::DOWNLINK_PORT_ENV, "19143");
    ScopedEnvVar uplink_env(TelemetryBridge::UPLINK_PORT_ENV, "19144");
    TelemetryBridge bridge{"RadioLink", 20};
    EXPECT_EQ(bridge.getDownlinkPort(), 19143u);
    EXPECT_EQ(bridge.getUplinkPort(), 19144u);
}

TEST(TelemetryBridge, ExplicitPortsTakePrecedenceOverEnvOverrides) {
    ScopedReplaySeqFile replay_path_env{};
    ScopedEnvVar downlink_env(TelemetryBridge::DOWNLINK_PORT_ENV, "19145");
    ScopedEnvVar uplink_env(TelemetryBridge::UPLINK_PORT_ENV, "19146");
    TelemetryBridge bridge{"RadioLink", 20, 19147, 19148};
    EXPECT_EQ(bridge.getDownlinkPort(), 19147u);
    EXPECT_EQ(bridge.getUplinkPort(), 19148u);
}

TEST(TelemetryBridge, InvalidAllowIpEnvFallsBackWithoutBlockingLocalIngress) {
    ScopedReplaySeqFile replay_path_env{};
    ScopedEnvVar allow_ip_env(TelemetryBridge::UPLINK_ALLOW_IP_ENV, "not-an-ip");
    TelemetryBridge bridge{"RadioLink", 20, 19129, 19130};
    InputPort<CommandPacket, 8> sink{};
    bridge.command_out.connect(&sink);
    EXPECT_GT(bridge.getErrorCount(), 0u);

    auto frame = buildUplinkFrame(899u, CommandPacket{200u, 1u, 0.0f});
    EXPECT_TRUE(bridge.ingestUplinkFrameForTest(frame.data(), frame.size()));
    CommandPacket cmd{};
    ASSERT_TRUE(sink.tryConsume(cmd));
    EXPECT_EQ(cmd.target_id, 200u);
    EXPECT_EQ(cmd.opcode, 1u);
}

TEST(TelemetryBridge, KISSFrameDispatchesCommand) {
    ScopedReplaySeqFile replay_path_env{};
    TelemetryBridge bridge{"RadioLink", 20, 19133, 19134};
    InputPort<CommandPacket, 16> sink{};
    bridge.command_out.connect(&sink);

    const auto ccsds = buildUplinkFrame(901u, CommandPacket{200u, 1u, 0.0f});
    std::array<uint8_t, 1 + sizeof(ccsds)> kiss{};
    kiss[0] = TelemetryBridge::KISS_CMD_DATA;
    std::memcpy(kiss.data() + 1, ccsds.data(), ccsds.size());

    EXPECT_TRUE(bridge.ingestKissFrameForTest(kiss.data(), kiss.size()));
    CommandPacket cmd{};
    ASSERT_TRUE(sink.tryConsume(cmd));
    EXPECT_EQ(cmd.target_id, 200u);
    EXPECT_EQ(cmd.opcode, 1u);
}

TEST(TelemetryBridge, RejectsNonDataKissFrames) {
    ScopedReplaySeqFile replay_path_env{};
    TelemetryBridge bridge{"RadioLink", 20, 19135, 19136};
    const auto ccsds = buildUplinkFrame(902u, CommandPacket{200u, 1u, 0.0f});
    std::array<uint8_t, 1 + sizeof(ccsds)> kiss{};
    kiss[0] = 0x01u;
    std::memcpy(kiss.data() + 1, ccsds.data(), ccsds.size());
    EXPECT_FALSE(bridge.ingestKissFrameForTest(kiss.data(), kiss.size()));
    EXPECT_EQ(bridge.getRejectedCount(), 1u);
}

TEST(TelemetryBridge, RawKissWireFrameDispatchesCommand) {
    ScopedReplaySeqFile replay_path_env{};
    TelemetryBridge bridge{"RadioLink", 20, 19137, 19138};
    InputPort<CommandPacket, 16> sink{};
    bridge.command_out.connect(&sink);

    const auto frame = buildUplinkFrame(903u, CommandPacket{200u, 1u, 0.0f});
    const auto wire = buildRawKissWireFrame(frame);
    EXPECT_EQ(bridge.ingestSerialBytesForTest(wire.data(), wire.size()), 1u);

    CommandPacket cmd{};
    ASSERT_TRUE(sink.tryConsume(cmd));
    EXPECT_EQ(cmd.target_id, 200u);
    EXPECT_EQ(cmd.opcode, 1u);
}

TEST(TelemetryBridge, InvalidRawKissEscapeIsRejected) {
    ScopedReplaySeqFile replay_path_env{};
    TelemetryBridge bridge{"RadioLink", 20, 19139, 19140};

    const std::array<uint8_t, 5> wire{{
        TelemetryBridge::KISS_FEND,
        TelemetryBridge::KISS_CMD_DATA,
        TelemetryBridge::KISS_FESC,
        0x01u,
        TelemetryBridge::KISS_FEND,
    }};
    EXPECT_EQ(bridge.ingestSerialBytesForTest(wire.data(), wire.size()), 0u);
    EXPECT_GT(bridge.getRejectedCount(), 0u);
}

TEST(TelemetryBridge, RawKissNoiseStillAllowsValidFrameProperty) {
    ScopedReplaySeqFile replay_path_env{};
    TelemetryBridge bridge{"RadioLink", 20, 19141, 19142};
    InputPort<CommandPacket, 256> sink{};
    bridge.command_out.connect(&sink);

    std::mt19937 rng(0xC011D00Du);
    std::uniform_int_distribution<int> noise_len_dist(1, 8);
    std::uniform_int_distribution<int> byte_dist(0, 255);

    for (uint16_t seq = 1000u; seq < 1064u; ++seq) {
        const int noise_len = noise_len_dist(rng);
        std::vector<uint8_t> noise(static_cast<size_t>(noise_len), 0u);
        for (uint8_t& byte : noise) {
            byte = static_cast<uint8_t>(byte_dist(rng));
        }

        const auto frame = buildUplinkFrame(seq, CommandPacket{200u, 1u, 0.0f});
        auto wire = buildRawKissWireFrame(frame);
        noise.insert(noise.end(), wire.begin(), wire.end());
        (void)bridge.ingestSerialBytesForTest(noise.data(), noise.size());
    }

    size_t consumed = 0u;
    CommandPacket cmd{};
    while (sink.tryConsume(cmd)) {
        ++consumed;
    }
    EXPECT_EQ(consumed, 64u);
}

TEST(HalMocks, SpiTransferEchoesTxBytes) {
    hal::MockSpi spi{};
    std::array<uint8_t, 4> tx{{1u, 2u, 3u, 4u}};
    std::array<uint8_t, 4> rx{};
    EXPECT_TRUE(spi.transfer(tx.data(), rx.data(), tx.size()));
    EXPECT_EQ(rx, tx);
}

TEST(HalMocks, UartInjectAndRead) {
    hal::MockUart uart{};
    EXPECT_TRUE(uart.configure(115200u));
    std::array<uint8_t, 5> input{{10u, 20u, 30u, 40u, 50u}};
    EXPECT_EQ(uart.injectRx(input.data(), input.size()), input.size());
    std::array<uint8_t, 8> out{};
    const size_t n = uart.read(out.data(), out.size());
    EXPECT_EQ(n, input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        EXPECT_EQ(out[i], input[i]);
    }
}

TEST(HalMocks, PwmDutyCycleClamped) {
    hal::MockPwm pwm{};
    EXPECT_TRUE(pwm.setFrequency(0u, 100u));
    EXPECT_TRUE(pwm.setDutyCycle(0u, 1.5f));
    EXPECT_FLOAT_EQ(pwm.getDutyCycle(0u), 1.0f);
    EXPECT_TRUE(pwm.setDutyCycle(0u, -0.5f));
    EXPECT_FLOAT_EQ(pwm.getDutyCycle(0u), 0.0f);
}

TEST(CommandSequencerComponent, StagedCommandDispatchesImmediately) {
    TimeService::initEpoch();
    CommandSequencerComponent sequencer{"CmdSequencer", 40};
    InputPort<CommandPacket, 8> sink{};
    sequencer.command_out.connect(&sink);
    sequencer.init();

    ASSERT_TRUE(sequencer.cmd_in.receive(
        CommandPacket{40u, CommandSequencerComponent::OP_SET_TARGET, 200.0f}));
    ASSERT_TRUE(sequencer.cmd_in.receive(
        CommandPacket{40u, CommandSequencerComponent::OP_SET_OPCODE, 1.0f}));
    ASSERT_TRUE(sequencer.cmd_in.receive(
        CommandPacket{40u, CommandSequencerComponent::OP_SET_ARGUMENT, 0.0f}));
    ASSERT_TRUE(sequencer.cmd_in.receive(
        CommandPacket{40u, CommandSequencerComponent::OP_COMMIT_DELAY_SECONDS, 0.0f}));

    sequencer.step();

    CommandPacket out{};
    ASSERT_TRUE(sink.tryConsume(out));
    EXPECT_EQ(out.target_id, 200u);
    EXPECT_EQ(out.opcode, 1u);
}

TEST(CommandSequencerComponent, NonFiniteTargetAndOpcodeClampToZero) {
    TimeService::initEpoch();
    CommandSequencerComponent sequencer{"CmdSequencer", 40};
    InputPort<CommandPacket, 8> sink{};
    sequencer.command_out.connect(&sink);
    sequencer.init();

    ASSERT_TRUE(sequencer.cmd_in.receive(CommandPacket{
        40u,
        CommandSequencerComponent::OP_SET_TARGET,
        std::numeric_limits<float>::infinity(),
    }));
    ASSERT_TRUE(sequencer.cmd_in.receive(CommandPacket{
        40u,
        CommandSequencerComponent::OP_SET_OPCODE,
        std::numeric_limits<float>::quiet_NaN(),
    }));
    ASSERT_TRUE(sequencer.cmd_in.receive(
        CommandPacket{40u, CommandSequencerComponent::OP_SET_ARGUMENT, 3.5f}));
    ASSERT_TRUE(sequencer.cmd_in.receive(
        CommandPacket{40u, CommandSequencerComponent::OP_COMMIT_DELAY_SECONDS, 0.0f}));
    sequencer.step();

    CommandPacket out{};
    ASSERT_TRUE(sink.tryConsume(out));
    EXPECT_EQ(out.target_id, 0u);
    EXPECT_EQ(out.opcode, 0u);
    EXPECT_FLOAT_EQ(out.argument, 3.5f);
}

TEST(FileTransferComponent, SessionStoresAndReadsChunkData) {
    FileTransferComponent file_mgr{"FileTransfer", 41};
    file_mgr.init();
    const std::array<uint8_t, 4> payload{{1u, 2u, 3u, 4u}};
    ASSERT_TRUE(file_mgr.beginSession(payload.size()));
    ASSERT_TRUE(file_mgr.ingestChunk(payload.data(), payload.size()));
    ASSERT_TRUE(file_mgr.finalizeSession());
    EXPECT_EQ(file_mgr.receivedBytes(), payload.size());

    std::array<uint8_t, 8> out{};
    const size_t n = file_mgr.readNextChunk(out.data(), out.size());
    EXPECT_EQ(n, payload.size());
    for (size_t i = 0; i < payload.size(); ++i) {
        EXPECT_EQ(out[i], payload[i]);
    }
}

TEST(FileTransferComponent, FinalizeFailsWhenExpectedBytesNotReceived) {
    FileTransferComponent file_mgr{"FileTransfer", 41};
    file_mgr.init();

    const std::array<uint8_t, 2> chunk{{8u, 9u}};
    ASSERT_TRUE(file_mgr.beginSession(4u));
    ASSERT_TRUE(file_mgr.ingestChunk(chunk.data(), chunk.size()));
    EXPECT_FALSE(file_mgr.finalizeSession());
    EXPECT_EQ(file_mgr.receivedBytes(), chunk.size());
}

TEST(FileTransferComponent, RejectsChunkThatExceedsExpectedSessionBytes) {
    FileTransferComponent file_mgr{"FileTransfer", 41};
    file_mgr.init();

    const std::array<uint8_t, 2> chunk{{1u, 2u}};
    const std::array<uint8_t, 1> extra{{3u}};
    ASSERT_TRUE(file_mgr.beginSession(2u));
    ASSERT_TRUE(file_mgr.ingestChunk(chunk.data(), chunk.size()));
    EXPECT_FALSE(file_mgr.ingestChunk(extra.data(), extra.size()));
    EXPECT_EQ(file_mgr.receivedBytes(), chunk.size());
}

TEST(MemoryDwellComponent, SampleNowEmitsTelemetry) {
    TimeService::initEpoch();
    MemoryDwellComponent dwell{"MemoryDwell", 42};
    InputPort<Serializer::ByteArray, 8> sink{};
    dwell.telemetry_out.connect(&sink);
    dwell.init();

    ASSERT_TRUE(dwell.cmd_in.receive(
        CommandPacket{42u, MemoryDwellComponent::OP_SELECT_CHANNEL, 1.0f}));
    ASSERT_TRUE(dwell.cmd_in.receive(
        CommandPacket{42u, MemoryDwellComponent::OP_SAMPLE_NOW, 0.0f}));
    dwell.step();

    Serializer::ByteArray sample{};
    ASSERT_TRUE(sink.tryConsume(sample));
    const TelemetryPacket telem = Serializer::unpackTelem(sample);
    EXPECT_EQ(telem.component_id, 42u);
}

TEST(MemoryDwellComponent, PatchAndDwellAddressWord) {
    TimeService::initEpoch();
    MemoryDwellComponent dwell{"MemoryDwell", 42};
    dwell.init();

    const uint32_t addr = MemoryDwellComponent::DEBUG_MEM_BASE_ADDR;
    const uint16_t addr_hi = static_cast<uint16_t>(addr >> 16U);
    const uint16_t addr_lo = static_cast<uint16_t>(addr & 0xFFFFU);

    ASSERT_TRUE(dwell.cmd_in.receive(
        CommandPacket{42u, MemoryDwellComponent::OP_SET_ADDR_HI16, static_cast<float>(addr_hi)}));
    ASSERT_TRUE(dwell.cmd_in.receive(
        CommandPacket{42u, MemoryDwellComponent::OP_SET_ADDR_LO16, static_cast<float>(addr_lo)}));
    ASSERT_TRUE(dwell.cmd_in.receive(
        CommandPacket{42u, MemoryDwellComponent::OP_SET_VALUE_HI16, 0.0f}));
    ASSERT_TRUE(dwell.cmd_in.receive(
        CommandPacket{42u, MemoryDwellComponent::OP_SET_VALUE_LO16, 42.0f}));
    ASSERT_TRUE(dwell.cmd_in.receive(
        CommandPacket{42u, MemoryDwellComponent::OP_PATCH_ADDRESS, 0.0f}));
    dwell.step();

    EXPECT_EQ(dwell.readDebugWordForTest(addr), 42u);
}

TEST(TimeService, UtcSyncHelpers) {
    TimeService::initEpoch();
    const uint64_t target_utc_ms = 5000000ULL;
    TimeService::setUtcFromUnixMs(target_utc_ms);
    EXPECT_TRUE(TimeService::hasUtcSync());
    const uint64_t got = TimeService::getUtcUnixMs();
    const int64_t diff = static_cast<int64_t>(got) - static_cast<int64_t>(target_utc_ms);
    EXPECT_LT(std::llabs(diff), 2000LL);
}

TEST(TimeService, UtcNeverMovesBackwardAfterResync) {
    TimeService::initEpoch();
    TimeService::setUtcFromUnixMs(5'000'000ULL);
    const uint64_t first = TimeService::getUtcUnixMs();

    // Simulate a bad/stale sync packet that would move UTC backwards.
    TimeService::setUtcFromUnixMs(4'000'000ULL);
    const uint64_t second = TimeService::getUtcUnixMs();

    EXPECT_GE(second, first);
}

TEST(TimeSyncComponent, ApplySyncFromWordCommands) {
    TimeService::initEpoch();
    TimeSyncComponent sync{"TimeSync", 43};
    sync.init();

    constexpr uint64_t utc_ms = 0x0000000000123456ULL;
    const uint16_t w0 = static_cast<uint16_t>((utc_ms >> 48U) & 0xFFFFU);
    const uint16_t w1 = static_cast<uint16_t>((utc_ms >> 32U) & 0xFFFFU);
    const uint16_t w2 = static_cast<uint16_t>((utc_ms >> 16U) & 0xFFFFU);
    const uint16_t w3 = static_cast<uint16_t>(utc_ms & 0xFFFFU);

    ASSERT_TRUE(sync.cmd_in.receive(CommandPacket{43u, TimeSyncComponent::OP_SET_UTC_HI16, static_cast<float>(w0)}));
    ASSERT_TRUE(sync.cmd_in.receive(CommandPacket{43u, TimeSyncComponent::OP_SET_UTC_MIDHI16, static_cast<float>(w1)}));
    ASSERT_TRUE(sync.cmd_in.receive(CommandPacket{43u, TimeSyncComponent::OP_SET_UTC_MIDLO16, static_cast<float>(w2)}));
    ASSERT_TRUE(sync.cmd_in.receive(CommandPacket{43u, TimeSyncComponent::OP_SET_UTC_LO16, static_cast<float>(w3)}));
    ASSERT_TRUE(sync.cmd_in.receive(CommandPacket{43u, TimeSyncComponent::OP_APPLY_SYNC, 0.0f}));
    sync.step();

    EXPECT_TRUE(TimeService::hasUtcSync());
    const int64_t diff = static_cast<int64_t>(TimeService::getUtcUnixMs()) - static_cast<int64_t>(utc_ms);
    EXPECT_LT(std::llabs(diff), 2000LL);
}

TEST(TimeSyncComponent, OutputBackpressureIncrementsErrorCount) {
    TimeService::initEpoch();
    TimeSyncComponent sync{"TimeSync", 43};
    InputPort<Serializer::ByteArray, 1> telem_sink{};
    InputPort<EventPacket, 1> event_sink{};
    OutputPort<Serializer::ByteArray> telem_prefill{};

    sync.telemetry_out.connect(&telem_sink);
    sync.event_out.connect(&event_sink);
    telem_prefill.connect(&telem_sink);
    sync.init();

    ASSERT_TRUE(telem_prefill.send(Serializer::pack(TelemetryPacket{1u, 99u, 1.0f})));
    ASSERT_TRUE(sync.cmd_in.receive(CommandPacket{43u, TimeSyncComponent::OP_PUBLISH_STATUS, 0.0f}));

    const uint32_t before = sync.getErrorCount();
    sync.step();

    EXPECT_GE(sync.getErrorCount(), before + 2u);
}

TEST(TimeSyncComponent, PublishStatusReflectsSyncStateAndBadOpcode) {
    TimeService::initEpoch();
    TimeSyncComponent sync{"TimeSync", 43};
    InputPort<EventPacket, 16> events{};
    sync.event_out.connect(&events);
    sync.init();

    ASSERT_TRUE(sync.cmd_in.receive(CommandPacket{43u, TimeSyncComponent::OP_PUBLISH_STATUS, 0.0f}));
    const uint32_t before = sync.getErrorCount();
    ASSERT_TRUE(sync.cmd_in.receive(CommandPacket{43u, 999u, 0.0f}));
    sync.step();

    bool saw_unsynced = false;
    bool saw_bad_opcode = false;
    EventPacket evt{};
    while (events.tryConsume(evt)) {
        const std::string_view msg(evt.message.data());
        if (msg == "TIME_UNSYNCED") {
            saw_unsynced = true;
        } else if (msg == "TIME_BAD_OPCODE") {
            saw_bad_opcode = true;
        }
    }
    EXPECT_TRUE(saw_unsynced);
    EXPECT_TRUE(saw_bad_opcode);
    EXPECT_GE(sync.getErrorCount(), before + 1u);

    constexpr uint64_t utc_ms = 0x0000000000ABCDEFULL;
    ASSERT_TRUE(sync.cmd_in.receive(CommandPacket{
        43u, TimeSyncComponent::OP_SET_UTC_MIDLO16,
        static_cast<float>((utc_ms >> 16U) & 0xFFFFU)}));
    ASSERT_TRUE(sync.cmd_in.receive(CommandPacket{
        43u, TimeSyncComponent::OP_SET_UTC_LO16,
        static_cast<float>(utc_ms & 0xFFFFU)}));
    ASSERT_TRUE(sync.cmd_in.receive(CommandPacket{43u, TimeSyncComponent::OP_APPLY_SYNC, 0.0f}));
    ASSERT_TRUE(sync.cmd_in.receive(CommandPacket{43u, TimeSyncComponent::OP_PUBLISH_STATUS, 0.0f}));
    sync.step();

    bool saw_synced = false;
    while (events.tryConsume(evt)) {
        if (std::string_view(evt.message.data()) == "TIME_SYNCED") {
            saw_synced = true;
        }
    }
    EXPECT_TRUE(saw_synced);
}

TEST(PlaybackComponent, LoadsAndReplaysHistoricalSamples) {
    const char* log_path = "flight_log.csv";
    std::string backup{};
    bool had_backup = false;
    {
        std::ifstream in(log_path, std::ios::binary);
        if (in.is_open()) {
            backup.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            had_backup = true;
        }
    }

    {
        std::ofstream out(log_path, std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << "timestamp_ms,component_id,data_payload\n";
        out << "101,200,99.5\n";
        out << "102,100,12.25\n";
    }

    TimeService::initEpoch();
    PlaybackComponent playback{"Playback", 44};
    InputPort<Serializer::ByteArray, 16> sink{};
    playback.telemetry_out.connect(&sink);
    playback.init();
    ASSERT_TRUE(playback.cmd_in.receive(CommandPacket{44u, PlaybackComponent::OP_LOAD_LOG, 0.0f}));
    ASSERT_TRUE(playback.cmd_in.receive(CommandPacket{44u, PlaybackComponent::OP_STEP_ONCE, 0.0f}));
    playback.step();

    Serializer::ByteArray sample{};
    ASSERT_TRUE(sink.tryConsume(sample));
    const TelemetryPacket telem = Serializer::unpackTelem(sample);
    EXPECT_EQ(telem.timestamp_ms, 101u);
    EXPECT_EQ(telem.component_id, 200u);

    if (had_backup) {
        std::ofstream restore(log_path, std::ios::binary | std::ios::trunc);
        restore << backup;
    } else {
        (void)std::remove(log_path);
    }
}

TEST(PlaybackComponent, MissingLogAndSendFailureAreVisible) {
    const char* log_path = "flight_log.csv";
    std::string backup{};
    bool had_backup = false;
    {
        std::ifstream in(log_path, std::ios::binary);
        if (in.is_open()) {
            backup.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            had_backup = true;
        }
    }
    (void)std::remove(log_path);

    TimeService::initEpoch();
    PlaybackComponent playback{"Playback", 44};
    InputPort<EventPacket, 16> events{};
    playback.event_out.connect(&events);
    playback.init();

    const uint32_t before_missing = playback.getErrorCount();
    ASSERT_TRUE(playback.cmd_in.receive(CommandPacket{44u, PlaybackComponent::OP_LOAD_LOG, 0.0f}));
    playback.step();

    bool saw_load_fail = false;
    EventPacket evt{};
    while (events.tryConsume(evt)) {
        if (std::string_view(evt.message.data()) == "PLAYBACK_LOAD_FAIL") {
            saw_load_fail = true;
        }
    }
    EXPECT_TRUE(saw_load_fail);
    EXPECT_GE(playback.getErrorCount(), before_missing + 1u);

    {
        std::ofstream out(log_path, std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << "timestamp_ms,component_id,data_payload\n";
        out << "101,200,99.5\n";
    }

    InputPort<Serializer::ByteArray, 1> sink{};
    OutputPort<Serializer::ByteArray> prefill{};
    playback.telemetry_out.connect(&sink);
    prefill.connect(&sink);
    ASSERT_TRUE(prefill.send(Serializer::pack(TelemetryPacket{1u, 99u, 1.0f})));

    const uint32_t before_send_fail = playback.getErrorCount();
    ASSERT_TRUE(playback.cmd_in.receive(CommandPacket{44u, PlaybackComponent::OP_LOAD_LOG, 0.0f}));
    ASSERT_TRUE(playback.cmd_in.receive(CommandPacket{44u, PlaybackComponent::OP_STEP_ONCE, 0.0f}));
    playback.step();

    bool saw_send_fail = false;
    while (events.tryConsume(evt)) {
        if (std::string_view(evt.message.data()) == "PLAYBACK_SEND_FAIL") {
            saw_send_fail = true;
        }
    }
    EXPECT_TRUE(saw_send_fail);
    EXPECT_GE(playback.getErrorCount(), before_send_fail + 1u);

    if (had_backup) {
        std::ofstream restore(log_path, std::ios::binary | std::ios::trunc);
        restore << backup;
    } else {
        (void)std::remove(log_path);
    }
}

TEST(OtaComponent, VerifiesImageAndRequestsReboot) {
    OtaComponent ota{"OtaManager", 45};
    ota.init();

    constexpr std::array<uint8_t, 4> image{{1u, 2u, 3u, 4u}};
    auto crc32 = [](const uint8_t* data, size_t len) -> uint32_t {
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; ++i) {
            crc ^= data[i];
            for (int b = 0; b < 8; ++b) {
                const uint32_t mask = static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1U)));
                crc = (crc >> 1U) ^ (0xEDB88320U & mask);
            }
        }
        return ~crc;
    };

    const uint32_t expected_crc = crc32(image.data(), image.size());
    const uint16_t crc_hi = static_cast<uint16_t>(expected_crc >> 16U);
    const uint16_t crc_lo = static_cast<uint16_t>(expected_crc & 0xFFFFU);

    ASSERT_TRUE(ota.cmd_in.receive(CommandPacket{45u, OtaComponent::OP_BEGIN_SESSION, 4.0f}));
    ASSERT_TRUE(ota.cmd_in.receive(CommandPacket{45u, OtaComponent::OP_SET_EXPECTED_CRC_HI16, static_cast<float>(crc_hi)}));
    ASSERT_TRUE(ota.cmd_in.receive(CommandPacket{45u, OtaComponent::OP_SET_EXPECTED_CRC_LO16, static_cast<float>(crc_lo)}));
    for (uint8_t b : image) {
        ASSERT_TRUE(ota.cmd_in.receive(CommandPacket{45u, OtaComponent::OP_PUSH_TEST_BYTE, static_cast<float>(b)}));
    }
    ASSERT_TRUE(ota.cmd_in.receive(CommandPacket{45u, OtaComponent::OP_VERIFY_IMAGE, 0.0f}));
    ASSERT_TRUE(ota.cmd_in.receive(CommandPacket{45u, OtaComponent::OP_STAGE_ACTIVATE, 0.0f}));
    ota.step();

    EXPECT_TRUE(ota.isRebootRequested());
}

TEST(AtsRtsSequencerComponent, RtsEntryTriggersAfterMatchingEvent) {
    TimeService::initEpoch();
    AtsRtsSequencerComponent seq{"AtsRtsSequencer", 46};
    InputPort<CommandPacket, 16> cmd_sink{};
    seq.command_out.connect(&cmd_sink);
    seq.init();

    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_SET_TARGET, 200.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_SET_OPCODE, 1.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_SET_ARGUMENT, 0.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_SET_TRIGGER_TYPE, 2.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_SET_EVENT_SOURCE, 12.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_SET_TIME_HI16, 0.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_SET_TIME_LO16, 0.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_COMMIT_ENTRY, 0.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_ARM, 0.0f}));

    seq.step();
    CommandPacket out{};
    EXPECT_FALSE(cmd_sink.tryConsume(out));

    ASSERT_TRUE(seq.event_in.receive(EventPacket::create(Severity::INFO, 12u, "TRIGGER")));
    seq.step();
    ASSERT_TRUE(cmd_sink.tryConsume(out));
    EXPECT_EQ(out.target_id, 200u);
    EXPECT_EQ(out.opcode, 1u);
}

TEST(LimitCheckerComponent, DynamicThresholdsEmitCriticalAlarm) {
    TimeService::initEpoch();
    LimitCheckerComponent limits{"LimitChecker", 47};
    InputPort<EventPacket, 16> events{};
    limits.event_out.connect(&events);
    limits.init();

    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_TARGET_ID, 200.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_WARN_LOW, 20.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_WARN_HIGH, 80.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_CRIT_LOW, 10.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_CRIT_HIGH, 90.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_APPLY_LIMITS, 0.0f}));

    const TelemetryPacket hot_sample{TimeService::getMET(), 200u, 95.0f};
    ASSERT_TRUE(limits.telem_in.receive(Serializer::pack(hot_sample)));
    limits.step();

    bool saw_critical = false;
    EventPacket evt{};
    while (events.tryConsume(evt)) {
        if (std::string_view(evt.message.data()) == "LIM_CRIT") {
            saw_critical = true;
            break;
        }
    }
    EXPECT_TRUE(saw_critical);
}

TEST(CfdpComponent, TracksMissingChunksAcrossSession) {
    CfdpComponent cfdp{"CfdpManager", 48};
    cfdp.init();

    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_BEGIN_SESSION, 3.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_SET_CHUNK_INDEX, 0.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_PUSH_TEST_BYTE, 10.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_COMMIT_CHUNK, 0.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_SET_CHUNK_INDEX, 2.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_PUSH_TEST_BYTE, 22.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_COMMIT_CHUNK, 0.0f}));
    cfdp.step();

    EXPECT_EQ(cfdp.missingCount(), 1u);
}

TEST(ModeManagerComponent, AppliesModeRuleDispatch) {
    TimeService::initEpoch();
    ModeManagerComponent mode_mgr{"ModeManager", 49};
    InputPort<CommandPacket, 16> cmd_sink{};
    mode_mgr.command_out.connect(&cmd_sink);
    mode_mgr.init();

    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_STAGE_TARGET_ID, 200.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_STAGE_ENABLE_OPCODE, 2.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_STAGE_ENABLE_ARG, 0.25f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_STAGE_DISABLE_OPCODE, 2.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_STAGE_DISABLE_ARG, 0.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{
        49u, ModeManagerComponent::OP_STAGE_MODE_MASK,
        static_cast<float>(ModeManagerComponent::MODE_MASK_SCIENCE)}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_COMMIT_RULE, 0.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_SET_MODE, 3.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_APPLY_MODE, 0.0f}));

    mode_mgr.step();

    CommandPacket out{};
    ASSERT_TRUE(cmd_sink.tryConsume(out));
    EXPECT_EQ(out.target_id, 200u);
    EXPECT_EQ(out.opcode, 2u);
    EXPECT_FLOAT_EQ(out.argument, 0.25f);
}

TEST(OtaComponent, StageWritesArtifactAndManifest) {
    const std::string artifact = makeUniqueTestLogPath("test_ota_stage", ".bin");
    const std::string manifest = makeUniqueTestLogPath("test_ota_stage", ".meta");
    (void)std::remove(artifact.c_str());
    (void)std::remove(manifest.c_str());
    ScopedEnvVar artifact_env(OtaComponent::OTA_ARTIFACT_PATH_ENV, artifact.c_str());
    ScopedEnvVar manifest_env(OtaComponent::OTA_MANIFEST_PATH_ENV, manifest.c_str());

    OtaComponent ota{"OtaManager", 45};
    ota.init();

    constexpr std::array<uint8_t, 4> image{{9u, 8u, 7u, 6u}};
    auto crc32 = [](const uint8_t* data, size_t len) -> uint32_t {
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; ++i) {
            crc ^= data[i];
            for (int b = 0; b < 8; ++b) {
                const uint32_t mask = static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1U)));
                crc = (crc >> 1U) ^ (0xEDB88320U & mask);
            }
        }
        return ~crc;
    };
    const uint32_t expected_crc = crc32(image.data(), image.size());

    ASSERT_TRUE(ota.cmd_in.receive(CommandPacket{45u, OtaComponent::OP_BEGIN_SESSION, 4.0f}));
    ASSERT_TRUE(ota.cmd_in.receive(CommandPacket{
        45u, OtaComponent::OP_SET_EXPECTED_CRC_HI16, static_cast<float>(expected_crc >> 16U)}));
    ASSERT_TRUE(ota.cmd_in.receive(CommandPacket{
        45u, OtaComponent::OP_SET_EXPECTED_CRC_LO16, static_cast<float>(expected_crc & 0xFFFFU)}));
    for (uint8_t b : image) {
        ASSERT_TRUE(ota.cmd_in.receive(CommandPacket{
            45u, OtaComponent::OP_PUSH_TEST_BYTE, static_cast<float>(b)}));
    }
    ASSERT_TRUE(ota.cmd_in.receive(CommandPacket{45u, OtaComponent::OP_VERIFY_IMAGE, 0.0f}));
    ASSERT_TRUE(ota.cmd_in.receive(CommandPacket{45u, OtaComponent::OP_STAGE_ACTIVATE, 0.0f}));
    ota.step();

    EXPECT_TRUE(ota.isRebootRequested());

    std::ifstream image_file(artifact, std::ios::binary);
    ASSERT_TRUE(image_file.is_open());
    std::array<uint8_t, 4> staged{};
    image_file.read(reinterpret_cast<char*>(staged.data()),
        static_cast<std::streamsize>(staged.size()));
    EXPECT_EQ(image_file.gcount(), static_cast<std::streamsize>(image.size()));
    EXPECT_EQ(staged, image);

    std::ifstream manifest_file(manifest);
    ASSERT_TRUE(manifest_file.is_open());
    std::string manifest_text{
        std::istreambuf_iterator<char>(manifest_file),
        std::istreambuf_iterator<char>()};
    EXPECT_NE(manifest_text.find("bytes=4"), std::string::npos);
    EXPECT_NE(manifest_text.find("crc32=0x"), std::string::npos);

    (void)std::remove(artifact.c_str());
    (void)std::remove(manifest.c_str());
}

TEST(OtaComponent, SizeMismatchPreventsActivation) {
    OtaComponent ota{"OtaManager", 45};
    ota.init();

    constexpr std::array<uint8_t, 4> image{{1u, 2u, 3u, 4u}};
    ASSERT_TRUE(ota.cmd_in.receive(CommandPacket{45u, OtaComponent::OP_BEGIN_SESSION, 5.0f}));
    ASSERT_TRUE(ota.cmd_in.receive(CommandPacket{45u, OtaComponent::OP_SET_EXPECTED_CRC_HI16, 0.0f}));
    ASSERT_TRUE(ota.cmd_in.receive(CommandPacket{45u, OtaComponent::OP_SET_EXPECTED_CRC_LO16, 0.0f}));
    for (uint8_t b : image) {
        ASSERT_TRUE(ota.cmd_in.receive(CommandPacket{
            45u, OtaComponent::OP_PUSH_TEST_BYTE, static_cast<float>(b)}));
    }
    ASSERT_TRUE(ota.cmd_in.receive(CommandPacket{45u, OtaComponent::OP_VERIFY_IMAGE, 0.0f}));
    ASSERT_TRUE(ota.cmd_in.receive(CommandPacket{45u, OtaComponent::OP_STAGE_ACTIVATE, 0.0f}));
    ota.step();

    EXPECT_FALSE(ota.isRebootRequested());
}

TEST(AtsRtsSequencerComponent, DisarmPreventsImmediateDispatch) {
    TimeService::initEpoch();
    AtsRtsSequencerComponent seq{"AtsRtsSequencer", 46};
    InputPort<CommandPacket, 16> cmd_sink{};
    seq.command_out.connect(&cmd_sink);
    seq.init();

    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_SET_TARGET, 200.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_SET_OPCODE, 1.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_SET_TRIGGER_TYPE, 0.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_SET_TIME_HI16, 0.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_SET_TIME_LO16, 0.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_COMMIT_ENTRY, 0.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_DISARM, 0.0f}));
    seq.step();

    CommandPacket out{};
    EXPECT_FALSE(cmd_sink.tryConsume(out));
}

TEST(AtsRtsSequencerComponent, FullTableAndSendFailureSurfaceFaults) {
    TimeService::initEpoch();
    AtsRtsSequencerComponent seq{"AtsRtsSequencer", 46};
    InputPort<CommandPacket, 1> cmd_sink{};
    InputPort<EventPacket, 64> events{};
    OutputPort<CommandPacket> prefill{};
    seq.command_out.connect(&cmd_sink);
    seq.event_out.connect(&events);
    prefill.connect(&cmd_sink);
    seq.init();

    for (size_t i = 0; i < AtsRtsSequencerComponent::MAX_SEQUENCE_ENTRIES; ++i) {
        ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{
            46u, AtsRtsSequencerComponent::OP_SET_TARGET, static_cast<float>(200u + static_cast<uint32_t>(i))}));
        ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_COMMIT_ENTRY, 0.0f}));
        seq.step();
    }

    const uint32_t before = seq.getErrorCount();
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_COMMIT_ENTRY, 0.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_CLEAR, 0.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_SET_TARGET, 200.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_SET_OPCODE, 1.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_SET_TRIGGER_TYPE, 0.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_SET_TIME_HI16, 0.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_SET_TIME_LO16, 0.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_COMMIT_ENTRY, 0.0f}));
    ASSERT_TRUE(seq.cmd_in.receive(CommandPacket{46u, AtsRtsSequencerComponent::OP_ARM, 0.0f}));
    ASSERT_TRUE(prefill.send(CommandPacket{999u, 1u, 0.0f}));
    seq.step();

    bool saw_full = false;
    bool saw_send_fail = false;
    EventPacket evt{};
    while (events.tryConsume(evt)) {
        const std::string_view msg(evt.message.data());
        if (msg == "ATSRTS_FULL") {
            saw_full = true;
        } else if (msg == "ATSRTS_SEND_FAIL") {
            saw_send_fail = true;
        }
    }
    EXPECT_TRUE(saw_full);
    EXPECT_TRUE(saw_send_fail);
    EXPECT_GE(seq.getErrorCount(), before + 2u);
}

TEST(LimitCheckerComponent, DisableSuppressesAlarmEmission) {
    TimeService::initEpoch();
    LimitCheckerComponent limits{"LimitChecker", 47};
    InputPort<EventPacket, 32> events{};
    limits.event_out.connect(&events);
    limits.init();

    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_TARGET_ID, 200.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_WARN_LOW, 20.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_WARN_HIGH, 80.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_CRIT_LOW, 10.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_CRIT_HIGH, 90.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_APPLY_LIMITS, 0.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_DISABLE, 0.0f}));
    limits.step();

    EventPacket evt{};
    while (events.tryConsume(evt)) {}

    const TelemetryPacket hot_sample{TimeService::getMET(), 200u, 95.0f};
    ASSERT_TRUE(limits.telem_in.receive(Serializer::pack(hot_sample)));
    limits.step();

    bool saw_alarm = false;
    while (events.tryConsume(evt)) {
        const std::string_view msg(evt.message.data());
        if (msg == "LIM_WARN" || msg == "LIM_CRIT") {
            saw_alarm = true;
        }
    }
    EXPECT_FALSE(saw_alarm);
}

TEST(LimitCheckerComponent, ClearTargetStopsFutureAlarms) {
    TimeService::initEpoch();
    LimitCheckerComponent limits{"LimitChecker", 47};
    InputPort<EventPacket, 32> events{};
    limits.event_out.connect(&events);
    limits.init();

    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_TARGET_ID, 200.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_WARN_LOW, 20.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_WARN_HIGH, 80.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_CRIT_LOW, 10.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_CRIT_HIGH, 90.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_APPLY_LIMITS, 0.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_CLEAR_TARGET, 0.0f}));
    limits.step();

    EventPacket evt{};
    while (events.tryConsume(evt)) {}

    const TelemetryPacket hot_sample{TimeService::getMET(), 200u, 95.0f};
    ASSERT_TRUE(limits.telem_in.receive(Serializer::pack(hot_sample)));
    limits.step();

    bool saw_alarm = false;
    while (events.tryConsume(evt)) {
        const std::string_view msg(evt.message.data());
        if (msg == "LIM_WARN" || msg == "LIM_CRIT") {
            saw_alarm = true;
        }
    }
    EXPECT_FALSE(saw_alarm);
}

TEST(LimitCheckerComponent, InvalidConfigurationsAndWarnTransitionsAreVisible) {
    TimeService::initEpoch();
    LimitCheckerComponent limits{"LimitChecker", 47};
    InputPort<EventPacket, 64> events{};
    limits.event_out.connect(&events);
    limits.init();

    const uint32_t before = limits.getErrorCount();
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_APPLY_LIMITS, 0.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_TARGET_ID, 200.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_WARN_LOW, 80.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_WARN_HIGH, 20.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_CRIT_LOW, 10.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_CRIT_HIGH, 90.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_APPLY_LIMITS, 0.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_WARN_LOW, 20.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_SET_WARN_HIGH, 80.0f}));
    ASSERT_TRUE(limits.cmd_in.receive(CommandPacket{47u, LimitCheckerComponent::OP_APPLY_LIMITS, 0.0f}));
    limits.step();

    const TelemetryPacket warn_sample{TimeService::getMET(), 200u, 85.0f};
    ASSERT_TRUE(limits.telem_in.receive(Serializer::pack(warn_sample)));
    limits.step();
    ASSERT_TRUE(limits.telem_in.receive(Serializer::pack(warn_sample)));
    limits.step();

    size_t warn_count = 0U;
    bool saw_no_target = false;
    bool saw_bad_range = false;
    EventPacket evt{};
    while (events.tryConsume(evt)) {
        const std::string_view msg(evt.message.data());
        if (msg == "LIM_NO_TARGET") {
            saw_no_target = true;
        } else if (msg == "LIM_BAD_RANGE") {
            saw_bad_range = true;
        } else if (msg == "LIM_WARN") {
            ++warn_count;
        }
    }
    EXPECT_TRUE(saw_no_target);
    EXPECT_TRUE(saw_bad_range);
    EXPECT_EQ(warn_count, 1u);
    EXPECT_GE(limits.getErrorCount(), before + 2u);
}

TEST(CfdpComponent, CompleteSessionEmitsMissingAndDoneStates) {
    CfdpComponent cfdp{"CfdpManager", 48};
    InputPort<EventPacket, 32> events{};
    cfdp.event_out.connect(&events);
    cfdp.init();

    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_BEGIN_SESSION, 2.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_SET_CHUNK_INDEX, 0.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_PUSH_TEST_BYTE, 7.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_COMMIT_CHUNK, 0.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_COMPLETE_SESSION, 0.0f}));
    cfdp.step();

    bool saw_missing = false;
    EventPacket evt{};
    while (events.tryConsume(evt)) {
        if (std::string_view(evt.message.data()) == "CFDP_MISSING") {
            saw_missing = true;
        }
    }
    EXPECT_TRUE(saw_missing);

    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_RESET_SESSION, 0.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_BEGIN_SESSION, 1.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_SET_CHUNK_INDEX, 0.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_PUSH_TEST_BYTE, 9.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_COMMIT_CHUNK, 0.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_COMPLETE_SESSION, 0.0f}));
    cfdp.step();

    bool saw_done = false;
    while (events.tryConsume(evt)) {
        if (std::string_view(evt.message.data()) == "CFDP_DONE") {
            saw_done = true;
        }
    }
    EXPECT_TRUE(saw_done);
}

TEST(CfdpComponent, InvalidCommandsAndReportsAreVisible) {
    CfdpComponent cfdp{"CfdpManager", 48};
    InputPort<EventPacket, 32> events{};
    InputPort<Serializer::ByteArray, 16> telem{};
    cfdp.event_out.connect(&events);
    cfdp.telemetry_out.connect(&telem);
    cfdp.init();

    const uint32_t before = cfdp.getErrorCount();
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_BEGIN_SESSION, 0.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_COMMIT_CHUNK, 0.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, 999u, 0.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_BEGIN_SESSION, 3.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_SET_CHUNK_INDEX, 0.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_PUSH_TEST_BYTE, 10.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_COMMIT_CHUNK, 0.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_REPORT_MISSING_COUNT, 0.0f}));
    ASSERT_TRUE(cfdp.cmd_in.receive(CommandPacket{48u, CfdpComponent::OP_REPORT_NEXT_MISSING, 0.0f}));
    cfdp.step();

    bool saw_bad_size = false;
    bool saw_idx_fail = false;
    bool saw_bad_op = false;
    EventPacket evt{};
    while (events.tryConsume(evt)) {
        const std::string_view msg(evt.message.data());
        if (msg == "CFDP_BAD_SIZE") {
            saw_bad_size = true;
        } else if (msg == "CFDP_IDX_FAIL") {
            saw_idx_fail = true;
        } else if (msg == "CFDP_BAD_OP") {
            saw_bad_op = true;
        }
    }
    EXPECT_TRUE(saw_bad_size);
    EXPECT_TRUE(saw_idx_fail);
    EXPECT_TRUE(saw_bad_op);
    EXPECT_GE(cfdp.getErrorCount(), before + 3u);

    bool saw_missing_count = false;
    bool saw_next_missing = false;
    Serializer::ByteArray raw{};
    while (telem.tryConsume(raw)) {
        const TelemetryPacket tlm = Serializer::unpackTelem(raw);
        if (tlm.component_id != 48u) {
            continue;
        }
        if (tlm.data_payload == 2.0f) {
            saw_missing_count = true;
        } else if (tlm.data_payload == 1.0f) {
            saw_next_missing = true;
        }
    }
    EXPECT_TRUE(saw_missing_count);
    EXPECT_TRUE(saw_next_missing);
}

TEST(ModeManagerComponent, AppliesDisablePathOutsideModeMask) {
    TimeService::initEpoch();
    ModeManagerComponent mode_mgr{"ModeManager", 49};
    InputPort<CommandPacket, 16> cmd_sink{};
    mode_mgr.command_out.connect(&cmd_sink);
    mode_mgr.init();

    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_STAGE_TARGET_ID, 200.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_STAGE_ENABLE_OPCODE, 2.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_STAGE_ENABLE_ARG, 4.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_STAGE_DISABLE_OPCODE, 2.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_STAGE_DISABLE_ARG, 0.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{
        49u, ModeManagerComponent::OP_STAGE_MODE_MASK,
        static_cast<float>(ModeManagerComponent::MODE_MASK_SCIENCE)}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_COMMIT_RULE, 0.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_SET_MODE, 2.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_APPLY_MODE, 0.0f}));
    mode_mgr.step();

    CommandPacket out{};
    ASSERT_TRUE(cmd_sink.tryConsume(out));
    EXPECT_EQ(out.target_id, 200u);
    EXPECT_EQ(out.opcode, 2u);
    EXPECT_FLOAT_EQ(out.argument, 0.0f);
}

TEST(ModeManagerComponent, CommitValidationSendFailureAndStatusAreVisible) {
    TimeService::initEpoch();
    ModeManagerComponent mode_mgr{"ModeManager", 49};
    InputPort<CommandPacket, 1> cmd_sink{};
    InputPort<EventPacket, 32> events{};
    InputPort<Serializer::ByteArray, 16> telemetry{};
    OutputPort<CommandPacket> prefill{};
    mode_mgr.command_out.connect(&cmd_sink);
    mode_mgr.event_out.connect(&events);
    mode_mgr.telemetry_out.connect(&telemetry);
    prefill.connect(&cmd_sink);
    mode_mgr.init();

    const uint32_t before = mode_mgr.getErrorCount();
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_COMMIT_RULE, 0.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_STAGE_TARGET_ID, 200.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_STAGE_ENABLE_OPCODE, 2.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_STAGE_DISABLE_OPCODE, 2.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_COMMIT_RULE, 0.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_REPORT_STATUS, 0.0f}));
    ASSERT_TRUE(mode_mgr.cmd_in.receive(CommandPacket{49u, ModeManagerComponent::OP_APPLY_MODE, 0.0f}));
    ASSERT_TRUE(prefill.send(CommandPacket{999u, 1u, 0.0f}));
    mode_mgr.step();

    bool saw_no_target = false;
    bool saw_send_fail = false;
    EventPacket evt{};
    while (events.tryConsume(evt)) {
        const std::string_view msg(evt.message.data());
        if (msg == "MODE_NO_TARGET") {
            saw_no_target = true;
        } else if (msg == "MODE_SEND_FAIL") {
            saw_send_fail = true;
        }
    }
    EXPECT_TRUE(saw_no_target);
    EXPECT_TRUE(saw_send_fail);
    EXPECT_GE(mode_mgr.getErrorCount(), before + 2u);

    size_t status_packets = 0U;
    bool saw_mode_value = false;
    Serializer::ByteArray raw{};
    while (telemetry.tryConsume(raw)) {
        const TelemetryPacket tlm = Serializer::unpackTelem(raw);
        if (tlm.component_id != 49u) {
            continue;
        }
        ++status_packets;
        if (tlm.data_payload == 2.0f) {
            saw_mode_value = true;
        }
    }
    EXPECT_GE(status_packets, 3u);
    EXPECT_TRUE(saw_mode_value);
}

// =============================================================================
// main
// =============================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
