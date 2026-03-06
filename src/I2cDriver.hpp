#pragma once
// =============================================================================
// I2cDriver.hpp — Cross-Platform I2C Implementation
// =============================================================================
#include "Hal.hpp"
#include <iostream>

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
// -----------------------------------------------------------------------------
// ESP32-S3 PHYSICAL HARDWARE DRIVER
// -----------------------------------------------------------------------------
#include <driver/i2c.h>

namespace deltav { namespace hal {
class Esp32I2c : public I2cBus {
public:
    Esp32I2c(i2c_port_t port = I2C_NUM_0) : i2c_port(port) {}

    bool read(uint8_t dev_addr, uint8_t reg_addr, uint8_t* data, size_t len) override {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, reg_addr, true);
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);
        if (len > 1) {
            i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
        }
        i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);
        return (ret == ESP_OK);
    }

    bool write(uint8_t dev_addr, uint8_t reg_addr, const uint8_t* data, size_t len) override {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, reg_addr, true);
        i2c_master_write(cmd, data, len, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);
        return (ret == ESP_OK);
    }
private:
    i2c_port_t i2c_port;
};
}} // namespace deltav::hal

#else
// -----------------------------------------------------------------------------
// macOS / LINUX SITL MOCK DRIVER
// -----------------------------------------------------------------------------
#include "TimeService.hpp"
#include <cmath>

namespace deltav { namespace hal {
class MockI2c : public I2cBus {
public:
    bool read(uint8_t dev_addr, uint8_t reg_addr, uint8_t* data, size_t len) override {
        if (len >= 2) {
            // Simulate IMU rolling based on time
            float roll = std::sin(TimeService::getMET() / 1000.0f) * 1000.0f;
            int16_t raw_accel = static_cast<int16_t>(roll);
            data[0] = static_cast<uint8_t>((raw_accel >> 8) & 0xFF);
            data[1] = static_cast<uint8_t>(raw_accel & 0xFF);
        }
        return true;
    }

    bool write(uint8_t /*dev_addr*/, uint8_t /*reg_addr*/, const uint8_t* /*data*/, size_t /*len*/) override {
        return true; // Pretend hardware wake-up succeeded
    }
};
}} // namespace deltav::hal
#endif