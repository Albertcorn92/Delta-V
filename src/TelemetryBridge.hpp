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
#include <array>
#include <cstring> 

namespace deltav {

class TelemetryBridge : public ActiveComponent {
public:
    // 🚀 ALIGNED TO GDS: Ports 9001 (Downlink) and 9002 (Uplink)
    static constexpr uint16_t GDS_DOWNLINK_PORT = 9001; 
    static constexpr uint16_t GDS_UPLINK_PORT = 9002;

    TelemetryBridge(std::string_view name, uint32_t id) : ActiveComponent(name, id, 20) {}

    // Must match the Serializer output type from TelemHub
    InputPort<Serializer::ByteArray> telem_in;
    InputPort<EventPacket> event_in; 
    OutputPort<CommandPacket> command_out;

    void init() override {
        sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_fd < 0) {
            std::cerr << "[" << getName() << "] FATAL: Socket creation failed!" << std::endl;
            return;
        }

        // Set non-blocking to prevent the 1Hz loop from hanging
        fcntl(sock_fd, F_SETFL, O_NONBLOCK);

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(GDS_UPLINK_PORT); 
        server_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "[" << getName() << "] WARN: Could not bind Uplink port 9002" << std::endl;
        }

        std::cout << "[" << getName() << "] Bridge Online. Downlink: " << GDS_DOWNLINK_PORT 
                  << ", Uplink: " << GDS_UPLINK_PORT << " [CCSDS FRAMING ACTIVE]" << std::endl;
    }

    void step() override {
        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(GDS_DOWNLINK_PORT);
        dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        static uint16_t telem_seq = 0;
        static uint16_t event_seq = 0;

        // 1. TELEMETRY DOWNLINK
        if (telem_in.hasNew()) {
            auto data = telem_in.consume();
            CcsdsHeader header = {0xDEADBEEF, 10, telem_seq++, static_cast<uint16_t>(data.size())};
            
            // Total packet size: 10 (header) + 12 (payload) = 22 bytes
            std::array<uint8_t, 22> frame;
            std::memcpy(frame.data(), &header, sizeof(CcsdsHeader));
            std::memcpy(frame.data() + sizeof(CcsdsHeader), data.data(), data.size());
            
            ssize_t sent = sendto(sock_fd, frame.data(), frame.size(), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
            
            if (sent > 0) {
                // 🚀 DEBUG PRINT: You should see this in your flight_software terminal
                std::cout << "[RadioLink] Sent Telemetry Packet (Seq: " << telem_seq-1 << ")" << std::endl;
            } else {
                std::perror("[RadioLink] Sendto Failed");
            }
        }

        // 2. EVENT DOWNLINK
        if (event_in.hasNew()) {
            auto evt = event_in.consume();
            CcsdsHeader header = {0xDEADBEEF, 20, event_seq++, sizeof(EventPacket)};
            
            std::array<uint8_t, sizeof(CcsdsHeader) + sizeof(EventPacket)> frame;
            std::memcpy(frame.data(), &header, sizeof(CcsdsHeader));
            std::memcpy(frame.data() + sizeof(CcsdsHeader), &evt, sizeof(EventPacket));
            
            sendto(sock_fd, frame.data(), frame.size(), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
            std::cout << "[RadioLink] Sent Event Packet" << std::endl;
        }

        // 3. COMMAND UPLINK
        std::array<uint8_t, 64> recv_buf;
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int len = recvfrom(sock_fd, recv_buf.data(), recv_buf.size(), 0, (struct sockaddr*)&client_addr, &addr_len);

        if (len >= (int)sizeof(CcsdsHeader)) {
            CcsdsHeader* hdr = reinterpret_cast<CcsdsHeader*>(recv_buf.data());
            if (hdr->sync_word == 0xDEADBEEF && hdr->apid == 30) {
                CommandPacket* cmd = reinterpret_cast<CommandPacket*>(recv_buf.data() + sizeof(CcsdsHeader));
                command_out.send(*cmd); 
                std::cout << "[RadioLink] Command Received! Routing to CmdHub..." << std::endl;
            }
        }
    }

private:
    int sock_fd = -1;
    struct sockaddr_in server_addr;
};

} // namespace deltav