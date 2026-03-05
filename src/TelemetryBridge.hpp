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

    // Input Ports (Downlink)
    InputPort<Serializer::ByteArray> telem_in;
    InputPort<EventPacket> event_in; 

    // Output Port (Uplink)
    OutputPort<CommandPacket> command_out;

    void init() override {
        sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        fcntl(sock_fd, F_SETFL, O_NONBLOCK);

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(9001); 
        server_addr.sin_addr.s_addr = INADDR_ANY;

        bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
        std::cout << "[" << getName() << "] Ground Link Active (Multiplexed)" << std::endl;
    }

    void step() override {
        struct sockaddr_in dest_addr = server_addr;
        dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        // 1. Downlink Telemetry (12 bytes)
        if (telem_in.hasNew()) {
            auto data = telem_in.consume();
            sendto(sock_fd, data.data(), data.size(), 0,
                   (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }

        // 2. Downlink Events (36 bytes)
        if (event_in.hasNew()) {
            auto evt = event_in.consume();
            sendto(sock_fd, &evt, sizeof(evt), 0,
                   (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }

        // 3. Uplink Commands
        CommandPacket cmd;
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int len = recvfrom(sock_fd, &cmd, sizeof(cmd), 0, 
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