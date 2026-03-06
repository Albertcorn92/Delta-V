#pragma once
// =============================================================================
// ImuComponent.hpp — DELTA-V Inertial Measurement Unit
// =============================================================================
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "TimeService.hpp"
#include "Types.hpp"
#include "Hal.hpp"
#include <iostream>

namespace deltav {

class ImuComponent : public Component {
public:
    InputPort<CommandPacket>          cmd_in;
    OutputPort<Serializer::ByteArray> telemetry_out;
    OutputPort<EventPacket>           event_out;

    // Standard constructor + HAL Injection
    ImuComponent(std::string_view comp_name, uint32_t comp_id, hal::I2cBus& i2c_bus)
        : Component(comp_name, comp_id), bus(i2c_bus) {}

    void init() override {
        // Wake up MPU6050 (Dev Addr 0x68, Power Mgmt Reg 0x6B, Write 0x00)
        uint8_t wake_cmd = 0x00;
        if (!bus.write(0x68, 0x6B, &wake_cmd, 1)) {
            recordError();
            event_out.send(EventPacket::create(Severity::CRITICAL, getId(), "IMU I2C Wake Failed"));
            std::cerr << "[" << getName() << "] FATAL: Hardware IMU missing.\n";
        } else {
            std::cout << "[" << getName() << "] IMU Initialized via HAL.\n";
        }
    }

    void step() override {
        CommandPacket cmd{};
        while (cmd_in.tryConsume(cmd)) { handleCommand(cmd); }

        // Read 2 bytes of raw X-axis acceleration from register 0x3B
        uint8_t buffer[2] = {0, 0};
        if (!bus.read(0x68, 0x3B, buffer, 2)) {
            recordError();
            return;
        }

        int16_t accel_x = static_cast<int16_t>((buffer[0] << 8) | buffer[1]);
        
        TelemetryPacket p{ TimeService::getMET(), getId(), static_cast<float>(accel_x) };
        telemetry_out.send(Serializer::pack(p));
    }

private:
    hal::I2cBus& bus;

    void handleCommand(const CommandPacket& cmd) {
        recordError();
        event_out.send(EventPacket::create(Severity::WARNING, getId(), "IMU: Unknown opcode"));
    }
};

} // namespace deltav