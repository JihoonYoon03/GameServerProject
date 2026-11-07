#pragma once
#include <cstdint>
#include <memory>

class NetworkServer;
class RoomManager;

class Framework
{
public:
    Framework();
    ~Framework();
    bool StartServer(std::uint16_t tcpPort, std::uint16_t udpPort);
    void StopServer();

private:
    std::unique_ptr<NetworkServer> network_;
    std::unique_ptr<RoomManager> roomManager_;
};
