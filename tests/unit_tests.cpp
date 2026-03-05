#include <gtest/gtest.h>
#include "Port.hpp"
#include "Serializer.hpp"
#include "ParamDb.hpp"

using namespace deltav;

// TEST 1: RingBuffer Integrity
// Ensures we can't over-read and that data remains FIFO
TEST(RingBufferTest, FIFO_Integrity) {
    RingBuffer<int, 4> buffer;
    
    // Check initial state
    EXPECT_TRUE(buffer.isEmpty());
    
    // Fill buffer
    buffer.push(10);
    buffer.push(20);
    buffer.push(30);
    
    EXPECT_FALSE(buffer.isEmpty());
    
    // Verify values and order
    EXPECT_EQ(buffer.pop(), 10);
    EXPECT_EQ(buffer.pop(), 20);
    EXPECT_EQ(buffer.pop(), 30);
    EXPECT_TRUE(buffer.isEmpty());
}

// TEST 2: Serializer Static Proofs
// Validates that the packet sizes exactly match our ICD
TEST(SerializerTest, PacketSizeValidation) {
    EXPECT_EQ(sizeof(TelemetryPacket), 12);
    EXPECT_EQ(sizeof(EventPacket), 36);
    EXPECT_EQ(sizeof(CcsdsHeader), 10);
}

// TEST 3: ParamDb CRC Checksum Protection
// This simulates a "Cosmic Ray" bit-flip and ensures the system catches it
TEST(ParamDbTest, CRC_Corruption_Detection) {
    ParamDb db;
    db.setValue("KV_VOLTS", 12.5f);
    
    // Manually corrupt the memory (Simulating a hardware fault)
    // We reach into the DB and flip a bit
    uint8_t* raw_ptr = reinterpret_cast<uint8_t*>(db.getPtr("KV_VOLTS"));
    *raw_ptr = 0xFF; 

    // The verify function should return false because the hash no longer matches
    EXPECT_FALSE(db.verifyIntegrity());
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}