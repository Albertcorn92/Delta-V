#pragma once
// =============================================================================
// I2cDriver.hpp — Cross-Platform I2C Implementation
// =============================================================================
#include "Hal.hpp"

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
// -----------------------------------------------------------------------------
// ESP32-S3 PHYSICAL HARDWARE DRIVER
// -----------------------------------------------------------------------------
#include <driver/i2c.h>
#include <driver/gpio.h>
#include <esp_idf_version.h>

namespace deltav { namespace hal {
class Esp32I2c : public I2cBus {
public:
    Esp32I2c(i2c_port_t port = I2C_NUM_0,
             gpio_num_t sda_pin = GPIO_NUM_8,
             gpio_num_t scl_pin = GPIO_NUM_9,
             uint32_t clock_hz = 400000U)
        : i2c_port(port), sda_pin_(sda_pin), scl_pin_(scl_pin), clock_hz_(clock_hz) {
        initBus();
    }

    ~Esp32I2c() override {
        if (owns_driver_) {
            (void)i2c_driver_delete(i2c_port);
        }
    }

    bool read(uint8_t dev_addr, uint8_t reg_addr, uint8_t* data, size_t len) override {
        if (!ready_ || data == nullptr || len == 0U) {
            return false;
        }
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
        if (!ready_ || data == nullptr || len == 0U) {
            return false;
        }
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
    void initBus() {
        i2c_config_t cfg{};
        cfg.mode = I2C_MODE_MASTER;
        cfg.sda_io_num = sda_pin_;
        cfg.scl_io_num = scl_pin_;
        cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
        cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
        cfg.master.clk_speed = clock_hz_;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
        cfg.clk_flags = 0;
#endif

        const esp_err_t cfg_ret = i2c_param_config(i2c_port, &cfg);
        if (cfg_ret != ESP_OK) {
            ready_ = false;
            return;
        }

        const esp_err_t install_ret = i2c_driver_install(i2c_port, cfg.mode, 0, 0, 0);
        if (install_ret == ESP_OK) {
            ready_ = true;
            owns_driver_ = true;
            return;
        }
        if (install_ret == ESP_ERR_INVALID_STATE) {
            // Driver already installed by board support code.
            ready_ = true;
            owns_driver_ = false;
            return;
        }
        ready_ = false;
    }

    i2c_port_t i2c_port;
    gpio_num_t sda_pin_;
    gpio_num_t scl_pin_;
    uint32_t   clock_hz_;
    bool       ready_{false};
    bool       owns_driver_{false};
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
            const float met_seconds = static_cast<float>(TimeService::getMET()) / 1000.0f;
            const float roll = std::sin(met_seconds) * 1000.0f;
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
