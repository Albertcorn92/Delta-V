#pragma once
// =============================================================================
// Hal.hpp — DELTA-V Hardware Abstraction Layer
// =============================================================================
// DO-178C compliant pure virtual interfaces for physical hardware buses.
// =============================================================================
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>

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

class UartPort {
public:
    virtual ~UartPort() = default;
    virtual bool configure(uint32_t baud_rate) = 0;
    [[nodiscard]] virtual auto write(const uint8_t* data, size_t len) -> size_t = 0;
    [[nodiscard]] virtual auto read(uint8_t* data, size_t len) -> size_t = 0;
};

class PwmOutput {
public:
    virtual ~PwmOutput() = default;
    virtual bool setFrequency(uint8_t channel, uint32_t hz) = 0;
    virtual bool setDutyCycle(uint8_t channel, float duty_cycle_0_to_1) = 0;
    [[nodiscard]] virtual auto getDutyCycle(uint8_t channel) const -> float = 0;
};

// -----------------------------------------------------------------------------
// Host mock peripherals for SITL and unit testing
// -----------------------------------------------------------------------------
class MockSpi : public SpiBus {
public:
    bool transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t len) override {
        if ((tx_data == nullptr && rx_data == nullptr) || len == 0U) {
            return false;
        }
        if (rx_data == nullptr) {
            return true;
        }
        for (size_t i = 0; i < len; ++i) {
            rx_data[i] = (tx_data != nullptr) ? tx_data[i] : 0U;
        }
        return true;
    }
    void select() override {}
    void deselect() override {}
};

class MockUart : public UartPort {
public:
    static constexpr size_t RX_CAPACITY = 256U;

    bool configure(uint32_t baud_rate) override {
        configured_baud_ = baud_rate;
        return baud_rate > 0U;
    }

    [[nodiscard]] auto write(const uint8_t* data, size_t len) -> size_t override {
        if (data == nullptr || len == 0U) {
            return 0U;
        }
        last_write_len_ = len;
        return len;
    }

    [[nodiscard]] auto read(uint8_t* data, size_t len) -> size_t override {
        if (data == nullptr || len == 0U) {
            return 0U;
        }
        const size_t n = std::min(len, rx_size_);
        for (size_t i = 0; i < n; ++i) {
            data[i] = rx_buf_[i];
        }
        for (size_t i = n; i < rx_size_; ++i) {
            rx_buf_[i - n] = rx_buf_[i];
        }
        rx_size_ -= n;
        return n;
    }

    auto injectRx(const uint8_t* data, size_t len) -> size_t {
        if (data == nullptr || len == 0U) {
            return 0U;
        }
        const size_t room = RX_CAPACITY - rx_size_;
        const size_t n = (len < room) ? len : room;
        for (size_t i = 0; i < n; ++i) {
            rx_buf_[rx_size_ + i] = data[i];
        }
        rx_size_ += n;
        return n;
    }

    [[nodiscard]] auto configuredBaud() const -> uint32_t {
        return configured_baud_;
    }

    [[nodiscard]] auto lastWriteLen() const -> size_t {
        return last_write_len_;
    }

private:
    std::array<uint8_t, RX_CAPACITY> rx_buf_{};
    size_t rx_size_{0U};
    size_t last_write_len_{0U};
    uint32_t configured_baud_{0U};
};

class MockPwm : public PwmOutput {
public:
    static constexpr size_t CHANNEL_COUNT = 8U;

    bool setFrequency(uint8_t channel, uint32_t hz) override {
        if (channel >= CHANNEL_COUNT || hz == 0U) {
            return false;
        }
        frequency_[channel] = hz;
        return true;
    }

    bool setDutyCycle(uint8_t channel, float duty_cycle_0_to_1) override {
        if (channel >= CHANNEL_COUNT) {
            return false;
        }
        float bounded = duty_cycle_0_to_1;
        if (bounded < 0.0F) {
            bounded = 0.0F;
        }
        if (bounded > 1.0F) {
            bounded = 1.0F;
        }
        duty_[channel] = bounded;
        return true;
    }

    [[nodiscard]] auto getDutyCycle(uint8_t channel) const -> float override {
        if (channel >= CHANNEL_COUNT) {
            return 0.0F;
        }
        return duty_[channel];
    }

private:
    std::array<uint32_t, CHANNEL_COUNT> frequency_{};
    std::array<float, CHANNEL_COUNT> duty_{};
};

} // namespace hal
} // namespace deltav
