#pragma once
#include "Component.hpp"
#include "Port.hpp"
#include "Serializer.hpp"
#include "Types.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h> 
#include <iostream>

namespace deltav {

class TelemetryBridge : public Component {
public:
    TelemetryBridge(std::string_view name, uint32_t id) : Component(name, id) {}

    InputPort<Serializer::ByteArray> telem_in;
    InputPort<EventPacket> event_in; 
    OutputPort<CommandPacket> command_out;

    void init() override {
        sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        // Ensure non-blocking so the flight loop doesn't hang waiting for a command
        fcntl(sock_fd, F_SETFL, O_NONBLOCK);

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(9002); 
        server_addr.sin_addr.s_addr = INADDR_ANY;

        bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
        std::cout << "[" << getName() << "] Bridge Online. Downlink: 9001, Uplink: 9002" << std::endl;
    }

    void step() override {
        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(9001);
        dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        // 1. Downlink Telemetry (Strict 12 bytes)
        if (telem_in.hasNew()) {
            auto data = telem_in.consume();
            sendto(sock_fd, data.data(), data.size(), 0,
                   (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }

        // 2. Downlink Events (Strict 36 bytes)
        if (event_in.hasNew()) {
            auto evt = event_in.consume();
            // Use sizeof(EventPacket) which is now 36 due to packing
            sendto(sock_fd, &evt, sizeof(EventPacket), 0,
                   (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }

        // 3. Uplink Commands
        CommandPacket cmd;
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int len = recvfrom(sock_fd, &cmd, sizeof(CommandPacket), 0, 
                           (struct sockaddr*)&client_addr, &addr_len);

        if (len == sizeof(CommandPacket)) {
            command_out.send(cmd); 
        }
    }

private:
    int sock_fd = -1;
    struct sockaddr_in server_addr;
};

} // namespace deltav