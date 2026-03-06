#pragma once
// =============================================================================
// Hal.hpp — DELTA-V Hardware Abstraction Layer
// =============================================================================
// DO-178C compliant pure virtual interfaces for physical hardware buses.
// =============================================================================
#include <cstdint>
#include <cstddef>

namespace deltav {
namespace hal {

class I2cBus {
public:
    virtual ~I2cBus() = default;
    
    // Read 'len' bytes from 'reg_addr' at I2C device 'dev_addr'
    virtual bool read(uint8_t dev_addr, uint8_t reg_addr, uint8_t* data, size_t len) = 0;
    
    // Write 'len' bytes to 'reg_addr' at I2C device 'dev_addr'
    virtual bool write(uint8_t dev_addr, uint8_t reg_addr, const uint8_t* data, size_t len) = 0;
};

class SpiBus {
public:
    virtual ~SpiBus() = default;
    virtual bool transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t len) = 0;
    virtual void select() = 0;
    virtual void deselect() = 0;
};

} // namespace hal
} // namespace deltav